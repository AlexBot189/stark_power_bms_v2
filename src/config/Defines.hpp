#pragma once

#include <cstdint>
#include <string>
#include <termios.h>

namespace stark_power_manager
{
struct CoreOption
{
    std::string strTty{ "/dev/ttyS11" };
    // speed_t iBaudRate{ B115200 };
    int iBaudRate{ 115200 };
};

struct BatteryOption
{
    std::string strTty{ "/dev/ttyS2" };
    int iBaudRate{ 115200 };
    uint32_t poll_period_ms{ 500 };
    uint32_t soc_period_ms{ 2000 };
    uint32_t fault_period_ms{ 30000 };
    bool enable_voltage{ true };
    bool enable_current_temp{ true };
    bool enable_soc_capacity{ true };
    bool enable_fault{ true };
    bool enable_charge_current{ false };
};
struct WebOption {
    bool enabled{ false };
    uint16_t port{ 50080 };
};

/* IP2366 快充芯片配置 (对应 IP2366Source::Config) */
struct Ip2366Option {
    std::string i2c_dev{ "/dev/i2c-2" };
    int         i2c_addr{ 0xEA };
    std::string int_gpio_chip{ "gpiochip0" };
    int         int_gpio_line{ 39 };       /* GPIO0_C7 */
    std::string charge_en_gpio_chip{ "gpiochip2" };
    int         charge_en_gpio_line{ 4 };  /* GPIO2_A4 */
    int         pdo_select{ 5 };           /* 5=20V */
    int         charge_voltage_mv{ 4200 }; /* 单节满充 mV */
    int         charge_current_ma{ 3000 };
    int         trickle_current_ma{ 200 };
    int         stop_current_ma{ 100 };
    int         int_poll_interval_ms{ 100 };
};

/* 充电管理配置 (对应 PowerManagerConfig) */
struct PowerManagerOption {
    int  adapter_debounce_ms{ 200 };
    int  fault_debounce_ms{ 500 };
    int  recovery_debounce_ms{ 2000 };
    int  full_soc_percent{ 100 };
    int  full_current_ma{ 200 };
    int  recharge_hyst_mv{ 200 };
    int  cc_timeout_min{ 240 };
    int  total_timeout_min{ 480 };
    int  max_temp_c{ 85 };
    int  resume_temp_c{ 75 };
    int  pd_timeout_ms{ 10000 };
    bool cross_verify_charge{ true };
    int  vbus_present_mv{ 4000 };
};
}  // namespace stark_power_manager
