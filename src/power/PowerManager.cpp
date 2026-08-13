/*
 * PowerManager.cpp -- 充电管理器实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "PowerManager.h"
#include "PowerRegistry.h"
#include <algorithm>

namespace stark_power_manager {

PowerManager::PowerManager(const PowerManagerConfig& cfg)
    : m_cfg(cfg)
{
}

bool PowerManager::initialize()
{
    auto now = std::chrono::steady_clock::now();
    m_adapter_changed_at = now;
    m_fault_started_at = now;
    m_recovery_started_at = now;
    m_charge_started_at = now;
    m_detect_started_at = now;
    return true;
}

void PowerManager::shutdown()
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    m_state_cb = nullptr;
    m_fault_cb = nullptr;
}

void PowerManager::setChargerOnline(bool online)
{
    bool prev = m_adapter_online.exchange(online);
    if (prev != online) {
        m_adapter_dirty.store(true);
    }
}

void PowerManager::setStateChangeCb(StateChangeCb cb)
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    m_state_cb = std::move(cb);
}

void PowerManager::setFaultCb(FaultCb cb)
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    m_fault_cb = std::move(cb);
}

/*
 * 主循环: 读数据源 → 状态机评估
 */
void PowerManager::tick()
{
    bool prev_online = m_adapter_online.load();

    readChargerStatus();
    readBatteryStatus();

    /* 适配器状态变化时更新消抖计时起点 */
    bool curr_online = m_adapter_online.load();
    if (curr_online != prev_online || m_adapter_dirty.exchange(false)) {
        m_adapter_changed_at = std::chrono::steady_clock::now();
    }

    switch (m_sm.current) {
    case ChargeState::IDLE:   tickIdle();   break;
    case ChargeState::DETECT: tickDetect(); break;
    case ChargeState::CHARGE: tickCharge(); break;
    case ChargeState::FULL:   tickFull();   break;
    case ChargeState::FAULT:  tickFault();  break;
    }
}

/*
 * IDLE: 等待适配器插入
 */
void PowerManager::tickIdle()
{
    if (adapterDebounced()) {
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::ADAPTER_ONLINE)) {
            m_detect_started_at = std::chrono::steady_clock::now();
            m_fault_debouncing = false;
            applyControl(ChargeEvent::ADAPTER_ONLINE);
            notifyStateChange(prev, m_sm.current);
        }
    }
}

/*
 * DETECT: 等待 PD 协商完成
 */
void PowerManager::tickDetect()
{
    /* 适配器拔出 */
    if (!m_adapter_online.load()) {
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::ADAPTER_OFFLINE)) {
            m_fault_debouncing = false;
            applyControl(ChargeEvent::ADAPTER_OFFLINE);
            notifyStateChange(prev, m_sm.current);
        }
        return;
    }

    /* 检查故障 (在 DETECT 阶段也可能出现 BMS 故障/温度异常) */
    if (hasAnyFault()) {
        if (faultDebounced()) {
            ChargeState prev = m_sm.current;
            if (m_sm.transition(ChargeEvent::FAULT_DETECTED)) {
                notifyFault(m_last_fault.c_str());
                applyControl(ChargeEvent::FAULT_DETECTED);
                notifyStateChange(prev, m_sm.current);
            }
        }
        return;
    } else {
        m_fault_debouncing = false;
    }

    /* PD 协商完成: 交叉验证 charger + battery 都报充电 */
    if (isActuallyCharging()) {
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::PD_READY)) {
            m_charge_started_at = std::chrono::steady_clock::now();
            m_fault_debouncing = false;
            applyControl(ChargeEvent::PD_READY);
            notifyStateChange(prev, m_sm.current);
        }
        return;
    }

    /* PD 协商超时 */
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_detect_started_at).count();
    if (elapsed >= m_cfg.pd_timeout_ms) {
        m_last_fault = "PD negotiation timeout";
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::FAULT_DETECTED)) {
            notifyFault(m_last_fault.c_str());
            applyControl(ChargeEvent::FAULT_DETECTED);
            notifyStateChange(prev, m_sm.current);
        }
    }
}

/*
 * CHARGE: 充电中, 监控充满/故障/超时
 */
