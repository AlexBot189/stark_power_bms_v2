/*
 * IP2366Source.h — IP2366 快充芯片驱动
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 实现 IPowerSource 接口, 封装 IP2366 I2C 通信和 INT 中断处理。
 *
 * 硬件连接:
 *   - I2C: /dev/i2c-2, 从机地址 0x75 (7-bit)
 *   - INT: GPIO0_C7 (双向: 唤醒通知 + 故障通知)
 *   - CHARGE_EN: GPIO2_A4 (硬件充电通路开关)
 *
 * INT 协议:
 *   - 唤醒: INT 拉高 100ms 后芯片就绪, 主机可发起 I2C
 *   - 休眠/故障: INT 拉低, 16ms 内主机必须停止 I2C
 *
 * IP2366 硬件自动管理充电曲线 (涓流 -> CC -> CV -> 充满),
 * SOC 层只需监控状态 + 干预 (使能/禁用/调电流)。
 */
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "power/IPowerSource.h"
#include "IP2366Reg.h"

/* 前向声明 */
struct gpiod_chip;
struct gpiod_line;

namespace stark_power_manager {

class IP2366Source : public IPowerSource {
public:
    struct Config {
        /* I2C */
        std::string i2c_dev = "/dev/i2c-2";
        uint8_t     addr    = IP2366_ADDR;

        /* GPIO */
        std::string int_gpio_chip       = "gpiochip0";
        int         int_gpio_line       = 39;   /* GPIO0_C7 */
        std::string charge_en_gpio_chip = "gpiochip2";
        int         charge_en_gpio_line = 4;    /* GPIO2_A4 */

        /* 充电参数 */
        uint8_t  pdo_select         = CONST_PDO_20V;  /* 5=20V */
        uint16_t charge_voltage_mv  = 4200;   /* 单节充满电压 mV */
        uint16_t charge_current_ma  = 3000;   /* 充电电流 mA */
        uint16_t trickle_current_ma = 200;    /* 涓流电流 mA */
        uint16_t stop_current_ma    = 100;    /* 停充电流 mA */

        /* INT 线程轮询间隔 ms */
        uint32_t int_poll_interval_ms = 100;
    };

    explicit IP2366Source(const Config& cfg);
    ~IP2366Source() override;

    /* 禁止拷贝 */
    IP2366Source(const IP2366Source&) = delete;
    IP2366Source& operator=(const IP2366Source&) = delete;

    /* IPowerSource 接口 */
    const char* name() const override { return "charger_ip2366"; }
    const char* type() const override { return "charger"; }

    std::vector<PowerProp> supportedProps() const override;
    bool getProp(PowerProp prop, PowerValue& out) override;
    bool setProp(PowerProp prop, const PowerValue& val) override;
    void subscribe(ChangeCallback cb) override;

    /*
     * 初始化: 打开 I2C + GPIO + 初始化充电参数 + 启动 INT 线程
     * 返回 true 表示成功, false 表示失败 (上层决定重试或降级)
     */
    bool initialize();

private:
    /* I2C 读写 */
    bool readReg(uint8_t reg, uint8_t& val);
    bool writeReg(uint8_t reg, uint8_t val);

    /* 读 16 位 ADC (严格先低后高, 读低触发锁存更新)
     * 返回 false 表示读取失败; 调用方应保留旧值而非当作 0 */
    bool readADC16(uint8_t reg_low, uint8_t reg_high, uint16_t& out);

    /* 读-修改-写: 读 reg -> 只改 mask 位 -> 写回 */
    bool rmwReg(uint8_t reg, uint8_t mask, uint8_t bits);

    /* 读取全部充电状态 (0x31/0x32/0x33/0x34/0x38 + ADC) */
    void readChargeState();

    /* GPIO 操作 */
    bool initGpio();
    void releaseGpio();

    /* INT 线程 */
    void intThreadFunc();

    /* 充电参数写入 (initialize 中调用) */
    bool initChargeParams();

    /* 通知所有订阅者 */
    void notifyChange(PowerProp prop, const PowerValue& val);

    /* ---- 配置 ---- */
    Config m_cfg;

    /* ---- I2C ---- */
    int m_i2c_fd = -1;

    /* ---- GPIO ---- */
    gpiod_chip* m_int_chip     = nullptr;
    gpiod_line* m_int_line     = nullptr;
    gpiod_chip* m_charge_en_chip = nullptr;
    gpiod_line* m_charge_en_line = nullptr;
    bool m_gpio_ready = false;

    /* ---- I2C 访问串行化 (INT 线程 readChargeState 与 setProp 并发保护) ---- */
    std::mutex m_i2c_mutex;

    /* ---- 状态缓存 (mutex 保护) ---- */
    mutable std::mutex m_mutex;
    uint8_t  m_chg_state     = CHG_STANDBY;
    bool     m_chg_active    = false;
    bool     m_chg_end       = false;
    bool     m_vbus_ok       = false;
    bool     m_sink_ok       = false;
    bool     m_sink_pd_ok    = false;
    bool     m_fault         = false;
    uint8_t  m_fault_code    = 0;
    uint16_t m_batt_voltage_mv = 0;
    uint16_t m_vsys_voltage_mv = 0;
    uint16_t m_batt_current_ma = 0;
    uint16_t m_sys_current_ma  = 0;

    /* ---- INT 线程 ---- */
    std::thread m_int_thread;
    std::atomic<bool> m_int_running{false};

    /* ---- 变化回调 ---- */
    mutable std::mutex m_cb_mutex;
    std::vector<ChangeCallback> m_callbacks;
};

} /* namespace stark_power_manager */
