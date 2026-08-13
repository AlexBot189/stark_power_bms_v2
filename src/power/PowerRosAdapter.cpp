/*
 * PowerRosAdapter.cpp — 电源管理 ROS 接口适配器实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "PowerRosAdapter.h"

#include <log_helper/LogHelper.h>
#include "power/PowerRegistry.h"

namespace stark_power_manager {

/* ================================================================
 * 构造 / 初始化
 * ================================================================ */

PowerRosAdapter::PowerRosAdapter(std::shared_ptr<ros::NodeHandle> nh,
                                 std::shared_ptr<PowerManager> mgr)
    : m_nh(std::move(nh)), m_mgr(std::move(mgr))
{
}

void PowerRosAdapter::Init()
{
    m_chargeStatePub = m_nh->advertise<stark_msgs::ChargeState>(
        "/stark/charge_state", 10);
    m_powerStatePub = m_nh->advertise<stark_msgs::PowerState>(
        "/stark/power_state", 10);
    m_powerCtrlSub = m_nh->subscribe<stark_msgs::PowerCtrl>(
        "/stark/power_ctrl", 10, &PowerRosAdapter::OnPowerCtrl, this);

    /* 1Hz 周期发布汇总状态 */
    m_timer = m_nh->createTimer(ros::Duration(1.0),
        &PowerRosAdapter::PublishPeriodic, this);

    ECO_INFO("[PowerRosAdapter] initialized: /stark/power_ctrl -> charge/power_state");
}

/* ================================================================
 * 控制指令处理
 * ================================================================ */

