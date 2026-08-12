/*
 * BatteryTypes.h — 电池包协议数据结构和常量定义
 * Copyright (c) 2026 zhiqiang.yang
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stark_power_manager {

/* 帧协议常量 */
constexpr uint8_t BATTERY_FRAME_HEAD_REQ = 0xF1;
constexpr uint8_t BATTERY_FRAME_HEAD_RSP = 0xF2;
constexpr uint8_t BATTERY_FRAME_TAIL_REQ = 0xF2;
constexpr uint8_t BATTERY_FRAME_TAIL_RSP = 0xF1;
constexpr uint8_t BATTERY_ADDR_HOST  = 0xC0;
constexpr uint8_t BATTERY_ADDR_BMS   = 0x01;
constexpr uint8_t BATTERY_FRAME_HDR_LEN = 5;
constexpr uint8_t BATTERY_FRAME_MIN_LEN  = 8;

/* 功能码 */
namespace BatteryFunc {
    constexpr uint8_t BASIC_INFO    = 0x10;
    constexpr uint8_t VOLTAGE_CTRL  = 0x20;
    constexpr uint8_t COMPREHENSIVE = 0x50;
    constexpr uint8_t CONTROL       = 0x90;
}

/* 指令码 */
namespace BatteryCmd {
    constexpr uint8_t GET_VERSION     = 0x03;
    constexpr uint8_t GET_ID          = 0x05;
    constexpr uint8_t WRITE_QR        = 0x06;
    constexpr uint8_t READ_QR         = 0x07;
    constexpr uint8_t CELL_VOLTAGE    = 0x04;
    constexpr uint8_t CHARGE_CURRENT  = 0x05;
    constexpr uint8_t CHARGER_STATUS  = 0x06;
    constexpr uint8_t MOS_CTRL        = 0x07;
    constexpr uint8_t CURRENT_TEMP    = 0x02;
    constexpr uint8_t SOC_CAPACITY    = 0x04;
    constexpr uint8_t FAULT_INFO      = 0x3B;
    constexpr uint8_t FAULT_COUNT     = 0x3C;
    constexpr uint8_t POWER_CTRL      = 0x01;
    constexpr uint8_t LED_CTRL        = 0x02;
    constexpr uint8_t CHARGE_SWITCH   = 0x03;
}

namespace BatteryPowerCtrl {
    constexpr uint8_t POWER_OFF        = 0x00;
    constexpr uint8_t LIGHT_LOAD_DELAY = 0x10;
    constexpr uint8_t LIGHT_LOAD_5S    = 0x11;
}

namespace BatteryChargeSwitch {
    constexpr uint8_t DISCHARGE_TO_CHARGE = 0x00;
    constexpr uint8_t CHARGE_TO_DISCHARGE = 0x01;
}

namespace BatteryChargerStatus {
    constexpr uint8_t NOT_PLUGGED = 0x00;
    constexpr uint8_t PLUGGED     = 0x01;
}

namespace BatteryMosCtrl {
    constexpr uint8_t NO_ACTION = 0x00;
    constexpr uint8_t OPEN      = 0x01;
    constexpr uint8_t CLOSE     = 0x02;
}

namespace BatteryBmsStatus {
    constexpr uint8_t NORMAL_DISCHARGE    = 0x1A;
    constexpr uint8_t NORMAL_CHARGE       = 0x0A;
    constexpr uint8_t CHARGE_LOW_TEMP     = 0xC1;
    constexpr uint8_t CHARGE_HIGH_TEMP    = 0xC2;
    constexpr uint8_t CHARGE_NTC_SHORT    = 0xA1;
    constexpr uint8_t CHARGE_NTC_OPEN     = 0xA2;
    constexpr uint8_t CHARGE_VOLT_DIFF    = 0xA3;
    constexpr uint8_t CHARGER_OVERCURRENT = 0xB1;
    constexpr uint8_t CHARGER_OVERVOLTAGE = 0xB2;
    constexpr uint8_t CHARGE_MOS_FAULT    = 0xB3;
    constexpr uint8_t CHARGE_FULL_5MIN    = 0xF0;
    constexpr uint8_t CHARGE_TIMEOUT      = 0xF1;
}

