/*
 * ChargeStateMachine.cpp -- 充电状态机实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "ChargeStateMachine.h"

namespace stark_power_manager {

bool ChargeStateMachine::transition(ChargeEvent event)
{
    using Key = std::pair<ChargeState, ChargeEvent>;

    static const std::map<Key, ChargeState> table = {
        {{ChargeState::IDLE,   ChargeEvent::ADAPTER_ONLINE},  ChargeState::DETECT},
        {{ChargeState::IDLE,   ChargeEvent::FAULT_DETECTED},  ChargeState::FAULT},
        {{ChargeState::DETECT, ChargeEvent::PD_READY},        ChargeState::CHARGE},
        {{ChargeState::DETECT, ChargeEvent::ADAPTER_OFFLINE}, ChargeState::IDLE},
        {{ChargeState::DETECT, ChargeEvent::FAULT_DETECTED},  ChargeState::FAULT},
        {{ChargeState::CHARGE, ChargeEvent::CHARGE_FULL},     ChargeState::FULL},
        {{ChargeState::CHARGE, ChargeEvent::ADAPTER_OFFLINE}, ChargeState::IDLE},
        {{ChargeState::CHARGE, ChargeEvent::FAULT_DETECTED},  ChargeState::FAULT},
        {{ChargeState::FULL,   ChargeEvent::ADAPTER_OFFLINE}, ChargeState::IDLE},
        {{ChargeState::FULL,   ChargeEvent::ADAPTER_ONLINE},  ChargeState::CHARGE},
        {{ChargeState::FULL,   ChargeEvent::FAULT_DETECTED},  ChargeState::FAULT},
        {{ChargeState::FAULT,  ChargeEvent::FAULT_CLEARED},   ChargeState::IDLE},
    };

    auto it = table.find({current, event});
    if (it == table.end()) {
        return false;
    }

    current = it->second;
    return true;
}

const char* ChargeStateMachine::stateName() const
{
    switch (current) {
    case ChargeState::IDLE:   return "IDLE";
    case ChargeState::DETECT: return "DETECT";
    case ChargeState::CHARGE: return "CHARGE";
    case ChargeState::FULL:   return "FULL";
    case ChargeState::FAULT:  return "FAULT";
    }
    return "UNKNOWN";
}

} /* namespace stark_power_manager */
