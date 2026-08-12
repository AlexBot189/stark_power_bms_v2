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
}  // namespace stark_power_manager
