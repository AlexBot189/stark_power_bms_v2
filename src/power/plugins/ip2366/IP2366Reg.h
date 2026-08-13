/*
 * IP2366Reg.h — IP2366 快充芯片寄存器定义
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 英集芯 IP2366: 6S 锂电快充管理芯片
 * I2C 从机地址 0x75 (7-bit), 支持 PD/QC 快充协议输入。
 *
 * 关键约束:
 *   1. 16位 ADC 必须先读低字节再读高字节, 读低触发硬件锁存更新
 *   2. 寄存器操作必须读-修改-写, 不能直接写新值
 *   3. INT 双向协议: 唤醒后拉高 100ms 才能 I2C, 变低后 16ms 内必须停止 I2C
 */
#pragma once

#include <cstdint>

namespace stark_power_manager {

/* I2C 从机地址 (7-bit; i2cdetect 显示 0x75; 数据手册 0xEA = 0x75<<1 是 8 位写地址) */
constexpr uint8_t IP2366_ADDR = 0x75;

/* ================================================================
 * 控制寄存器 (读写)
 * ================================================================ */

/* 0x00: 系统控制 0 — 充电使能 + 快充协议选择 */
constexpr uint8_t REG_SYS_CTL0 = 0x00;
/* bit0: 充电使能 */
constexpr uint8_t BIT_CHG_EN = 0x01;
/* bit5: 异常 INT 通知使能 */
constexpr uint8_t BIT_EN_INT_LOW = 0x20;
/* [4:1]: 快充协议选择 */
constexpr uint8_t MASK_FAST_CHG = 0x1E;
constexpr uint8_t FAST_CHG_PD = 0x02;   /* PD 快充 */

/* 0x01: 系统控制 1 — VBUS/VOUT MOS 控制 */
constexpr uint8_t REG_SYS_CTL1 = 0x01;
constexpr uint8_t BIT_VBUS_MOS_ON = 0x01;  /* bit0: VBUS MOS 导通 */
constexpr uint8_t BIT_VOUT_MOS_ON = 0x02;  /* bit1: VOUT MOS 导通 */

/* 0x02: 系统控制 2 — 单节充满电压 */
constexpr uint8_t REG_SYS_CTL2 = 0x02;
/* Vset = N * 10mV + 2500mV, 例: 4200mV -> N=(4200-2500)/10=170 */

/* 0x03: 系统控制 3 — 充电电流 */
constexpr uint8_t REG_SYS_CTL3 = 0x03;
/* Iset = N * 100mA, 例: 3000mA -> N=30 */

/* 0x06: 系统控制 6 — 涓流电流 */
constexpr uint8_t REG_SYS_CTL6 = 0x06;
/* Itrickle = N * 50mA */

/* 0x08: 系统控制 8 — 停充电流 + 再充电阈值 */
constexpr uint8_t REG_SYS_CTL8 = 0x08;
/* [7:4]: 停充电流 = N * 50mA */
constexpr uint8_t MASK_STOP_CURRENT = 0xF0;
/* [3:0]: 再充电阈值 */
constexpr uint8_t MASK_RECHARGE_THRES = 0x0F;

/* 0x09: 系统控制 9 — 待机使能和低电电压设置 */
constexpr uint8_t REG_SYS_CTL9 = 0x09;
constexpr uint8_t BIT_STANDBY_EN = 0x80;    /* bit7: 待机使能 */
constexpr uint8_t BIT_STANDBY_ENTER = 0x40; /* bit6: 写 1 进待机(单次有效, 需 bit7=1) */

/* 0x0D: PDO 档位选择 */
constexpr uint8_t REG_SELECT_PDO = 0x0D;
/* [2:0]: PDO 档位 */
constexpr uint8_t MASK_PDO_SELECT = 0x07;
constexpr uint8_t CONST_PDO_MAX  = 0;
constexpr uint8_t CONST_PDO_5V   = 1;
constexpr uint8_t CONST_PDO_9V   = 2;
constexpr uint8_t CONST_PDO_12V  = 3;
constexpr uint8_t CONST_PDO_15V  = 4;
constexpr uint8_t CONST_PDO_20V  = 5;

/* ================================================================
 * 状态寄存器 (只读)
 * ================================================================ */

/* 0x31: 充电状态 + 充电阶段 */
constexpr uint8_t REG_STATE_CTL0 = 0x31;
/* bit5: 充电进行中 */
constexpr uint8_t BIT_CHG_ACTIVE = 0x20;
/* bit4: 充满 */
constexpr uint8_t BIT_CHG_END = 0x10;
/* [2:0]: 充电阶段 */
constexpr uint8_t MASK_CHG_STATE = 0x07;

/* 充电阶段枚举 (STATE_CTL0[2:0]) */
constexpr uint8_t CHG_STANDBY = 0; /* 待机 */
constexpr uint8_t CHG_TRICKLE = 1; /* 涓流 */
constexpr uint8_t CHG_CC      = 2; /* 恒流 */
constexpr uint8_t CHG_CV      = 3; /* 恒压 */
constexpr uint8_t CHG_WAITING = 4; /* 等待 */
constexpr uint8_t CHG_FULL    = 5; /* 充满 */
constexpr uint8_t CHG_TIMEOUT = 6; /* 超时 */

/* 0x32: MOS 状态 */
constexpr uint8_t REG_STATE_CTL1 = 0x32;
/* bit0: VBUS MOS 状态 */
constexpr uint8_t BIT_VBUS_MOS_STAT = 0x01;
/* bit1: VOUT MOS 状态 */
constexpr uint8_t BIT_VOUT_MOS_STAT = 0x02;

/* 0x33: VBUS 状态 + 输入电压标志 */
constexpr uint8_t REG_STATE_CTL2 = 0x33;
constexpr uint8_t BIT_VBUS_OK = 0x80;    /* bit7: Vbus_Ok, 1=有电 */
constexpr uint8_t BIT_VBUS_OV = 0x40;    /* bit6: Vbus_Ov, 1=输入过压 */

/* 0x34: TypeC 连接状态 */
constexpr uint8_t REG_TYPEC_STATE = 0x34;
constexpr uint8_t BIT_SINK_OK    = 0x80;  /* bit7: Sink 输入连接有效 */
constexpr uint8_t BIT_SRC_OK     = 0x40;  /* bit6: Src 输出连接有效 */
constexpr uint8_t BIT_SRC_PD_OK  = 0x20;  /* bit5: Src PD 协商完成 */
constexpr uint8_t BIT_SINK_PD_OK = 0x10;  /* bit4: Sink PD 协商完成 */

/* 0x35: 适配器 PDO 能力 */
constexpr uint8_t REG_RECEIVED_PDO = 0x35;

/* 0x38: 过流/短路标志 */
constexpr uint8_t REG_STATE_CTL3 = 0x38;
constexpr uint8_t BIT_VSYS_OC   = 0x20; /* bit5: Vsys 输出过流 */
constexpr uint8_t BIT_VSYS_SCDT = 0x10; /* bit4: Vsys 输出短路 */

/* ================================================================
 * ADC 寄存器 (16位, 先读低字节再读高字节)
 * ================================================================ */

constexpr uint8_t REG_BATVADC_L = 0x50;
constexpr uint8_t REG_BATVADC_H = 0x51; /* 电池电压, mV */

constexpr uint8_t REG_VSYSADC_L = 0x52;
constexpr uint8_t REG_VSYSADC_H = 0x53; /* 系统电压, mV */

constexpr uint8_t REG_BATIADC_L = 0x6E;
constexpr uint8_t REG_BATIADC_H = 0x6F; /* 电池电流, mA */

constexpr uint8_t REG_SYSIADC_L = 0x70;
constexpr uint8_t REG_SYSIADC_H = 0x71; /* 系统电流, mA */

} /* namespace stark_power_manager */
