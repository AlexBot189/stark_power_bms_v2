/*
 * IP2366Source.cpp — IP2366 快充芯片驱动实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "IP2366Source.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/ioctl.h>

#include <linux/i2c-dev.h>
#include <gpiod.h>

#include <log_helper/LogHelper.h>

namespace stark_power_manager {

/* ================================================================
 * 构造/析构
 * ================================================================ */

IP2366Source::IP2366Source(const Config& cfg)
    : m_cfg(cfg)
{
}

IP2366Source::~IP2366Source()
{
    /* 停止 INT 线程 */
    m_int_running = false;
    if (m_int_thread.joinable()) {
        m_int_thread.join();
    }

    /* 释放 GPIO */
    releaseGpio();

    /* 关闭 I2C */
    if (m_i2c_fd >= 0) {
        close(m_i2c_fd);
        m_i2c_fd = -1;
    }

    ECO_INFO("[IP2366] driver destroyed");
}

/* ================================================================
 * IPowerSource 接口 — 属性列表
 * ================================================================ */

std::vector<PowerProp> IP2366Source::supportedProps() const
{
    return {
        PowerProp::STATUS,
        PowerProp::ONLINE,
        PowerProp::HEALTH,
        PowerProp::CHARGE_TYPE,
        PowerProp::FAULT,
        PowerProp::FAULT_REASON,
        PowerProp::VOLTAGE_NOW,
        PowerProp::CURRENT_NOW,
        PowerProp::CHARGE_ENABLE,
        PowerProp::CHARGE_CURRENT_SET,
        PowerProp::MODEL_NAME,
    };
}

/* ================================================================
 * IPowerSource 接口 — 读属性
 * ================================================================ */

bool IP2366Source::getProp(PowerProp prop, PowerValue& out)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (prop) {
    case PowerProp::STATUS: {
        if (m_fault) {
            out = PowerValue("fault");
        } else if (m_chg_state == CHG_FULL || m_chg_end) {
            out = PowerValue("full");
        } else if (m_chg_state == CHG_CC || m_chg_state == CHG_CV
                   || m_chg_state == CHG_TRICKLE) {
            out = PowerValue("charging");
        } else if (m_chg_state == CHG_TIMEOUT) {
            out = PowerValue("fault");
        } else {
            out = PowerValue("idle");
        }
        return true;
    }

    case PowerProp::ONLINE:
        out = PowerValue(m_vbus_ok);
        return true;

    case PowerProp::HEALTH:
        if (m_fault_code & BIT_VSYS_OC) {
            out = PowerValue("overcurrent");
        } else if (m_fault_code & BIT_VSYS_SCDT) {
            out = PowerValue("dead");
        } else if (m_chg_state == CHG_TIMEOUT) {
            out = PowerValue("overheat");
        } else {
            out = PowerValue("good");
        }
        return true;

    case PowerProp::CHARGE_TYPE: {
        switch (m_chg_state) {
        case CHG_TRICKLE: out = PowerValue("trickle"); break;
        case CHG_CC:      out = PowerValue("cc");      break;
        case CHG_CV:      out = PowerValue("cv");      break;
        default:          out = PowerValue("none");     break;
        }
        return true;
    }

    case PowerProp::FAULT:
        out = PowerValue(m_fault);
        return true;

    case PowerProp::FAULT_REASON: {
        std::string reason;
        if (m_fault_code & BIT_VSYS_OC)   reason += "vsys_oc ";
        if (m_fault_code & BIT_VSYS_SCDT) reason += "vsys_short ";
        if (m_fault_code & BIT_EN_INT_LOW) reason += "chip_int ";
        if (m_chg_state == CHG_TIMEOUT)   reason += "timeout ";
        if (reason.empty()) reason = "none";
        else reason.pop_back();
        out = PowerValue(reason);
        return true;
    }

    case PowerProp::VOLTAGE_NOW:
        out = PowerValue(static_cast<int64_t>(m_batt_voltage_mv));
        return true;

    case PowerProp::CURRENT_NOW:
        out = PowerValue(static_cast<int64_t>(m_batt_current_ma));
        return true;

    case PowerProp::CHARGE_ENABLE: {
        /* 同时检查 I2C 充电使能位和 CHARGE_EN GPIO */
        bool hw_en = false;
        if (m_charge_en_line) {
            int val = gpiod_line_get_value(m_charge_en_line);
            hw_en = (val == 1);
        }
        out = PowerValue(m_chg_active && hw_en);
        return true;
    }

    case PowerProp::CHARGE_CURRENT_SET:
        out = PowerValue(static_cast<int64_t>(m_cfg.charge_current_ma));
        return true;

    case PowerProp::MODEL_NAME:
        out = PowerValue("IP2366");
        return true;

    default:
        return false;
    }
}

