#include "TouchMonitorAdapter.hpp"
#include <iostream>
#include <chrono>
#include <cstdint>

namespace stark_power_manager
{

TouchMonitorAdapter::TouchMonitorAdapter(const std::string& gpioChipPath,
                                         int headLine,
                                         int chinLine,
                                         int LeftEarLine,
                                         int RightEarLine,
                                         std::shared_ptr<ros::NodeHandle> nh,
					 std::shared_ptr<UartDispatcher> uartDispatcher)
    : m_gpioChipPath(gpioChipPath)
    , m_headLine(headLine)
    , m_chinLine(chinLine)
    , m_LeftEarLine(LeftEarLine)
    , m_RightEarLine(RightEarLine)
    , m_rosNh(nh)
    , m_uartDispatcher(uartDispatcher)
    , m_running(false)
    , m_lastDynamicPose(0)
    , m_lastPoseTimestampMs(0)
    , m_lastTouchPressTsMs(0)
    , m_lastImuImpactTsMs(0)
{
    m_gpioMonitor = std::make_shared<GPIOMonitor>(m_gpioChipPath);

    m_touchStatusPublisher = m_rosNh->advertise<deebot_msgs::TouchData>("/petrobot/touch_data", 10);

    m_monitorThread = std::thread(&TouchMonitorAdapter::StartNode, this);
}

TouchMonitorAdapter::~TouchMonitorAdapter()
{
    m_running = false;
    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
    if (m_gpioMonitor) {
        m_gpioMonitor->stop();
    }
    ECO_INFO("Stopped and cleaned up.");
}

int TouchMonitorAdapter::StartNode()
{
    m_running = true;

    auto cb = [this](int line, int event_type) {
        this->HandleTouchEvent(line, event_type);
    };

    m_gpioMonitor->watch_line(m_headLine, cb);
    m_gpioMonitor->watch_line(m_chinLine, cb);
    m_gpioMonitor->watch_line(m_LeftEarLine, cb);
    m_gpioMonitor->watch_line(m_RightEarLine, cb);

    ECO_INFO("Monitoring started");
    ros::Rate r(10);
    while (ros::ok() && m_running) {
        ros::spinOnce();
        r.sleep();
    }

    return 0;
}

void TouchMonitorAdapter::UpdateDynamicPose(int pose, int64_t ts)
{
    if (pose == 1 || pose == 5) {
        // 记录IMU冲击时间
        m_lastImuImpactTsMs.store(ts);

        // 检查是否有最近触摸按下
        int64_t touchTs = m_lastTouchPressTsMs.load();
        constexpr int64_t PAT_WINDOW_MS = 200;

        if (touchTs > 0 && (ts - touchTs <= PAT_WINDOW_MS)) {
            // 双向融合：触摸先发生
            TriggerPatFromIMU(ts);
        }
    }

    m_lastDynamicPose.store(pose);
}

/*
 * 从IMU触发拍打事件
 * 会发布ROS消息和通知MCU
 */
void TouchMonitorAdapter::TriggerPatFromIMU(int64_t now)
{
    // 避免重复触发
    m_lastTouchPressTsMs.store(0);
    m_lastImuImpactTsMs.store(0);

    // 选择拍打部位，这里以头和两耳为例
    std::vector<uint8_t> patParts = {0, 3, 4}; // HEAD, LEFT EAR, RIGHT EAR
    for (auto part : patParts) {
        deebot_msgs::TouchData GpioTouchData;
        GpioTouchData.touch_part  = part;
        GpioTouchData.touch_mode  = 1; // ACTION_PAT
        GpioTouchData.touch_force = 1; // ACTION_ON

        m_touchStatusPublisher.publish(GpioTouchData);

        if (m_uartDispatcher) {
            m_uartDispatcher->SendTouchEventToMCU(part, 1);
        }
        ECO_INFO_NEW("PAT detected from IMU, part={}", part);
    }
}

void TouchMonitorAdapter::HandleTouchEvent(int line_num, int event_type)
{
    constexpr int64_t TOUCH_FILTER_MS = 100;
    constexpr int64_t PAT_WINDOW_MS   = 200;

    bool isPress = (event_type != GPIOD_LINE_EVENT_RISING_EDGE);

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();

    if (isPress) {
        std::lock_guard<std::mutex> lock(m_touchTsMutex);
        auto it = m_lastTouchTsMs.find(line_num);
        if (it != m_lastTouchTsMs.end() &&
            now - it->second < TOUCH_FILTER_MS) {
            return;
        }
        m_lastTouchTsMs[line_num] = now;
    }

    const uint8_t POSITION_HEAD = 0;
    const uint8_t POSITION_CHIN = 2;
    const uint8_t POSITION_LEFTEAR = 3;
    const uint8_t POSITION_RIGHTEAR = 4;
    const uint8_t ACTION_TOUCH  = 3;
    const uint8_t ACTION_PAT   = 1;
    const uint8_t ACTION_ON     = 1;
    const uint8_t ACTION_OFF    = 0;

    deebot_msgs::TouchData GpioTouchData;
    
    if (line_num == m_headLine) {
        GpioTouchData.touch_part = POSITION_HEAD;
    	ECO_INFO_NEW("[HEAD]");
    } else if (line_num == m_chinLine) {
        GpioTouchData.touch_part = POSITION_CHIN;
    	ECO_INFO_NEW("[CHIN]");
    } else if (line_num == m_LeftEarLine) {
        GpioTouchData.touch_part = POSITION_LEFTEAR;
    	ECO_INFO_NEW("[LEFTEAR]");
    } else if (line_num == m_RightEarLine) {
        GpioTouchData.touch_part = POSITION_RIGHTEAR;
    	ECO_INFO_NEW("[RIGHTEAR]");
    } else {
    	ECO_INFO_NEW("[UNKNOWN] Line {}", line_num);
        return;
    }

    GpioTouchData.touch_mode = ACTION_TOUCH;
    GpioTouchData.touch_force = isPress ? ACTION_ON : ACTION_OFF;

// 双向时间窗口判定拍打
    if (isPress &&
        (GpioTouchData.touch_part == POSITION_HEAD ||
         GpioTouchData.touch_part == POSITION_LEFTEAR ||
         GpioTouchData.touch_part == POSITION_RIGHTEAR)) {

        int64_t imuTs = m_lastImuImpactTsMs.load();

        if (imuTs > 0 && (now - imuTs <= PAT_WINDOW_MS)) {
            GpioTouchData.touch_mode = ACTION_PAT;

            // 消费该IMU事件，避免重复
            m_lastImuImpactTsMs.store(0);

            ECO_INFO_NEW("PAT detected from TouchEvent, part={}, delta={}ms",
                         GpioTouchData.touch_part, now - imuTs);
        }

        // 记录触摸按下时间，用于IMU事件融合
        m_lastTouchPressTsMs.store(now);
    }

    m_touchStatusPublisher.publish(GpioTouchData);

    std::string event_str = (GpioTouchData.touch_force == ACTION_ON) ? "PRESSED" : "RELEASED";
    ECO_INFO_NEW("Touch {}", event_str);

    if (m_uartDispatcher) {
        m_uartDispatcher->SendTouchEventToMCU(GpioTouchData.touch_part, GpioTouchData.touch_force);
    } else {
        ECO_ERROR("UART dispatcher not initialized");
    }
}

}

