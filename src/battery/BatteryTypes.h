/*
 * BatteryTypes.h — 电池包协议数据结构和常量定义
 * Copyright (c) 2026 zhiqiang.yang
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stark_power_manager {

/* ================================================================
 * 帧协议常量
 * ================================================================ */

constexpr uint8_t BATTERY_FRAME_HEAD_REQ = 0xF1;   /* 请求帧头 */
constexpr uint8_t BATTERY_FRAME_HEAD_RSP = 0xF2;   /* 响应帧头 */
constexpr uint8_t BATTERY_FRAME_TAIL_REQ = 0xF2;   /* 请求帧尾 */
constexpr uint8_t BATTERY_FRAME_TAIL_RSP = 0xF1;   /* 响应帧尾 */

constexpr uint8_t BATTERY_ADDR_HOST  = 0xC0;       /* 主站地址 (控制板) */
constexpr uint8_t BATTERY_ADDR_BMS   = 0x01;       /* 从站地址 (电池包) */

constexpr uint8_t BATTERY_FRAME_HDR_LEN = 5;       /* 帧定长头: src(1)+dst(1)+func(1)+cmd(1)+len(1) */
constexpr uint8_t BATTERY_FRAME_MIN_LEN  = 8;      /* 最小帧: 帧头(1)+定长(5)+校验(1)+帧尾(1), N=0 */

/* ================================================================
 * 功能码
 * ================================================================ */

namespace BatteryFunc {
    constexpr uint8_t BASIC_INFO    = 0x10;         /* 基本信息 */
    constexpr uint8_t VOLTAGE_CTRL  = 0x20;         /* 电压/电流 */
    constexpr uint8_t COMPREHENSIVE = 0x50;         /* 综合状态 */
    constexpr uint8_t CONTROL       = 0x90;         /* 控制指令 */
}

/* ================================================================
 * 指令码
 * ================================================================ */

namespace BatteryCmd {
    /* 0x10 基本信息 */
    constexpr uint8_t GET_VERSION     = 0x03;       /* 获取 BMS 版本号 */
    constexpr uint8_t GET_ID          = 0x05;       /* 获取电池包 ID */
    constexpr uint8_t WRITE_QR        = 0x06;       /* 写客户二维码信息 */
    constexpr uint8_t READ_QR         = 0x07;       /* 读客户二维码信息 */

    /* 0x20 电压/电流 */
    constexpr uint8_t CELL_VOLTAGE    = 0x04;       /* 电芯电压数据 */
    constexpr uint8_t CHARGE_CURRENT  = 0x05;       /* 充电电流请求 */

    /* 0x50 综合状态 */
    constexpr uint8_t CURRENT_TEMP    = 0x02;       /* 电流+温度 */
    constexpr uint8_t SOC_CAPACITY    = 0x04;       /* SOC+容量+状态 */
    constexpr uint8_t FAULT_INFO      = 0x3B;       /* 故障信息读取 */

    /* 0x90 控制指令 */
    constexpr uint8_t POWER_CTRL      = 0x01;       /* 停止供电/轻载保护 */
    constexpr uint8_t CHARGE_SWITCH   = 0x03;       /* 充放电转换 */
}

/* ================================================================
 * 控制字段
 * ================================================================ */

namespace BatteryPowerCtrl {
    constexpr uint8_t POWER_OFF           = 0x00;  /* 停止供电(关机)延时休眠 */
    constexpr uint8_t LIGHT_LOAD_DELAY    = 0x10;  /* 修改轻载保护延时 1.5h */
    constexpr uint8_t LIGHT_LOAD_5S       = 0x11;  /* 打开 5s 轻载保护 */
}

namespace BatteryChargeSwitch {
    constexpr uint8_t DISCHARGE_TO_CHARGE = 0x00;  /* 放电转充电 */
    constexpr uint8_t CHARGE_TO_DISCHARGE = 0x01;  /* 充电转放电 */
}


/* ================================================================
 * BMS 状态码
 * ================================================================ */

namespace BatteryBmsStatus {
    constexpr uint8_t NORMAL_DISCHARGE    = 0x1A;  /* 正常放电 */
    constexpr uint8_t NORMAL_CHARGE       = 0x0A;  /* 正常充电 */
    constexpr uint8_t CHARGE_LOW_TEMP     = 0xC1;  /* 充电低温(可恢复) */
    constexpr uint8_t CHARGE_HIGH_TEMP    = 0xC2;  /* 充电高温(可恢复) */
    constexpr uint8_t CHARGE_NTC_SHORT    = 0xA1;  /* NTC短路(不可恢复) */
    constexpr uint8_t CHARGE_NTC_OPEN     = 0xA2;  /* NTC断路(不可恢复) */
    constexpr uint8_t CHARGE_VOLT_DIFF    = 0xA3;  /* 充电压差故障(不可恢复) */
    constexpr uint8_t CHARGER_OVERCURRENT = 0xB1;  /* 充电器过流 */
    constexpr uint8_t CHARGER_OVERVOLTAGE = 0xB2;  /* 充电器过压 */
    constexpr uint8_t CHARGE_MOS_FAULT    = 0xB3;  /* 充电MOS异常 */
    constexpr uint8_t CHARGE_FULL_5MIN    = 0xF0;  /* 充电100%过5分钟 */
    constexpr uint8_t CHARGE_TIMEOUT      = 0xF1;  /* 充电超时 */
}

