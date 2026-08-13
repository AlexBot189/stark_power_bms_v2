/*
 * PowerProp.h — 电源属性枚举 (参考 Linux power_supply PROP_*)
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 统一命名空间, 所有电源驱动通过此枚举暴露属性。
 * 上层 PowerManager 只通过 prop key 访问, 不依赖硬件类型。
 */
#pragma once

#include <cstdint>
#include <string>

namespace stark_power_manager {

enum class PowerProp {
    /* 状态类 */
    STATUS,          /* 充电状态: "idle"/"charging"/"discharging"/"full"/"fault" */
    HEALTH,          /* 健康状态: "good"/"overheat"/"overvoltage"/"cold"/"dead" */
    ONLINE,          /* bool: 适配器是否插入 */
    PRESENT,         /* bool: 硬件是否在位 */
    CHARGE_TYPE,     /* 充电阶段: "none"/"trickle"/"cc"/"cv" */
    FAULT,           /* bool: 是否有故障 */
    FAULT_REASON,    /* string: 故障原因 */

    /* 测量类 (统一单位) */
    VOLTAGE_NOW,     /* int: 当前电压, mV */
    CURRENT_NOW,     /* int: 当前电流, mA (正=充电 负=放电) */
    TEMPERATURE,     /* int: 温度 x10, 例 32.5C=325 */
    CAPACITY,        /* int: SOC 百分比 0-100 */

    /* 阈值类 */
    VOLTAGE_MAX,     /* int: 充电上限电压, mV */
    CURRENT_MAX,     /* int: 充电上限电流, mA */
    VOLTAGE_MIN,     /* int: 最低放电电压, mV */

    /* 统计类 */
    CYCLE_COUNT,     /* int: 充放电循环次数 */
    CAPACITY_FULL,   /* int: 满充容量, mAh */
    CAPACITY_REMAIN, /* int: 剩余容量, mAh */

    /* 电芯电压 (1-indexed) */
    CELL_VOLTAGE_1,  /* int: mV */
    CELL_VOLTAGE_2,
    CELL_VOLTAGE_3,
    CELL_VOLTAGE_4,
    CELL_VOLTAGE_5,
    CELL_VOLTAGE_6,

    /* 控制类 */
    CHARGE_ENABLE,    /* bool: 充电使能/禁止 */
    DISCHARGE_ENABLE, /* bool: 放电使能/禁止 */
    CHARGE_CURRENT_SET, /* int: 设置充电电流, mA */
    CHARGE_VOLTAGE_SET, /* int: 设置充电电压, mV */
    SHUTDOWN,           /* bool: 关机 (写 true 触发, BMS 断开电池输出) */
    STANDBY,            /* bool: 强制待机 (写 true 触发, 充电IC进待机) */
    CHARGER_PRESENT,    /* bool: 通知 BMS 充电器接入状态 (0x2006) */

    /* 元数据 */
    MODEL_NAME,      /* string */
    VERSION,         /* string */
    SERIAL_NUMBER,   /* string */
};

/* 属性值类型 */
struct PowerValue {
    enum Type { INT, BOOL, STRING } type;

    union {
        int64_t  int_val;
        bool     bool_val;
    };
    std::string str_val;

    PowerValue() : type(INT), int_val(0) {}
    explicit PowerValue(int64_t v) : type(INT), int_val(v) {}
    explicit PowerValue(bool v) : type(BOOL), bool_val(v) {}
    PowerValue(const char* s) : type(STRING), str_val(s) {}
    PowerValue(const std::string& s) : type(STRING), str_val(s) {}

    int64_t asInt() const { return int_val; }
    bool    asBool() const { return bool_val; }
    const std::string& asStr() const { return str_val; }
};

} /* namespace stark_power_manager */
