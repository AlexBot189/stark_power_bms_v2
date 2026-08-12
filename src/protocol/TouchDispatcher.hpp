#ifndef TOUCH_PUBLISHER_HPP
#define TOUCH_PUBLISHER_HPP

#include <ros/ros.h>
#include <deebot_msgs/TouchData.h>  // 包含自定义消息类型的头文件
#include <memory>
#include "UartRecive.hpp"  // 包含 SerialReceiver 的定义

namespace stark_power_manager {

class TouchPublisher {
public:
    /**
     * @brief 构造函数
     * @param nh ROS NodeHandle
     * @param port 串口设备（例如 "/dev/ttyS4"）
     * @param topic 发布的 ROS 话题名称
     * @param chunkSize 每条消息拆分的字节数，默认 4
     */
    TouchPublisher(const ros::NodeHandle& nh, const std::string& port, const std::string& topic, size_t chunkSize = 4)
        : nh_(nh), chunkSize_(chunkSize)
    {
        // 初始化发布者，发布自定义的 TouchData 消息类型
        publisher_ = nh_.advertise<deebot_msgs::TouchData>(topic, 10);
        // 初始化串口接收器
        receiver_ = std::make_shared<SerialReceiver>(port.c_str(),
            [this](const uint8_t* data, size_t len) {
                this->handleReceivedData(data, len);
            }
        );
    }

    /// 启动串口接收
    bool start() {
        if (receiver_) {
            return receiver_->start();
        }
        return false;
    }

    /// 停止串口接收
    void stop() {
        if (receiver_) {
            receiver_->stop();
        }
    }

private:
#if 0
    /// 内部回调：对接收到的每个4字节数据块提取指定字段并发布
    void handleReceivedData(const uint8_t* data, size_t len) {
        // 每4个字节为一个数据块，若数据不足4个字节则忽略
        for (size_t i = 0; i + 3 < len; i += chunkSize_) {
            deebot_msgs::TouchData msg;
            msg.touch_part  = data[i];       // 第1个字节：触摸部位
            msg.touch_mode  = data[i + 2];   // 第3个字节：触摸模式
            msg.touch_force = data[i + 3];   // 第4个字节：触摸力度
            publisher_.publish(msg);
        }
    }
#endif
    void handleReceivedData(const uint8_t* data, size_t len) {
	    // 每4个字节为一个数据块，若数据不足4个字节则忽略
	    for (size_t i = 0; i + 3 < len; i += chunkSize_) {
		    // 先把四个字节存到局部变量，便于打印
		    uint8_t b0 = data[i];
		    uint8_t b1 = data[i + 1];
		    uint8_t b2 = data[i + 2];
		    uint8_t b3 = data[i + 3];

		    // 打印出这4个字节（十六进制格式），方便调试
		    ROS_INFO("Received bytes: [%02X, %02X, %02X, %02X]", b0, b1, b2, b3);

		    // 构造并发布 TouchData 消息
		    deebot_msgs::TouchData msg;
		    msg.touch_part  = b0;  // 第1个字节：触摸部位
		 //   msg.touch_status  = b1;  // 第2个字节：触摸状态，0：触摸，1：结束触摸
		    msg.touch_mode  = b2;  // 第3个字节：触摸模式
		    msg.touch_force = b3;  // 第4个字节：触摸力度
		    publisher_.publish(msg);
	    }
    }

    ros::NodeHandle nh_;
    ros::Publisher publisher_;
    std::shared_ptr<SerialReceiver> receiver_;
    size_t chunkSize_;
};

} // namespace stark_power_manager

#endif // TOUCH_PUBLISHER_HPP