void PowerManager::tickCharge()
{
    /* 适配器拔出 */
    if (!m_adapter_online.load()) {
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::ADAPTER_OFFLINE)) {
            m_fault_debouncing = false;
            applyControl(ChargeEvent::ADAPTER_OFFLINE);
            notifyStateChange(prev, m_sm.current);
        }
        return;
    }

    /* 故障检查 */
    if (hasAnyFault()) {
        if (faultDebounced()) {
            ChargeState prev = m_sm.current;
            if (m_sm.transition(ChargeEvent::FAULT_DETECTED)) {
                notifyFault(m_last_fault.c_str());
                applyControl(ChargeEvent::FAULT_DETECTED);
                notifyStateChange(prev, m_sm.current);
            }
        }
        return;
    } else {
        m_fault_debouncing = false;
    }

    /* 充满检查 */
    if (isActuallyFull()) {
        m_full_voltage_mv = m_batt_voltage_mv;
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::CHARGE_FULL)) {
            m_fault_debouncing = false;
            applyControl(ChargeEvent::CHARGE_FULL);
            notifyStateChange(prev, m_sm.current);
        }
    }
}

/*
 * FULL: 充满, 监控再充电/适配器拔出/故障
 */
void PowerManager::tickFull()
{
    /* 适配器拔出 */
    if (!m_adapter_online.load()) {
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::ADAPTER_OFFLINE)) {
            m_fault_debouncing = false;
            applyControl(ChargeEvent::ADAPTER_OFFLINE);
            notifyStateChange(prev, m_sm.current);
        }
        return;
    }

    /* 故障检查 */
    if (hasAnyFault()) {
        if (faultDebounced()) {
            ChargeState prev = m_sm.current;
            if (m_sm.transition(ChargeEvent::FAULT_DETECTED)) {
                notifyFault(m_last_fault.c_str());
                applyControl(ChargeEvent::FAULT_DETECTED);
                notifyStateChange(prev, m_sm.current);
            }
        }
        return;
    } else {
        m_fault_debouncing = false;
    }

    /* 再充电: 电池电压低于阈值 (减去回滞) */
    int recharge_threshold = m_full_voltage_mv - m_cfg.recharge_hyst_mv;
    if (m_batt_voltage_mv > 0 && m_batt_voltage_mv < recharge_threshold) {
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::ADAPTER_ONLINE)) {
            m_charge_started_at = std::chrono::steady_clock::now();
            m_fault_debouncing = false;
            applyControl(ChargeEvent::ADAPTER_ONLINE);
            notifyStateChange(prev, m_sm.current);
        }
    }
}

/*
 * FAULT: 故障, 等待恢复
 */
void PowerManager::tickFault()
{
    /* 适配器拔出时无条件恢复 */
    if (!m_adapter_online.load()) {
        m_recovery_debouncing = false;
        m_fault_debouncing = false;
        ChargeState prev = m_sm.current;
        if (m_sm.transition(ChargeEvent::FAULT_CLEARED)) {
            notifyStateChange(prev, m_sm.current);
        }
        return;
    }

    /* 故障恢复检查 */
    if (isFaultCleared()) {
        if (recoveryDebounced()) {
            m_fault_debouncing = false;
            ChargeState prev = m_sm.current;
            if (m_sm.transition(ChargeEvent::FAULT_CLEARED)) {
                notifyStateChange(prev, m_sm.current);
            }
        }
    } else {
        m_recovery_debouncing = false;
        /* 更新最后故障原因 */
        if (hasAnyFault()) {
            /* hasAnyFault 已更新 m_last_fault */
        }
    }
}

/*
 * 从 PowerRegistry 读取 charger 数据
 */
bool PowerManager::readChargerStatus()
{
    auto& reg = PowerRegistry::instance();
    PowerValue v;

    /* IP2366 充电状态: "idle"/"charging"/"full"/"fault" */
    if (reg.getProp("charger_ip2366", PowerProp::STATUS, v)) {
        const auto& s = v.asStr();
        m_charger_charging = (s == "charging");
        m_charger_full = (s == "full");
    }

    /* 充电阶段 (trickle/cc/cv/none), 供 CC 超时等判断使用 */
    if (reg.getProp("charger_ip2366", PowerProp::CHARGE_TYPE, v)) {
        m_charge_type = v.asStr();
    }

    /* IP2366 是否在线 */
    if (reg.getProp("charger_ip2366", PowerProp::ONLINE, v)) {
        bool online = v.asBool();
        if (m_adapter_online.exchange(online) != online) {
            m_adapter_dirty.store(true);
        }
    }

    /* 充电电流 (正=充电) */
    if (reg.getProp("charger_ip2366", PowerProp::CURRENT_NOW, v)) {
        m_charge_current_ma = static_cast<int>(v.asInt());
    }

    return true;
}

