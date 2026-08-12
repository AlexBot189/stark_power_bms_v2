#pragma once

namespace stark_power_manager
{
enum class ListenerType
{
    ROS   = 0,
    WEB  = 1,
    lIDAR = 2,
    PREDICTION = 3,
    MAX_SIZE
};

enum class MsgType
{
    IMU   = 0,
    ODOM  = 1,
    lIDAR = 2,
    RTC   = 3,
    MAX_SIZE
};
}  // namespace stark_power_manager