/* ================================================================
 * IPowerSource 接口 — 写属性
 * ================================================================ */

bool IP2366Source::setProp(PowerProp prop, const PowerValue& val)
{
    switch (prop) {
    case PowerProp::CHARGE_ENABLE: {
        bool en = val.asBool();

        /* 控制 CHARGE_EN GPIO */
        if (m_charge_en_line) {
            int ret = gpiod_line_set_value(m_charge_en_line, en ? 1 : 0);
            if (ret < 0) {
                ECO_ERROR("[IP2366] set CHARGE_EN GPIO failed: %s",
                          strerror(errno));
                return false;
            }
        }

        /* 控制 IP2366 内部充电使能位 */
        if (!rmwReg(REG_SYS_CTL0, BIT_CHG_EN, en ? BIT_CHG_EN : 0)) {
            ECO_ERROR("[IP2366] set CHG_EN bit failed");
            return false;
        }

        ECO_INFO("[IP2366] charge enable: %s", en ? "ON" : "OFF");
        return true;
    }

    case PowerProp::CHARGE_CURRENT_SET: {
        int64_t ma = val.asInt();
        if (ma < 0) ma = 0;
        if (ma > 25500) ma = 25500; /* IP2366 max 255*100mA */

        uint8_t iset = static_cast<uint8_t>(ma / 100);
        if (iset == 0) iset = 1;

        if (!writeReg(REG_SYS_CTL3, iset)) {
            ECO_ERROR("[IP2366] set charge current failed");
            return false;
        }

        m_cfg.charge_current_ma = static_cast<uint16_t>(iset * 100);
        ECO_INFO("[IP2366] charge current set: %u mA", m_cfg.charge_current_ma);
        return true;
    }

    default:
        return false;
    }
}

/* ================================================================
 * 订阅
 * ================================================================ */

void IP2366Source::subscribe(ChangeCallback cb)
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    m_callbacks.push_back(std::move(cb));
}

/* ================================================================
 * 初始化
 * ================================================================ */

bool IP2366Source::initialize()
{
    /* 1. 打开 I2C */
    m_i2c_fd = open(m_cfg.i2c_dev.c_str(), O_RDWR);
    if (m_i2c_fd < 0) {
        ECO_ERROR("[IP2366] open I2C %s failed: %s",
                  m_cfg.i2c_dev.c_str(), strerror(errno));
        return false;
    }

    if (ioctl(m_i2c_fd, I2C_SLAVE, m_cfg.addr) < 0) {
        ECO_ERROR("[IP2366] ioctl I2C_SLAVE 0x%02X failed: %s",
                  m_cfg.addr, strerror(errno));
        close(m_i2c_fd);
        m_i2c_fd = -1;
        return false;
    }

    ECO_INFO("[IP2366] I2C opened: %s addr=0x%02X",
             m_cfg.i2c_dev.c_str(), m_cfg.addr);

    /* 2. 验证 I2C 通信 (读一次充电状态寄存器) */
    uint8_t test_val = 0;
    if (!readReg(REG_STATE_CTL0, test_val)) {
        ECO_WARN("[IP2366] first I2C read failed, chip may be in sleep");
        /* 不 fatal, 等 INT 唤醒后重试 */
    } else {
        ECO_INFO("[IP2366] chip detected, STATE_CTL0=0x%02X", test_val);
    }

    /* 3. 初始化 GPIO */
    if (!initGpio()) {
        ECO_ERROR("[IP2366] GPIO init failed");
        close(m_i2c_fd);
        m_i2c_fd = -1;
        return false;
    }

    /* 4. 初始化充电参数 (仅在 I2C 已就绪时) */
    if (test_val != 0) {
        if (!initChargeParams()) {
            ECO_WARN("[IP2366] initChargeParams failed, will retry on wake");
        }
    }

    /* 5. 启动 INT 线程 */
    m_int_running = true;
    m_int_thread = std::thread(&IP2366Source::intThreadFunc, this);

    ECO_INFO("[IP2366] initialized successfully");
    return true;
}