struct BatteryPkg {
    bool    is_request;
    uint8_t src_addr;
    uint8_t dst_addr;
    uint8_t func_code;
    uint8_t cmd_code;
    std::vector<uint8_t> data;
};

struct BatteryStatus {
    uint32_t total_voltage_mv;
    uint16_t cell_voltage_mv[6];
    int32_t  current_ma;
    uint16_t temperature_k_raw;
    uint8_t  soc_percent;
    uint16_t remain_capacity_mah;
    uint16_t total_capacity_mah;
    uint16_t cycle_count;
    uint8_t  bms_status;
    uint16_t charge_current_ma;
    double   temperature_c;
    uint64_t timestamp_ms;
    bool     has_voltage;
    bool     has_current_temp;
    bool     has_soc_capacity;
};

/* 0x503B 实时故障状态 — V1.02 7字节格式 */
struct BatteryFault {
    uint8_t sys_fault1;
    uint8_t sys_fault2;
    uint8_t dischg_fault1;
    uint8_t dischg_fault2;
    uint8_t chg_fault1;
    uint8_t chg_fault2;
    uint8_t pack_status;

    bool comm_timeout;
    bool cell_ntc_fault;
    bool mos_ntc_fault;
    bool cell_ntc_short;
    bool cell_ntc_open;
    bool mos_ntc_short;
    bool mos_ntc_open;
    bool cell_fault;
    bool afe_fault;
    bool fuse_fault;
    bool overdischarge_l1;
    bool dischg_overcurrent_l1;
    bool dischg_cell_overtemp;
    bool dischg_mos_overtemp;
    bool dischg_cell_lowtemp;
    bool overdischarge_l2;
    bool dischg_overcurrent_l2;
    bool dischg_short;
    bool dischg_mos_fault;
    bool overcharge_l1;
    bool chg_overcurrent_l1;
    bool chg_overtemp;
    bool charger_fault;
    bool chg_mos_fault;
    bool chg_timeout;
    bool overcharge_l2;
    bool chg_overcurrent_l2;
    bool chg_mos_active;
    bool dischg_mos_active;
    bool chg_mos_master_ctrl;
    bool dischg_mos_master_ctrl;
    uint64_t timestamp_ms;
};

/* 0x503C 历史故障次数 */
struct FaultCounters {
    uint16_t overcharge;
    uint16_t chg_overtemp;
    uint16_t chg_overcurrent;
    uint16_t charger_fault;
    uint16_t chg_timeout;
    uint16_t overdischarge;
    uint16_t dischg_overtemp;
    uint16_t dischg_overcurrent_l1;
    uint16_t dischg_overcurrent_l2;
    uint16_t dischg_short;
    uint16_t comm_timeout;
    uint64_t timestamp_ms;
};

struct BatteryInfo {
    std::string version;
    std::string id;
    uint64_t timestamp_ms;
};

inline bool needBlockCharge(const BatteryFault& f) {
    if (f.cell_ntc_fault || f.cell_fault || f.afe_fault || f.fuse_fault)
        return true;
    if (f.overcharge_l1 || f.chg_overcurrent_l1 || f.chg_overtemp ||
        f.charger_fault || f.chg_mos_fault || f.chg_timeout ||
        f.overcharge_l2 || f.chg_overcurrent_l2)
        return true;
    if (f.dischg_overcurrent_l1 || f.dischg_cell_overtemp ||
        f.dischg_mos_overtemp || f.dischg_cell_lowtemp ||
        f.dischg_overcurrent_l2 || f.dischg_short)
        return true;
    return false;
}

inline bool needLimitDischarge(const BatteryFault& f) {
    if (f.sys_fault1 != 0 || f.sys_fault2 != 0)
        return true;
    if (f.dischg_fault1 != 0 || f.dischg_fault2 != 0)
        return true;
    return false;
}

}  /* namespace stark_power_manager */