/*
 * 从 PowerRegistry 读取 battery (BMS) 数据
 */
bool PowerManager::readBatteryStatus()
{
    auto& reg = PowerRegistry::instance();
    PowerValue v;

    /* BMS SOC */
    if (reg.getProp("battery_bms", PowerProp::CAPACITY, v)) {
        m_soc_percent = static_cast<int>(v.asInt());
    }

    /* BMS 温度 (x10, 例 325=32.5C) */
    if (reg.getProp("battery_bms", PowerProp::TEMPERATURE, v)) {
        m_batt_temp_c = static_cast<int>(v.asInt()) / 10;
    }

    /* BMS 充电状态: "idle"/"charging"/"discharging"/"full" */
    if (reg.getProp("battery_bms", PowerProp::STATUS, v)) {
        const auto& s = v.asStr();
        m_bms_charging = (s == "charging");
        m_bms_full = (s == "full");
        m_bms_available = true;
    } else {
        /* BMS 状态读不到 => 数据源不可用, 交叉验证降级为单源 */
        m_bms_available = false;
    }

    /* BMS 故障 */
    if (reg.getProp("battery_bms", PowerProp::FAULT, v)) {
        m_bms_fault = v.asBool();
    }

    /* BMS 电压 */
    if (reg.getProp("battery_bms", PowerProp::VOLTAGE_NOW, v)) {
        m_batt_voltage_mv = static_cast<int>(v.asInt());
    }

    /* BMS 电流 (正=充电) */
    if (reg.getProp("battery_bms", PowerProp::CURRENT_NOW, v)) {
        int bms_current = static_cast<int>(v.asInt());
        /* BMS 电流优先作为充电电流, 因为更接近电池端 */
        if (bms_current >= 0) {
            m_charge_current_ma = bms_current;
        }
    }

    /* 满充电压 (用于再充电阈值计算) */
    if (reg.getProp("battery_bms", PowerProp::VOLTAGE_MAX, v)) {
        int max_v = static_cast<int>(v.asInt());
        if (max_v > 0) {
            m_full_voltage_mv = max_v;
        }
    }

    return true;
}

/*
 * 交叉验证: 是否真的在充电
 */
bool PowerManager::isActuallyCharging()
{
    if (m_cfg.cross_verify_charge && m_bms_available) {
        return m_charger_charging && m_bms_charging;
    }
    return m_charger_charging;
}

/*
 * 是否充满
 */
bool PowerManager::isActuallyFull()
{
    /* charger 报充满 */
    if (m_charger_full) {
        return true;
    }

    /* SOC 满且电流低于阈值 (补充判断, 防止 charger 未及时报充满) */
    if (m_soc_percent >= m_cfg.full_soc_percent
        && m_charge_current_ma >= 0
        && m_charge_current_ma < m_cfg.full_current_ma) {
        return true;
    }

    return false;
}

/*
 * 综合故障判断
 */
bool PowerManager::hasAnyFault()
{
    /* BMS 故障 */
    if (m_bms_fault) {
        m_last_fault = "BMS reported fault";
        return true;
    }

    /* 温度超限 */
    if (m_batt_temp_c > m_cfg.max_temp_c) {
        m_last_fault = "Battery over temperature";
        return true;
    }

    /* 充电超时 */
    if (m_sm.current == ChargeState::CHARGE) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_min = std::chrono::duration_cast<std::chrono::minutes>(
            now - m_charge_started_at).count();

        if (elapsed_min >= m_cfg.total_timeout_min) {
            m_last_fault = "Charge total timeout";
            return true;
        }

        /* CC 阶段超时: 仅当仍处于 CC 阶段时才判定, 避免 CV 阶段误报 */
        if (m_charge_type == "cc" && !m_charger_full
            && elapsed_min >= m_cfg.cc_timeout_min) {
            m_last_fault = "Charge CC phase timeout";
            return true;
        }
    }

    /* charger 故障 */
    auto& reg = PowerRegistry::instance();
    PowerValue v;
    if (reg.getProp("charger_ip2366", PowerProp::FAULT, v)) {
        if (v.asBool()) {
            /* 读取故障原因 */
            PowerValue reason;
            if (reg.getProp("charger_ip2366", PowerProp::FAULT_REASON, reason)) {
                m_last_fault = "Charger fault: " + reason.asStr();
            } else {
                m_last_fault = "Charger reported fault";
            }
            return true;
        }
    }

    return false;
}

