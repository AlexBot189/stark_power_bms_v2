/*
 * BmsUartSource.cpp — 电池包 BMS 数据源适配器实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "BmsUartSource.h"

#include <log_helper/LogHelper.h>

namespace stark_power_manager {

/* ================================================================
 * 构造/析构
 * ================================================================ */

BmsUartSource::BmsUartSource(std::shared_ptr<BatteryDispatcher> dispatcher,
                             const BmsUartConfig& cfg)
    : m_dispatcher(std::move(dispatcher)), m_cfg(cfg)
{
}

BmsUartSource::~BmsUartSource() = default;

/* ================================================================
 * IPowerSource 接口 — 属性列表
 * ================================================================ */

std::vector<PowerProp> BmsUartSource::supportedProps() const
{
    return {
        PowerProp::STATUS,
        PowerProp::HEALTH,
        PowerProp::FAULT,
        PowerProp::FAULT_REASON,
        PowerProp::VOLTAGE_NOW,
        PowerProp::CURRENT_NOW,
        PowerProp::TEMPERATURE,
        PowerProp::CAPACITY,
        PowerProp::VOLTAGE_MAX,
        PowerProp::CELL_VOLTAGE_1,
        PowerProp::CELL_VOLTAGE_2,
        PowerProp::CELL_VOLTAGE_3,
        PowerProp::CELL_VOLTAGE_4,
        PowerProp::CELL_VOLTAGE_5,
        PowerProp::CELL_VOLTAGE_6,
        PowerProp::CAPACITY_FULL,
        PowerProp::CAPACITY_REMAIN,
        PowerProp::CYCLE_COUNT,
        PowerProp::CHARGE_ENABLE,
        PowerProp::DISCHARGE_ENABLE,
        PowerProp::SHUTDOWN,
        PowerProp::CHARGER_PRESENT,
        PowerProp::MODEL_NAME,
        PowerProp::VERSION,
    };
}

/* ================================================================
 * IPowerSource 接口 — 读属性
 * ================================================================ */

bool BmsUartSource::getProp(PowerProp prop, PowerValue& out)
{
    if (!m_dispatcher) {
        return false;
    }

    switch (prop) {
    case PowerProp::STATUS: {
        const auto status = m_dispatcher->GetStatus();
        const auto fault  = m_dispatcher->GetFault();

        if (hasFault(fault)) {
            out = PowerValue("fault");
        } else if (status.bms_status == BatteryBmsStatus::NORMAL_CHARGE) {
            out = PowerValue("charging");
        } else if (status.bms_status == BatteryBmsStatus::NORMAL_DISCHARGE) {
            out = PowerValue("discharging");
        } else if (status.bms_status == BatteryBmsStatus::CHARGE_FULL_5MIN) {
            out = PowerValue("full");
        } else if (status.bms_status == BatteryBmsStatus::CHARGE_TIMEOUT) {
            out = PowerValue("fault");
        } else {
            out = PowerValue("idle");
        }
        return true;
    }

    case PowerProp::HEALTH: {
        const auto fault = m_dispatcher->GetFault();
        if (fault.chg_overtemp || fault.dischg_cell_overtemp
            || fault.dischg_mos_overtemp) {
            out = PowerValue("overheat");
        } else if (fault.overcharge_l1 || fault.overdischarge_l1) {
            out = PowerValue("overvoltage");
        } else if (hasFault(fault)) {
            out = PowerValue("dead");
        } else {
            out = PowerValue("good");
        }
        return true;
    }

    case PowerProp::FAULT:
        out = PowerValue(hasFault(m_dispatcher->GetFault()));
        return true;

    case PowerProp::FAULT_REASON: {
        const auto f = m_dispatcher->GetFault();
        std::string reason;
        if (f.comm_timeout)           reason = "comm_timeout";
        else if (f.chg_overtemp)      reason = "chg_overtemp";
        else if (f.overcharge_l1)     reason = "overcharge";
        else if (f.overdischarge_l1)  reason = "overdischarge";
        else if (f.dischg_short)      reason = "discharge_short";
        else if (f.cell_fault)        reason = "cell_fault";
        else if (f.afe_fault)         reason = "afe_fault";
        else if (f.fuse_fault)        reason = "fuse_fault";
        else if (hasFault(f))         reason = "fault";
        else                          reason = "none";
        out = PowerValue(reason);
        return true;
    }

    case PowerProp::VOLTAGE_NOW: {
        const auto status = m_dispatcher->GetStatus();
        if (!status.has_voltage) return false;
        out = PowerValue(static_cast<int64_t>(status.total_voltage_mv));
        return true;
    }

    case PowerProp::CURRENT_NOW: {
        const auto status = m_dispatcher->GetStatus();
        if (!status.has_current_temp) return false;
        out = PowerValue(static_cast<int64_t>(status.current_ma));
        return true;
    }

    case PowerProp::TEMPERATURE: {
        const auto status = m_dispatcher->GetStatus();
        if (!status.has_current_temp) return false;
        /* temperature_c 是 °C (double), PowerProp 要求 °C x10 */
        out = PowerValue(static_cast<int64_t>(status.temperature_c * 10.0));
        return true;
    }

    case PowerProp::CAPACITY: {
        const auto status = m_dispatcher->GetStatus();
        if (!status.has_soc_capacity) return false;
        out = PowerValue(static_cast<int64_t>(status.soc_percent));
        return true;
    }

    case PowerProp::VOLTAGE_MAX:
        out = PowerValue(static_cast<int64_t>(m_cfg.cell_count * m_cfg.cell_full_mv));
        return true;

    case PowerProp::CELL_VOLTAGE_1:
    case PowerProp::CELL_VOLTAGE_2:
    case PowerProp::CELL_VOLTAGE_3:
    case PowerProp::CELL_VOLTAGE_4:
    case PowerProp::CELL_VOLTAGE_5:
    case PowerProp::CELL_VOLTAGE_6: {
        const auto status = m_dispatcher->GetStatus();
        if (!status.has_voltage) return false;
        int idx = static_cast<int>(prop) - static_cast<int>(PowerProp::CELL_VOLTAGE_1);
        if (idx < 0 || idx >= 6) return false;
        out = PowerValue(static_cast<int64_t>(status.cell_voltage_mv[idx]));
        return true;
    }

    case PowerProp::CAPACITY_FULL:
        out = PowerValue(static_cast<int64_t>(
            m_dispatcher->GetStatus().total_capacity_mah));
        return true;

    case PowerProp::CAPACITY_REMAIN:
        out = PowerValue(static_cast<int64_t>(
            m_dispatcher->GetStatus().remain_capacity_mah));
        return true;

    case PowerProp::CYCLE_COUNT:
        out = PowerValue(static_cast<int64_t>(
            m_dispatcher->GetStatus().cycle_count));
        return true;

    case PowerProp::MODEL_NAME:
        out = PowerValue("BMS");
        return true;

    case PowerProp::VERSION:
        out = PowerValue(m_dispatcher->GetInfo().version);
        return true;

    default:
        return false;
    }
}