/* ================================================================
 * GPIO 初始化
 * ================================================================ */

bool IP2366Source::initGpio()
{
    /* INT GPIO (输入, 双边沿) */
    m_int_chip = gpiod_chip_open(m_cfg.int_gpio_chip.c_str());
    if (!m_int_chip) {
        ECO_ERROR("[IP2366] open INT gpiochip %s failed: %s",
                  m_cfg.int_gpio_chip.c_str(), strerror(errno));
        return false;
    }

    m_int_line = gpiod_chip_get_line(m_int_chip, m_cfg.int_gpio_line);
    if (!m_int_line) {
        ECO_ERROR("[IP2366] get INT line %d failed: %s",
                  m_cfg.int_gpio_line, strerror(errno));
        return false;
    }

    if (gpiod_line_request_both_edges_events(m_int_line, "ip2366_int") < 0) {
        ECO_ERROR("[IP2366] request INT both-edges failed: %s",
                  strerror(errno));
        return false;
    }

    ECO_INFO("[IP2366] INT GPIO: %s line %d (both edges)",
             m_cfg.int_gpio_chip.c_str(), m_cfg.int_gpio_line);

    /* CHARGE_EN GPIO (输出, 初始低) */
    m_charge_en_chip = gpiod_chip_open(m_cfg.charge_en_gpio_chip.c_str());
    if (!m_charge_en_chip) {
        ECO_ERROR("[IP2366] open CHARGE_EN gpiochip %s failed: %s",
                  m_cfg.charge_en_gpio_chip.c_str(), strerror(errno));
        return false;
    }

    m_charge_en_line = gpiod_chip_get_line(m_charge_en_chip,
                                           m_cfg.charge_en_gpio_line);
    if (!m_charge_en_line) {
        ECO_ERROR("[IP2366] get CHARGE_EN line %d failed: %s",
                  m_cfg.charge_en_gpio_line, strerror(errno));
        return false;
    }

    if (gpiod_line_request_output(m_charge_en_line, "ip2366_chg_en", 0) < 0) {
        ECO_ERROR("[IP2366] request CHARGE_EN output failed: %s",
                  strerror(errno));
        return false;
    }

    ECO_INFO("[IP2366] CHARGE_EN GPIO: %s line %d (output, init LOW)",
             m_cfg.charge_en_gpio_chip.c_str(), m_cfg.charge_en_gpio_line);

    m_gpio_ready = true;
    return true;
}

void IP2366Source::releaseGpio()
{
    m_gpio_ready = false;

    if (m_charge_en_line) {
        gpiod_line_set_value(m_charge_en_line, 0);
        gpiod_line_release(m_charge_en_line);
        m_charge_en_line = nullptr;
    }
    if (m_charge_en_chip) {
        gpiod_chip_close(m_charge_en_chip);
        m_charge_en_chip = nullptr;
    }

    if (m_int_line) {
        gpiod_line_release(m_int_line);
        m_int_line = nullptr;
    }
    if (m_int_chip) {
        gpiod_chip_close(m_int_chip);
        m_int_chip = nullptr;
    }
}

