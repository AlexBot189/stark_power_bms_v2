/*
 * PowerManager -- 充电管理器
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 职责:
 *   1. 驱动充电状态机
 *   2. 双数据源交叉验证 (IP2366 + BMS)
 *   3. 故障消抖 + 分级
 *   4. 通知回调
 *
 * 线程模型:
 *   PowerManager 所有公共方法在调用者线程执行
 *   tick() -> 定期调用 (在 BatteryDispatcher 的 poll 线程里, 1Hz)
 *   setChargerOnline() -> IP2366 INT 回调线程
 *
 * 使用方法:
 *   PowerManager mgr;
 *   mgr.initialize();
 *
 *   // 定期 tick (1Hz)
 *   while (running) {
 *     mgr.tick();
 *     sleep(1);
 *   }
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include "ChargeStateMachine.h"

namespace stark_power_manager {

struct PowerManagerConfig {
    /* 消抖 */
    int adapter_debounce_ms = 200;
    int fault_debounce_ms   = 500;
    int recovery_debounce_ms = 2000;

    /* 充电判断 */
    int full_soc_percent    = 100;
    int full_current_ma     = 200;
    int recharge_hyst_mv    = 200;

    /* 超时 */
    int cc_timeout_min      = 240;
    int total_timeout_min   = 480;

    /* 温度 */
    int max_temp_c           = 85;
    int resume_temp_c        = 75;

    /* PD 协商超时 */
    int pd_timeout_ms        = 10000;

    /* 交叉验证: charger 和 battery 都报充电才相信 */
    bool cross_verify_charge = true;

    /* VBUS 最低电压阈值 (mV), 用于判断 PD 协商完成 */
    int vbus_present_mv      = 4000;
};

class PowerManager {
public:
    using StateChangeCb = std::function<void(ChargeState from, ChargeState to)>;
    using FaultCb = std::function<void(const char* reason)>;

    explicit PowerManager(const PowerManagerConfig& cfg = {});

    bool initialize();
    void shutdown();

    /* 定期调用 (1Hz), 驱动状态机 */
    void tick();

    /* 充电器插入/拔出 (IP2366 INT 线程回调) */
    void setChargerOnline(bool online);

    /* 获取当前状态 */
    ChargeState getState() const { return m_sm.current; }

    /* 注册回调 */
    void setStateChangeCb(StateChangeCb cb);
    void setFaultCb(FaultCb cb);

private:
    /* 状态机每个 tick 周期的评估逻辑 */
    void tickIdle();
    void tickDetect();
    void tickCharge();
    void tickFull();
    void tickFault();

    /* 从 PowerRegistry 读双数据源 */
    bool readChargerStatus();
    bool readBatteryStatus();

    /* 交叉验证 */
    bool isActuallyCharging();
    bool isActuallyFull();
    bool hasAnyFault();
    bool isFaultCleared();

    /* 消抖辅助 */
    bool adapterDebounced();
    bool faultDebounced();
    bool recoveryDebounced();

    /* 通知 */
    void notifyStateChange(ChargeState from, ChargeState to);
    void notifyFault(const char* reason);

    PowerManagerConfig m_cfg;
    ChargeStateMachine m_sm;

    /* 适配器状态 (IP2366 INT 线程写入, tick 读取) */
    std::atomic<bool> m_adapter_online{false};
    std::atomic<bool> m_adapter_dirty{false};

    /* 交叉验证标记 */
    bool m_charger_charging = false;
    bool m_charger_full     = false;
    bool m_bms_charging     = false;
    bool m_bms_full         = false;
    bool m_bms_fault        = false;
    int  m_soc_percent      = 0;
    int  m_batt_temp_c      = 0;
    int  m_charge_current_ma = 0;
    int  m_batt_voltage_mv  = 0;
    int  m_full_voltage_mv  = 0;

    /* 故障 */
    std::string m_last_fault;

    /* 消抖计时 */
    std::chrono::steady_clock::time_point m_adapter_changed_at;
    std::chrono::steady_clock::time_point m_fault_started_at;
    std::chrono::steady_clock::time_point m_recovery_started_at;
    bool m_fault_debouncing  = false;
    bool m_recovery_debouncing = false;

    /* 回调 */
    std::mutex m_cb_mutex;
    StateChangeCb m_state_cb;
    FaultCb m_fault_cb;

    /* 充电开始时间 (超时监控) */
    std::chrono::steady_clock::time_point m_charge_started_at;
    std::chrono::steady_clock::time_point m_detect_started_at;
};

} /* namespace stark_power_manager */