/*
 * 故障恢复条件
 */
bool PowerManager::isFaultCleared()
{
    /* BMS 故障必须清除 */
    if (m_bms_fault) {
        return false;
    }

    /* 温度必须降到恢复阈值以下 */
    if (m_batt_temp_c > m_cfg.resume_temp_c) {
        return false;
    }

    /* charger 故障必须清除 */
    auto& reg = PowerRegistry::instance();
    PowerValue v;
    if (reg.getProp("charger_ip2366", PowerProp::FAULT, v)) {
        if (v.asBool()) {
            return false;
        }
    }

    return true;
}

/*
 * 适配器消抖: 连续在线超过 debounce_ms
 */
bool PowerManager::adapterDebounced()
{
    if (!m_adapter_online.load()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_adapter_changed_at).count();
    return elapsed >= m_cfg.adapter_debounce_ms;
}

/*
 * 故障消抖: 连续故障超过 fault_debounce_ms
 */
bool PowerManager::faultDebounced()
{
    auto now = std::chrono::steady_clock::now();

    if (!m_fault_debouncing) {
        m_fault_debouncing = true;
        m_fault_started_at = now;
        return false;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_fault_started_at).count();
    return elapsed >= m_cfg.fault_debounce_ms;
}

/*
 * 恢复消抖: 连续无故障超过 recovery_debounce_ms
 */
bool PowerManager::recoveryDebounced()
{
    auto now = std::chrono::steady_clock::now();

    if (!m_recovery_debouncing) {
        m_recovery_debouncing = true;
        m_recovery_started_at = now;
        return false;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_recovery_started_at).count();
    return elapsed >= m_cfg.recovery_debounce_ms;
}

/*
 * 状态转换时的控制下发
 * 事件驱动: 根据触发转换的事件, 下发对应的硬件控制指令
 * (fire-and-forget, 底层驱动的错误由各自 setProp 内部打日志)
 */
void PowerManager::applyControl(ChargeEvent event)
{
    auto& reg = PowerRegistry::instance();

    switch (event) {
    case ChargeEvent::ADAPTER_ONLINE:
        /* 适配器插入/再充电: 通知 BMS + 开充电 MOS + 开 IP2366 充电使能 */
        reg.setProp("battery_bms", PowerProp::CHARGER_PRESENT, PowerValue(true));
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(true));
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(true));
        break;

    case ChargeEvent::ADAPTER_OFFLINE:
        /* 适配器拔出: 通知 BMS + 关充电 MOS + 关 IP2366 充电使能 */
        reg.setProp("battery_bms", PowerProp::CHARGER_PRESENT, PowerValue(false));
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(false));
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(false));
        break;

    case ChargeEvent::PD_READY:
        /* 开始充电: 确保充电 MOS 开 + IP2366 充电使能 (硬件自动走充电曲线) */
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(true));
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(true));
        break;

    case ChargeEvent::CHARGE_FULL:
        /* 充满: 双重停止 (关 BMS 充电 MOS + 关 IP2366 充电使能) */
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(false));
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(false));
        break;

    case ChargeEvent::FAULT_DETECTED:
        /* 故障: 关 IP2366 充电 + 关充电 MOS + 关放电 MOS (防御纵深) */
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(false));
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(false));
        reg.setProp("battery_bms", PowerProp::DISCHARGE_ENABLE, PowerValue(false));
        break;

    default:
        break;
    }
}

/*
 * 状态变化通知
 */
void PowerManager::notifyStateChange(ChargeState from, ChargeState to)
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    if (m_state_cb) {
        m_state_cb(from, to);
    }
}

/*
 * 故障通知
 */
void PowerManager::notifyFault(const char* reason)
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    if (m_fault_cb) {
        m_fault_cb(reason);
    }
}

} /* namespace stark_power_manager */