/* ================================================================
 * I2C 读写
 * ================================================================ */

bool IP2366Source::readReg(uint8_t reg, uint8_t& val)
{
    if (m_i2c_fd < 0) {
        return false;
    }

    int32_t ret = i2c_smbus_read_byte_data(m_i2c_fd, reg);
    if (ret < 0) {
        ECO_WARN("[IP2366] I2C read reg 0x%02X failed: %s",
                 reg, strerror(errno));
        return false;
    }

    val = static_cast<uint8_t>(ret);
    return true;
}

bool IP2366Source::writeReg(uint8_t reg, uint8_t val)
{
    if (m_i2c_fd < 0) {
        return false;
    }

    int ret = i2c_smbus_write_byte_data(m_i2c_fd, reg, val);
    if (ret < 0) {
        ECO_WARN("[IP2366] I2C write reg 0x%02X=0x%02X failed: %s",
                 reg, val, strerror(errno));
        return false;
    }

    return true;
}

bool IP2366Source::rmwReg(uint8_t reg, uint8_t mask, uint8_t bits)
{
    uint8_t val;
    if (!readReg(reg, val)) {
        return false;
    }

    uint8_t new_val = (val & ~mask) | (bits & mask);
    if (new_val == val) {
        return true; /* 无需修改 */
    }

    return writeReg(reg, new_val);
}

/* ================================================================
 * ADC 读取 (关键约束: 必须先读低字节再读高字节)
 * ================================================================ */

uint16_t IP2366Source::readADC16(uint8_t reg_low, uint8_t reg_high)
{
    uint8_t lo = 0, hi = 0;

    /*
     * 严格先低后高:
     * 读低字节触发硬件锁存器更新, 颠倒顺序会读到错误数据。
     */
    if (!readReg(reg_low, lo)) {
        return 0;
    }
    if (!readReg(reg_high, hi)) {
        return 0;
    }

    return (static_cast<uint16_t>(hi) << 8) | lo;
}

/* ================================================================
 * 充电参数初始化
 * ================================================================ */

bool IP2366Source::initChargeParams()
{
    ECO_INFO("[IP2366] initializing charge params...");

    /* PDO 档位选择 */
    if (!rmwReg(REG_SELECT_PDO, MASK_PDO_SELECT, m_cfg.pdo_select)) {
        ECO_ERROR("[IP2366] set PDO select failed");
        return false;
    }
    ECO_INFO("[IP2366] PDO select: %u", m_cfg.pdo_select);

    /* 单节充满电压 (0x02): Vset = (V - 2500) / 10 */
    uint16_t vs = m_cfg.charge_voltage_mv;
    if (vs < 2500) vs = 2500;
    if (vs > 4450) vs = 4450;
    uint8_t vset = static_cast<uint8_t>((vs - 2500) / 10);
    if (!writeReg(REG_SYS_CTL2, vset)) {
        ECO_ERROR("[IP2366] set charge voltage failed");
        return false;
    }
    ECO_INFO("[IP2366] charge voltage: %u mV (reg=0x%02X)", vs, vset);

    /* 充电电流 (0x03): Iset = N * 100mA */
    uint16_t cs = m_cfg.charge_current_ma;
    uint8_t iset = static_cast<uint8_t>(cs / 100);
    if (iset == 0) iset = 1;
    if (iset > 255) iset = 255;
    if (!writeReg(REG_SYS_CTL3, iset)) {
        ECO_ERROR("[IP2366] set charge current failed");
        return false;
    }
    ECO_INFO("[IP2366] charge current: %u mA (reg=0x%02X)",
             static_cast<unsigned>(iset * 100), iset);

    /* 涓流电流 (0x06): N * 50mA */
    uint8_t tk = static_cast<uint8_t>(m_cfg.trickle_current_ma / 50);
    if (tk == 0) tk = 1;
    if (!writeReg(REG_SYS_CTL6, tk)) {
        ECO_ERROR("[IP2366] set trickle current failed");
        return false;
    }
    ECO_INFO("[IP2366] trickle current: %u mA (reg=0x%02X)",
             static_cast<unsigned>(tk * 50), tk);

    /* 停充电流 (0x08[7:4]): N * 50mA */
    uint8_t istop = static_cast<uint8_t>(m_cfg.stop_current_ma / 50);
    if (istop == 0) istop = 1;
    if (istop > 15) istop = 15;
    if (!rmwReg(REG_SYS_CTL8, MASK_STOP_CURRENT,
                static_cast<uint8_t>(istop << 4))) {
        ECO_ERROR("[IP2366] set stop current failed");
        return false;
    }
    ECO_INFO("[IP2366] stop current: %u mA", static_cast<unsigned>(istop * 50));

    /* 使能 INT 异常通知 (0x00 bit5): 故障时 INT 拉低 2ms */
    if (!rmwReg(REG_SYS_CTL0, BIT_EN_INT_LOW, BIT_EN_INT_LOW)) {
        ECO_ERROR("[IP2366] enable INT low failed");
        return false;
    }
    ECO_INFO("[IP2366] INT fault notification enabled");

    /* 禁止待机 (0x09 bit7: 待机使能 = 0) */
    if (!rmwReg(REG_SYS_CTL9, BIT_STANDBY_EN, 0)) {
        ECO_WARN("[IP2366] disable standby failed");
    } else {
        ECO_INFO("[IP2366] standby disabled");
    }

    ECO_INFO("[IP2366] charge params initialized");
    return true;
}