/* ================================================================
 * IPowerSource 接口 — 写属性
 * ================================================================ */

bool BmsUartSource::setProp(PowerProp prop, const PowerValue& val)
{
    if (!m_dispatcher) {
        return false;
    }

    switch (prop) {
    case PowerProp::CHARGE_ENABLE: {
        bool en = val.asBool();
        m_dispatcher->ControlMOS(
            en ? BatteryMosCtrl::OPEN : BatteryMosCtrl::CLOSE,
            BatteryMosCtrl::NO_ACTION);
        ECO_INFO("[BmsUartSource] charge enable: %s", en ? "ON" : "OFF");
        return true;
    }

    case PowerProp::DISCHARGE_ENABLE: {
        bool en = val.asBool();
        m_dispatcher->ControlMOS(
            BatteryMosCtrl::NO_ACTION,
            en ? BatteryMosCtrl::OPEN : BatteryMosCtrl::CLOSE);
        ECO_INFO("[BmsUartSource] discharge enable: %s", en ? "ON" : "OFF");
        return true;
    }

    case PowerProp::SHUTDOWN: {
        if (!val.asBool()) {
            return true; /* 关机是单向动作, false 无操作 */
        }
        /* 0x9001 停止供电(即关机)延时休眠 */
        m_dispatcher->SendControl(BatteryFunc::CONTROL, BatteryCmd::POWER_CTRL,
                                  { BatteryPowerCtrl::POWER_OFF });
        ECO_INFO("[BmsUartSource] shutdown (0x9001 POWER_OFF)");
        return true;
    }

    case PowerProp::CHARGER_PRESENT: {
        /* 0x2006 充电器接入通知 */
        bool plugged = val.asBool();
        m_dispatcher->SetChargerStatus(plugged);
        ECO_INFO("[BmsUartSource] charger present: %s", plugged ? "YES" : "NO");
        return true;
    }

    default:
        return false;
    }
}

/* ================================================================
 * 订阅
 * ================================================================ */

void BmsUartSource::subscribe(ChangeCallback cb)
{
    /*
     * 当前设计采用轮询模型: PowerManager 在 1Hz tick 里通过 getProp 拉取,
     * 不依赖 push 通知。BatteryDispatcher 自身的 Observer 回调已由
     * BatteryRosAdapter 消费, 此处不重复桥接, 避免跨线程回调复杂度。
     */
    (void)cb;
}

/* ================================================================
 * 辅助
 * ================================================================ */

bool BmsUartSource::hasFault(const BatteryFault& f)
{
    if (f.sys_fault1 != 0 || f.sys_fault2 != 0) {
        return true;
    }
    if (f.chg_fault1 != 0 || f.chg_fault2 != 0) {
        return true;
    }
    if (f.dischg_fault1 != 0 || f.dischg_fault2 != 0) {
        return true;
    }
    return false;
}

}  // namespace stark_power_manager
