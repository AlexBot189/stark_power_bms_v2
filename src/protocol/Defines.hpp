#pragma once

#include <math.h>
#include <string>
#include <vector>
#include <termios.h>

namespace stark_power_manager
{
constexpr const int MAX_RX_LEN = 4096;  ///< 4Kb
constexpr const int module_mcu = 0x04;  ///< 1Kb

/**< 电机配置类参数 */
constexpr const double MOTOR_RATIO = 61.92;   ///< 减速比
constexpr const double ENCODE_RESOLUTION = 20;  ///< 光栅分辨率

constexpr const double WHEEL_DIAMETER = 0.0703;  ///< 扫地机轮直径0.7, 单位(m)
constexpr const double WHEEL_GUAGE = 0.23;      ///< 扫地机两个主动轮轮间距,单位(m)
constexpr const float RAD = 180.0 / 3.1415926;

///< 计算每一个脉冲对应多少米单位值: 作为基础计算量
constexpr const double ONE_PULSE_TO_METER_RATIO = (WHEEL_DIAMETER * M_PI) / (MOTOR_RATIO * ENCODE_RESOLUTION);
constexpr const double ONE_PULSE_TO_RADIAN_RATIO = WHEEL_GUAGE / ONE_PULSE_TO_METER_RATIO;

constexpr const uint8_t CMD_CONFIG = 0;
constexpr const uint8_t CMD_CALIBRATION_IR_SENSOR = 1;
constexpr const uint8_t CMD_CALIBRATION_TOF = 2;
constexpr const uint8_t CMD_RESET_CALIBRATION = 3;

constexpr const uint8_t SUB_MODE_OFF = 0;
constexpr const uint8_t SUB_MODE_ON = 1;

struct BumpMsg
{
    int leftBump : 1;
    int frontLeftBump : 1;
    int frontRightBump : 1;
    int rightBump : 1;
    int ldsLeftBump : 1;
    int ldsRightBump : 1;
};

enum class McuState
{
    MCU_IDLE = 0x00,
    MCU_OK = 0x01,
    MCU_EXCEPT = 0x02,
    MCU_ERROR = 0x03
};

struct UartOption
{
    std::string strTty{ "/dev/ttyS11" };
    speed_t iBaudRate{ B115200 };
};

struct DeviceOption
{
    std::string strType{ "UART" };
    UartOption uartOption;
};
}  // namespace stark_power_manager