/* ================================================================
 * 读取充电状态
 * ================================================================ */

void IP2366Source::readChargeState()
{
    uint8_t s0 = 0, s2 = 0, s3 = 0, ts = 0;

    if (!readReg(REG_STATE_CTL0, s0)) {
        ECO_WARN("[IP2366] read STATE_CTL0 failed");
        return;
    }
    if (!readReg(REG_STATE_CTL2, s2)) {
        ECO_WARN("[IP2366] read STATE_CTL2 failed");
        return;
    }
    if (!readReg(REG_STATE_CTL3, s3)) {
        ECO_WARN("[IP2366] read STATE_CTL3 failed");
        return;
    }
    /* 0x34: TypeC 连接状态 (Sink/Src/PD 标志位) */
    if (!readReg(REG_TYPEC_STATE, ts)) {
        ECO_WARN("[IP2366] read TYPEC_STATE failed");
        ts = 0;
    }

    /* ADC (先低后高) */
    uint16_t batt_mv  = readADC16(REG_BATVADC_L, REG_BATVADC_H);
    uint16_t vsys_mv  = readADC16(REG_VSYSADC_L, REG_VSYSADC_H);
    uint16_t batt_ma  = readADC16(REG_BATIADC_L, REG_BATIADC_H);
    uint16_t sys_ma   = readADC16(REG_SYSIADC_L, REG_SYSIADC_H);

    /* 检测变化 */
    bool state_changed = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        uint8_t new_state  = s0 & MASK_CHG_STATE;
        bool    new_active = (s0 & BIT_CHG_ACTIVE) != 0;
        bool    new_end    = (s0 & BIT_CHG_END) != 0;
        bool    new_vbus   = (s2 & BIT_VBUS_OK) != 0;
        bool    new_sink   = (ts & BIT_SINK_OK) != 0;
        bool    new_sinkpd = (ts & BIT_SINK_PD_OK) != 0;

        bool new_fault = (s3 != 0);
        if (new_fault && s3 == m_fault_code) {
            new_fault = m_fault; /* 保持已有故障状态 */
        }

        state_changed = (new_state  != m_chg_state)
                     || (new_active != m_chg_active)
                     || (new_end    != m_chg_end)
                     || (new_vbus   != m_vbus_ok)
                     || (new_sink   != m_sink_ok)
                     || (new_sinkpd != m_sink_pd_ok)
                     || (new_fault  != m_fault);

        m_chg_state  = new_state;
        m_chg_active = new_active;
        m_chg_end    = new_end;
        m_vbus_ok    = new_vbus;
        m_sink_ok    = new_sink;
        m_sink_pd_ok = new_sinkpd;
        m_fault      = new_fault;
        m_fault_code = s3;

        m_batt_voltage_mv = batt_mv;
        m_vsys_voltage_mv = vsys_mv;
        m_batt_current_ma = batt_ma;
        m_sys_current_ma  = sys_ma;
    }

    if (state_changed) {
        ECO_INFO("[IP2366] state: chg=0x%02X active=%d end=%d vbus=%d "
                 "fault=0x%02X batt=%umV %umA",
                 m_chg_state, m_chg_active, m_chg_end, m_vbus_ok,
                 m_fault_code, m_batt_voltage_mv, m_batt_current_ma);

        notifyChange(PowerProp::STATUS, PowerValue("changed"));
    }
}