/* ================================================================
 * 故障位掩码
 * ================================================================ */

namespace BatteryFaultBit {
    constexpr uint16_t CHARGER_OVERVOLTAGE    = 0x0001;  /* bit0: 充电器过压 */
    constexpr uint16_t CHARGE_OVERCURRENT     = 0x0002;  /* bit1: 充电过流 */
    constexpr uint16_t NTC_SHORT              = 0x0004;  /* bit2: NTC短路 */
    constexpr uint16_t NTC_OPEN               = 0x0008;  /* bit3: NTC开路 */
    constexpr uint16_t CELL_VOLTAGE_DIFF      = 0x0010;  /* bit4: 电芯压差 */
    constexpr uint16_t CHARGE_TIMEOUT         = 0x0020;  /* bit5: 充电超时 */
    constexpr uint16_t DISCHARGE_OVERCURRENT  = 0x0040;  /* bit6: 放电过流 */
    constexpr uint16_t DISCHARGE_SHORT        = 0x0080;  /* bit7: 放电短路 */
    constexpr uint16_t SECONDARY_OVERCHARGE   = 0x0100;  /* bit8: 二次过充 */
}

/* ================================================================
 * 电池通信数据包
 * ================================================================ */

struct BatteryPkg {
    bool    is_request;                             /* true=请求, false=响应 */
    uint8_t src_addr;                               /* 发送地址 */
    uint8_t dst_addr;                               /* 接收地址 */
    uint8_t func_code;                              /* 功能码 */
    uint8_t cmd_code;                               /* 指令码 */
    std::vector<uint8_t> data;                      /* 数据域 */
};

/* ================================================================
 * 电池状态数据结构 (合并 0x2004 + 0x5002 + 0x5004)
 * ================================================================ */

struct BatteryStatus {
    /* 电芯电压 (0x2004) */
    uint32_t total_voltage_mv;                      /* 总电压, mV */
    uint16_t cell_voltage_mv[6];                    /* 各节电芯电压, mV */

    /* 电流+温度 (0x5002) */
    int32_t  current_ma;                            /* 充放电电流, mA (正=充电, 负=放电) */
    uint16_t temperature_k_raw;                     /* 开氏温度 ×10, 例 25.2℃=2982 */

    /* SOC+容量+状态 (0x5004) */
    uint8_t  soc_percent;                           /* 电量百分比, 0-100 */
    uint16_t remain_capacity_mah;                   /* 剩余容量, mAh */
    uint16_t total_capacity_mah;                    /* 总容量, mAh */
    uint16_t cycle_count;                           /* 循环次数 */
    uint8_t  bms_status;                            /* BMS 状态码, 见 BatteryBmsStatus */
    uint16_t charge_current_ma;                     /* 充电电流设定值 mA (0x2005) */

    /* 计算值 */
    double   temperature_c;                         /* 温度, ℃ = K/10 - 273.15 */
    uint64_t timestamp_ms;                          /* 数据更新时间戳, ms */

    /* 数据有效性标记 (按指令码) */
    bool     has_voltage;                           /* 0x2004 数据有效 */
    bool     has_current_temp;                      /* 0x5002 数据有效 */
    bool     has_soc_capacity;                      /* 0x5004 数据有效 */
};

/* ================================================================
 * 故障信息
 * ================================================================ */

struct BatteryFault {
    uint16_t records[10];                           /* 最近 10 次故障码, [0]=最近 */

    /* 最近一次故障解析 */
    bool charger_overvoltage;
    bool charge_overcurrent;
    bool ntc_short;
    bool ntc_open;
    bool cell_voltage_diff;
    bool charge_timeout;
    bool discharge_overcurrent;
    bool discharge_short;
    bool secondary_overcharge;

    uint64_t timestamp_ms;                          /* 数据更新时间戳 */
};

/* ================================================================
 * 电池基本信息
 * ================================================================ */

struct BatteryInfo {
    std::string version;                            /* BMS 版本号 */
    std::string id;                                 /* 电池包 ID */
    uint64_t timestamp_ms;                          /* 数据更新时间戳 */
};

}  /* namespace stark_power_manager */
