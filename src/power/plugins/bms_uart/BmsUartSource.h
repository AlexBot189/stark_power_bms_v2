/*
 * BmsUartSource.h — 电池包 BMS 数据源适配器
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 把 BatteryDispatcher (BMS UART 通讯层) 包装成 IPowerSource 接口,
 * 使 PowerManager 能通过标准 PowerProp 属性访问 BMS 数据。
 *
 * 职责:
 *   - getProp: BatteryStatus/BatteryFault -> PowerProp 属性映射
 *   - setProp: PowerProp 控制属性 -> ControlMOS (0x2007)
 *
 * 不持有 BatteryDispatcher 的所有权, 只引用 (shared_ptr)。
 * 线程安全: getProp/setProp 内部通过 BatteryDispatcher 自身的锁保证。
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "power/IPowerSource.h"
#include "battery/BatteryDispatcher.h"
#include "battery/BatteryTypes.h"

namespace stark_power_manager {

/* BMS 数据源配置 */
struct BmsUartConfig {
    int cell_count   = 6;     /* 电芯串联数 (6S) */
    int cell_full_mv = 4200;  /* 单节满充电压 mV */
};

class BmsUartSource : public IPowerSource {
public:
    explicit BmsUartSource(std::shared_ptr<BatteryDispatcher> dispatcher,
                           const BmsUartConfig& cfg = {});
    ~BmsUartSource() override;

    /* 禁止拷贝 */
    BmsUartSource(const BmsUartSource&) = delete;
    BmsUartSource& operator=(const BmsUartSource&) = delete;

    /* IPowerSource 接口 */
    const char* name() const override { return "battery_bms"; }
    const char* type() const override { return "battery"; }

    std::vector<PowerProp> supportedProps() const override;
    bool getProp(PowerProp prop, PowerValue& out) override;
    bool setProp(PowerProp prop, const PowerValue& val) override;
    void subscribe(ChangeCallback cb) override;

private:
    /* 任意故障位判定 (sys/chg/dischg 三类原始字节) */
    static bool hasFault(const BatteryFault& f);

    std::shared_ptr<BatteryDispatcher> m_dispatcher;
    BmsUartConfig m_cfg;
};

}  // namespace stark_power_manager