/* ================================================================
 * INT 线程
 *
 * IP2366 INT 双向协议:
 *   - 上升沿 (唤醒): 芯片从休眠唤醒, 拉高 INT 100ms 后 I2C 就绪
 *   - 下降沿 (休眠/故障): 芯片即将休眠或发生故障,
 *     16ms 内主机必须停止 I2C 通信
 * ================================================================ */

void IP2366Source::intThreadFunc()
{
    ECO_INFO("[IP2366] INT thread started");

    struct timespec timeout;
    timeout.tv_sec  = 0;
    timeout.tv_nsec = m_cfg.int_poll_interval_ms * 1000000L;

    while (m_int_running) {
        int ret = gpiod_line_event_wait(m_int_line, &timeout);
        if (ret < 0) {
            ECO_ERROR("[IP2366] INT event_wait error: %s", strerror(errno));
            break;
        }
        if (ret == 0) {
            continue; /* 超时, 继续等待 */
        }

        struct gpiod_line_event event;
        if (gpiod_line_event_read(m_int_line, &event) < 0) {
            ECO_ERROR("[IP2366] INT event_read error: %s", strerror(errno));
            continue;
        }

        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
            /*
             * 上升沿: 芯片唤醒
             * 硬件要求: INT 拉高后等 100ms I2C 才就绪
             */
            ECO_INFO("[IP2366] INT rising (wakeup), waiting 100ms...");
            usleep(100000); /* 100ms */

            if (!m_int_running) break;

            /* 如果 I2C 未就绪, 重新打开 */
            if (m_i2c_fd < 0) {
                m_i2c_fd = open(m_cfg.i2c_dev.c_str(), O_RDWR);
                if (m_i2c_fd >= 0) {
                    ioctl(m_i2c_fd, I2C_SLAVE, m_cfg.addr);
                }
            }

            /* 初始化充电参数 (如果之前失败) */
            if (m_i2c_fd >= 0) {
                uint8_t test;
                if (readReg(REG_STATE_CTL0, test)) {
                    initChargeParams();
                }
            }

            /* 读取当前状态 */
            readChargeState();

        } else if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
            /*
             * 下降沿: 芯片休眠或故障
             * 硬件要求: 16ms 内必须停止 I2C
             * 这里不做 I2C 操作即为满足约束
             */
            ECO_INFO("[IP2366] INT falling (sleep/fault)");

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_fault = true;
                m_vbus_ok = false;
            }

            notifyChange(PowerProp::STATUS, PowerValue("fault"));
            notifyChange(PowerProp::ONLINE, PowerValue(false));
        }
    }

    ECO_INFO("[IP2366] INT thread stopped");
}

/* ================================================================
 * 通知订阅者
 * ================================================================ */

void IP2366Source::notifyChange(PowerProp prop, const PowerValue& val)
{
    std::lock_guard<std::mutex> lock(m_cb_mutex);
    for (auto& cb : m_callbacks) {
        if (cb) {
            cb(prop, val);
        }
    }
}

} /* namespace stark_power_manager */
