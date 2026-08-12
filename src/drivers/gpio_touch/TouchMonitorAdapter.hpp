#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <ros/ros.h>
#include "utility/McuUartType.hpp"
#include "GPIOMonitor.hpp"
#include "deebot_msgs/TouchData.h"
#include <gpiod.h>
#include <log_helper/LogHelper.h>
#include "protocol/UartDispatcher.hpp"
#include <unordered_map>
#include <mutex>

namespace stark_power_manager
{

class TouchMonitorAdapter
{
public:
    TouchMonitorAdapter(const std::string& gpioChipPath,
                        int headLine,
                        int chinLine,
                        int LeftEarLine,
                        int RightEarLine,
                        std::shared_ptr<ros::NodeHandle> nh,
                        std::shared_ptr<UartDispatcher> uartDispatcher);

    ~TouchMonitorAdapter();

    int StartNode();  // 启动线程内部执行逻辑

    void SendTouchEventToMCU(uint8_t part, uint8_t force);
    void UpdateDynamicPose(int pose, int64_t ts);

private:
    void HandleTouchEvent(int line_num, int event_type);

    // ---------------- 新增成员 ----------------
    std::atomic<int64_t> m_lastTouchPressTsMs{0};  // 最近一次触摸按下时间
    std::atomic<int64_t> m_lastImuImpactTsMs{0};   // 最近一次IMU拍打事件时间
    void TriggerPatFromIMU(int64_t now);           // 从IMU触发拍打事件
    // -----------------------------------------

    std::string m_gpioChipPath;
    int m_headLine;
    int m_chinLine;
    int m_LeftEarLine;
    int m_RightEarLine;

    std::shared_ptr<ros::NodeHandle> m_rosNh;
    std::shared_ptr<GPIOMonitor> m_gpioMonitor;
    ros::Publisher m_touchStatusPublisher;
    std::shared_ptr<UartDispatcher> m_uartDispatcher; 

    std::thread m_monitorThread;
    std::atomic<bool> m_running;

    std::atomic<int> m_lastDynamicPose{0};
    std::atomic<int64_t> m_lastPoseTimestampMs{0};

    std::unordered_map<int, int64_t> m_lastTouchTsMs;
    std::mutex m_touchTsMutex;

    struct GpioTouchControl
    {
        uint8_t touch_part;
        uint8_t touch_status;
        uint8_t touch_mode;
        uint8_t touch_force;
    };
};

} // namespace stark_power_manager
