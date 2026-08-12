#pragma once

#include <string>
#include <termios.h>

namespace stark_power_manager
{
struct LidarOption
{
    std::string lidarVendor = "LD";
    std::string strProductName{ "LDLiDAR_LD14P" };
    std::string strTty{ "/dev/ttyS2" };
    int iBaudRate{ 230400 };
    int saveData{0};

    /**< 坐标系偏移量 */
    float xOffset{ 0.0 };
    float yOffset{ 0.0 };
    float thetaOffset{ 0.0 };
};

struct LaserScanSetting
{
    std::string frame_id{ "base_laser" };
    bool laser_scan_dir{ false }; // true：逆时针，false：顺时针
    bool enable_angle_crop_func{ false };
    double angle_crop_min{ 0.0 };
    double angle_crop_max{ 0.0 };
};
struct LineLaserOption
{
    std::string vendor = "HC";
    std::string strProductName{ "L2D3" };
    std::string strTty{ "/dev/ttyS1" };
    int iBaudRate{ 921600 };
    int saveData{0};
};
}  // namespace stark_power_manager