void PowerRosAdapter::OnPowerCtrl(const stark_msgs::PowerCtrl::ConstPtr& msg)
{
    auto& reg = PowerRegistry::instance();

    switch (msg->cmd) {
    case stark_msgs::PowerCtrl::CTRL_CHARGE: {
        /* 充电使能: 同时控制 IP2366 充电使能位 + BMS 充电 MOS */
        bool en = msg->enable;
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(en));
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(en));
        ECO_INFO("[PowerRosAdapter] CTRL_CHARGE enable=%d", en ? 1 : 0);
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_DISCHARGE: {
        bool en = msg->enable;
        reg.setProp("battery_bms", PowerProp::DISCHARGE_ENABLE, PowerValue(en));
        ECO_INFO("[PowerRosAdapter] CTRL_DISCHARGE enable=%d", en ? 1 : 0);
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_CURRENT: {
        int64_t ma = static_cast<int64_t>(msg->current_limit_ma);
        reg.setProp("charger_ip2366", PowerProp::CHARGE_CURRENT_SET, PowerValue(ma));
        ECO_INFO("[PowerRosAdapter] CTRL_CURRENT limit=%ld mA", (long)ma);
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_SHUTDOWN: {
        /* 关机: BMS 0x9001 断开电池输出 */
        ECO_WARN("[PowerRosAdapter] CTRL_SHUTDOWN received");
        reg.setProp("battery_bms", PowerProp::SHUTDOWN, PowerValue(true));
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_STANDBY: {
        /* 强制待机: IP2366 0x09 进待机 */
        ECO_WARN("[PowerRosAdapter] CTRL_STANDBY received");
        reg.setProp("charger_ip2366", PowerProp::STANDBY, PowerValue(true));
        break;
    }

    default:
        ECO_WARN("[PowerRosAdapter] unknown ctrl cmd=%u", msg->cmd);
        break;
    }
}

/* ================================================================
 * 周期发布
 * ================================================================ */

void PowerRosAdapter::PublishPeriodic(const ros::TimerEvent&)
{
    if (!m_mgr) {
        return;
    }

    stark_msgs::ChargeState cs;
    FillChargeState(cs);
    m_chargeStatePub.publish(cs);

    stark_msgs::PowerState ps;
    FillPowerState(ps);
    m_powerStatePub.publish(ps);
}

void PowerRosAdapter::FillChargeState(stark_msgs::ChargeState& msg)
{
    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "charge_state";

    msg.state = static_cast<uint8_t>(m_mgr->getState());

    auto& reg = PowerRegistry::instance();
    PowerValue v;

    if (reg.getProp("charger_ip2366", PowerProp::ONLINE, v)) {
        msg.adapter_online = v.asBool();
    }
    if (reg.getProp("battery_bms", PowerProp::VOLTAGE_NOW, v)) {
        msg.batt_voltage_mv = static_cast<uint16_t>(v.asInt());
    }
    if (reg.getProp("battery_bms", PowerProp::CURRENT_NOW, v)) {
        msg.batt_current_ma = static_cast<int32_t>(v.asInt());
    }
    if (reg.getProp("battery_bms", PowerProp::TEMPERATURE, v)) {
        msg.batt_temp_c = static_cast<int8_t>(v.asInt() / 10);
    }
    if (reg.getProp("battery_bms", PowerProp::CAPACITY, v)) {
        msg.batt_soc = static_cast<uint8_t>(v.asInt());
    }
    if (reg.getProp("battery_bms", PowerProp::FAULT_REASON, v)) {
        msg.fault_reason = v.asStr();
    } else if (reg.getProp("charger_ip2366", PowerProp::FAULT_REASON, v)) {
        msg.fault_reason = v.asStr();
    }
}

void PowerRosAdapter::FillPowerState(stark_msgs::PowerState& msg)
{
    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "power_state";

    msg.charge_state = static_cast<uint8_t>(m_mgr->getState());

    auto& reg = PowerRegistry::instance();
    PowerValue v;

    if (reg.getProp("charger_ip2366", PowerProp::ONLINE, v)) {
        msg.adapter_online = v.asBool();
    }

    /* 电池 */
    if (reg.getProp("battery_bms", PowerProp::VOLTAGE_NOW, v)) {
        msg.batt_voltage_mv = static_cast<uint16_t>(v.asInt());
    }
    if (reg.getProp("battery_bms", PowerProp::CURRENT_NOW, v)) {
        msg.batt_current_ma = static_cast<int32_t>(v.asInt());
    }
    if (reg.getProp("battery_bms", PowerProp::TEMPERATURE, v)) {
        msg.batt_temp_c = static_cast<int8_t>(v.asInt() / 10);
    }
    if (reg.getProp("battery_bms", PowerProp::CAPACITY, v)) {
        msg.batt_soc = static_cast<uint8_t>(v.asInt());
    }

    /* 充电器 */
    if (reg.getProp("charger_ip2366", PowerProp::CHARGE_TYPE, v)) {
        const auto& s = v.asStr();
        if (s == "trickle")      msg.charge_type = 1;
        else if (s == "cc")      msg.charge_type = 2;
        else if (s == "cv")      msg.charge_type = 3;
        else                     msg.charge_type = 0;
    }
    if (reg.getProp("charger_ip2366", PowerProp::CURRENT_NOW, v)) {
        msg.charge_current_ma = static_cast<uint16_t>(v.asInt());
    }
    if (reg.getProp("charger_ip2366", PowerProp::CHARGER_ACTIVE, v)) {
        msg.charger_active = v.asBool();
    }
    if (reg.getProp("charger_ip2366", PowerProp::CHARGER_FULL, v)) {
        msg.charger_full = v.asBool();
    }
    if (reg.getProp("charger_ip2366", PowerProp::VOLTAGE_NOW, v)) {
        msg.charger_voltage_mv = static_cast<uint16_t>(v.asInt());
    }
    if (reg.getProp("charger_ip2366", PowerProp::CHARGER_PHASE, v)) {
        msg.charger_phase = static_cast<uint8_t>(v.asInt());
    }

    /* 故障 */
    bool fault = false;
    if (reg.getProp("battery_bms", PowerProp::FAULT, v)) {
        fault = fault || v.asBool();
    }
    if (reg.getProp("charger_ip2366", PowerProp::FAULT, v)) {
        fault = fault || v.asBool();
    }
    msg.fault = fault;

    if (reg.getProp("battery_bms", PowerProp::FAULT_REASON, v)) {
        msg.fault_reason = v.asStr();
    } else if (reg.getProp("charger_ip2366", PowerProp::FAULT_REASON, v)) {
        msg.fault_reason = v.asStr();
    }
}

}  // namespace stark_power_manager
