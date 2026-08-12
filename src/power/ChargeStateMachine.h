/*
 * ChargeStateMachine.h -- 充电状态机 (基于 SML)
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 状态:
 *   IDLE -> DETECT -> CHARGE -> FULL -> FAULT
 *
 * 事件:
 *   adapter_online: 适配器插入
 *   adapter_offline: 适配器拔出
 *   pd_ready: PD 协商完成
 *   charge_started: IP2366 开始充电
 *   charge_full: IP2366 报充满
 *   fault_detected: 故障
 *   fault_cleared: 故障恢复
 */
#pragma once

#include <map>
#include <utility>

namespace stark_power_manager {

enum class ChargeState {
    IDLE,      /* 未充电 */
    DETECT,    /* 检测适配器/PD协商 */
    CHARGE,    /* 充电中 (硬件管CC/CV) */
    FULL,      /* 充满 */
    FAULT,     /* 故障 */
};

enum class ChargeEvent {
    ADAPTER_ONLINE,
    ADAPTER_OFFLINE,
    PD_READY,
    CHARGE_STARTED,
    CHARGE_FULL,
    FAULT_DETECTED,
    FAULT_CLEARED,
};

struct ChargeStateMachine {
    ChargeState current = ChargeState::IDLE;

    /*
     * 状态转换表
     *
     * IDLE   + ADAPTER_ONLINE  -> DETECT
     * IDLE   + FAULT_DETECTED  -> FAULT
     * DETECT + PD_READY        -> CHARGE
     * DETECT + ADAPTER_OFFLINE -> IDLE
     * DETECT + FAULT_DETECTED  -> FAULT
     * CHARGE + CHARGE_FULL     -> FULL
     * CHARGE + ADAPTER_OFFLINE -> IDLE
     * CHARGE + FAULT_DETECTED  -> FAULT
     * FULL   + ADAPTER_OFFLINE -> IDLE
     * FULL   + ADAPTER_ONLINE  -> CHARGE  (再充电)
     * FULL   + FAULT_DETECTED  -> FAULT
     * FAULT  + FAULT_CLEARED   -> IDLE
     */
    bool transition(ChargeEvent event);
    const char* stateName() const;
};

} /* namespace stark_power_manager */
