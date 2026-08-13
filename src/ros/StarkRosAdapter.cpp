/*
 * StarkRosAdapter.cpp — 电池 + 电源 ROS 接口统一适配器实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "StarkRosAdapter.h"

#include <log_helper/LogHelper.h>
#include "3rd_party/nlohmann/json.hpp"
#include "power/PowerRegistry.h"

using nJson = nlohmann::json;
using namespace stark_power_manager;

/* ================================================================
 * 构造 / 析构
 * ================================================================ */

StarkRosAdapter::StarkRosAdapter(std::shared_ptr<ros::NodeHandle> nh,
                                 std::shared_ptr<BatteryDispatcher> dispatcher,
                                 std::shared_ptr<PowerManager> mgr,
                                 WsBroadcastFn ws_broadcast)
    : m_nh(std::move(nh))
    , m_dispatcher(std::move(dispatcher))
    , m_mgr(std::move(mgr))
    , m_wsBroadcast(std::move(ws_broadcast))
{
}

StarkRosAdapter::~StarkRosAdapter()
{
    /*
     * 析构时注销回调, 防止 recv 线程通过悬空的 this 调用 OnStatus/OnFault/OnInfo。
     * 时机: main() 退出时本对象先于 dispatcher 析构。
     */
    if (m_dispatcher)
    {
        m_dispatcher->ClearCallbacks();
    }
}

/* ================================================================
 * 初始化
 * ================================================================ */

void
StarkRosAdapter::Init()
{
    /* 电池 */
    m_statusPub = m_nh->advertise<stark_msgs::BatteryStatus>("/stark/battery_status", 10);
    m_faultPub  = m_nh->advertise<stark_msgs::BatteryFault>("/stark/battery_fault", 10, true);
    m_infoPub   = m_nh->advertise<stark_msgs::BatteryInfo>("/stark/battery_info", 10, true);
    m_batteryCtrlSub = m_nh->subscribe<stark_msgs::BatteryCtrl>(
        "/stark/battery_ctrl", 10, &StarkRosAdapter::OnBatteryCtrlMsg, this);

    m_dispatcher->RegisterStatusCallback(
        std::bind(&StarkRosAdapter::OnStatus, this, std::placeholders::_1));
    m_dispatcher->RegisterFaultCallback(
        std::bind(&StarkRosAdapter::OnFault, this, std::placeholders::_1));
    m_dispatcher->RegisterInfoCallback(
        std::bind(&StarkRosAdapter::OnInfo, this, std::placeholders::_1));

    /* 电源 */
    m_chargeStatePub = m_nh->advertise<stark_msgs::ChargeState>(
        "/stark/charge_state", 10);
    m_powerStatePub = m_nh->advertise<stark_msgs::PowerState>(
        "/stark/power_state", 10);
    m_powerCtrlSub = m_nh->subscribe<stark_msgs::PowerCtrl>(
        "/stark/power_ctrl", 10, &StarkRosAdapter::OnPowerCtrl, this);

    /* 1Hz 周期发布电源汇总状态 */
    m_timer = m_nh->createTimer(ros::Duration(1.0),
        &StarkRosAdapter::PublishPowerState, this);

    ECO_INFO("[StarkRosAdapter] initialized");
}

/* ================================================================
 * 电池数据回调
 * ================================================================ */

void
StarkRosAdapter::OnStatus(const BatteryStatus& status)
{
    stark_msgs::BatteryStatus msg;

    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "battery_status";

    msg.total_voltage_mv   = status.total_voltage_mv;
    msg.cell1_mv           = status.cell_voltage_mv[0];
    msg.cell2_mv           = status.cell_voltage_mv[1];
    msg.cell3_mv           = status.cell_voltage_mv[2];
    msg.cell4_mv           = status.cell_voltage_mv[3];
    msg.cell5_mv           = status.cell_voltage_mv[4];
    msg.cell6_mv           = status.cell_voltage_mv[5];
    msg.current_ma         = status.current_ma;
    msg.temperature_c      = status.temperature_c;
    msg.soc_percent        = status.soc_percent;
    msg.remain_capacity_mah = status.remain_capacity_mah;
    msg.total_capacity_mah = status.total_capacity_mah;
    msg.cycle_count        = status.cycle_count;
    msg.bms_status         = status.bms_status;
    msg.charge_current_ma  = status.charge_current_ma;

    m_statusPub.publish(msg);

    if (m_wsBroadcast) {
        nJson data;
        data["total_voltage_mv"]    = status.total_voltage_mv;
        data["cell1_mv"]            = status.cell_voltage_mv[0];
        data["cell2_mv"]            = status.cell_voltage_mv[1];
        data["cell3_mv"]            = status.cell_voltage_mv[2];
        data["cell4_mv"]            = status.cell_voltage_mv[3];
        data["cell5_mv"]            = status.cell_voltage_mv[4];
        data["cell6_mv"]            = status.cell_voltage_mv[5];
        data["current_ma"]          = status.current_ma;
        data["temperature_c"]       = status.temperature_c;
        data["soc_percent"]         = status.soc_percent;
        data["remain_capacity_mah"] = status.remain_capacity_mah;
        data["total_capacity_mah"]  = status.total_capacity_mah;
        data["cycle_count"]         = status.cycle_count;
        data["bms_status"]          = status.bms_status;
        data["charge_current_ma"]   = status.charge_current_ma;

        nJson root;
        root["channel"] = "battery_status";
        root["data"]    = data;
        m_wsBroadcast(root.dump());
    }
}

void
StarkRosAdapter::OnFault(const BatteryFault& fault)
{
    stark_msgs::BatteryFault msg;

    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "battery_fault";

    msg.sys_fault1    = fault.sys_fault1;
    msg.sys_fault2    = fault.sys_fault2;
    msg.dischg_fault1 = fault.dischg_fault1;
    msg.dischg_fault2 = fault.dischg_fault2;
    msg.chg_fault1    = fault.chg_fault1;
    msg.chg_fault2    = fault.chg_fault2;
    msg.pack_status   = fault.pack_status;
    msg.comm_timeout      = fault.comm_timeout;
    msg.cell_ntc_fault    = fault.cell_ntc_fault;
    msg.mos_ntc_fault     = fault.mos_ntc_fault;
    msg.cell_ntc_short    = fault.cell_ntc_short;
    msg.cell_ntc_open     = fault.cell_ntc_open;
    msg.mos_ntc_short     = fault.mos_ntc_short;
    msg.mos_ntc_open      = fault.mos_ntc_open;
    msg.cell_fault = fault.cell_fault;
    msg.afe_fault  = fault.afe_fault;
    msg.fuse_fault = fault.fuse_fault;
    msg.overdischarge_l1      = fault.overdischarge_l1;
    msg.dischg_overcurrent_l1 = fault.dischg_overcurrent_l1;
    msg.dischg_cell_overtemp  = fault.dischg_cell_overtemp;
    msg.dischg_mos_overtemp   = fault.dischg_mos_overtemp;
    msg.dischg_cell_lowtemp   = fault.dischg_cell_lowtemp;
    msg.overdischarge_l2      = fault.overdischarge_l2;
    msg.dischg_overcurrent_l2 = fault.dischg_overcurrent_l2;
    msg.dischg_short          = fault.dischg_short;
    msg.dischg_mos_fault      = fault.dischg_mos_fault;
    msg.overcharge_l1      = fault.overcharge_l1;
    msg.chg_overcurrent_l1 = fault.chg_overcurrent_l1;
    msg.chg_overtemp       = fault.chg_overtemp;
    msg.charger_fault      = fault.charger_fault;
    msg.chg_mos_fault      = fault.chg_mos_fault;
    msg.chg_timeout        = fault.chg_timeout;
    msg.overcharge_l2      = fault.overcharge_l2;
    msg.chg_overcurrent_l2 = fault.chg_overcurrent_l2;
    msg.chg_mos_active          = fault.chg_mos_active;
    msg.dischg_mos_active       = fault.dischg_mos_active;
    msg.chg_mos_master_ctrl     = fault.chg_mos_master_ctrl;
    msg.dischg_mos_master_ctrl  = fault.dischg_mos_master_ctrl;

    m_faultPub.publish(msg);

    if (m_wsBroadcast) {
        nJson data;
        data["sys_fault1"]    = fault.sys_fault1;
        data["sys_fault2"]    = fault.sys_fault2;
        data["dischg_fault1"] = fault.dischg_fault1;
        data["dischg_fault2"] = fault.dischg_fault2;
        data["chg_fault1"]    = fault.chg_fault1;
        data["chg_fault2"]    = fault.chg_fault2;
        data["pack_status"]   = fault.pack_status;

        nJson root;
        root["channel"] = "battery_fault";
        root["data"]    = data;
        m_wsBroadcast(root.dump());
    }
}

void
StarkRosAdapter::OnInfo(const BatteryInfo& info)
{
    stark_msgs::BatteryInfo msg;

    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "battery_info";

    msg.version = info.version;
    msg.id      = info.id;

    m_infoPub.publish(msg);

    if (m_wsBroadcast) {
        nJson data;
        data["version"] = info.version;
        data["id"]      = info.id;

        nJson root;
        root["channel"] = "battery_info";
        root["data"]    = data;
        m_wsBroadcast(root.dump());
    }
}

/* ================================================================
 * 控制指令回调
 * ================================================================ */

void
StarkRosAdapter::OnBatteryCtrlMsg(const stark_msgs::BatteryCtrl::ConstPtr& msg)
{
    if (!m_dispatcher) {
        return;
    }

    switch (msg->cmd) {
    case 1:
        m_dispatcher->SendControl(0x90, 0x01, {msg->param1, msg->param2});
        break;
    case 2:
        m_dispatcher->SendControl(0x90, 0x03, {msg->param1});
        break;
    default:
        ECO_WARN("[StarkRosAdapter] unknown battery ctrl cmd: %u", msg->cmd);
        break;
    }
}

void
StarkRosAdapter::OnPowerCtrl(const stark_msgs::PowerCtrl::ConstPtr& msg)
{
    auto& reg = PowerRegistry::instance();

    switch (msg->cmd) {
    case stark_msgs::PowerCtrl::CTRL_CHARGE: {
        bool en = msg->enable;
        reg.setProp("charger_ip2366", PowerProp::CHARGE_ENABLE, PowerValue(en));
        reg.setProp("battery_bms", PowerProp::CHARGE_ENABLE, PowerValue(en));
        ECO_INFO("[StarkRosAdapter] CTRL_CHARGE enable=%d", en ? 1 : 0);
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_DISCHARGE: {
        bool en = msg->enable;
        reg.setProp("battery_bms", PowerProp::DISCHARGE_ENABLE, PowerValue(en));
        ECO_INFO("[StarkRosAdapter] CTRL_DISCHARGE enable=%d", en ? 1 : 0);
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_CURRENT: {
        int64_t ma = static_cast<int64_t>(msg->current_limit_ma);
        reg.setProp("charger_ip2366", PowerProp::CHARGE_CURRENT_SET, PowerValue(ma));
        ECO_INFO("[StarkRosAdapter] CTRL_CURRENT limit=%ld mA", (long)ma);
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_SHUTDOWN: {
        ECO_WARN("[StarkRosAdapter] CTRL_SHUTDOWN received");
        reg.setProp("battery_bms", PowerProp::SHUTDOWN, PowerValue(true));
        break;
    }

    case stark_msgs::PowerCtrl::CTRL_STANDBY: {
        ECO_WARN("[StarkRosAdapter] CTRL_STANDBY received");
        reg.setProp("charger_ip2366", PowerProp::STANDBY, PowerValue(true));
        break;
    }

    default:
        ECO_WARN("[StarkRosAdapter] unknown power ctrl cmd=%u", msg->cmd);
        break;
    }
}

void
StarkRosAdapter::HandleBatteryCtrl(const std::string& json_msg)
{
    if (!m_dispatcher) {
        return;
    }

    try {
        nJson j = nJson::parse(json_msg, nullptr, false);
        if (j.is_discarded()) {
            return;
        }

        uint8_t cmd = j.value("cmd", 0);
        uint8_t p1  = j.value("param1", 0);
        uint8_t p2  = j.value("param2", 0);

        switch (cmd) {
        case 1:
            m_dispatcher->SendControl(0x90, 0x01, {p1, p2});
            break;
        case 2:
            m_dispatcher->SendControl(0x90, 0x03, {p1});
            break;
        default:
            break;
        }
    } catch (const std::exception& e) {
        ECO_ERROR("[StarkRosAdapter] parse battery_ctrl failed: %s", e.what());
    }
}

/* ================================================================
 * 电源周期发布
 * ================================================================ */

void
StarkRosAdapter::PublishPowerState(const ros::TimerEvent&)
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

    /* WebSocket 广播充电状态 (供 web 调试工具实时显示) */
    if (m_wsBroadcast) {
        nJson data;
        data["charge_state"]      = ps.charge_state;
        data["adapter_online"]    = ps.adapter_online;
        data["batt_voltage_mv"]   = ps.batt_voltage_mv;
        data["batt_current_ma"]   = ps.batt_current_ma;
        data["batt_temp_c"]       = ps.batt_temp_c;
        data["batt_soc"]          = ps.batt_soc;
        data["charge_type"]       = ps.charge_type;
        data["charge_current_ma"] = ps.charge_current_ma;
        data["charger_active"]    = ps.charger_active;
        data["charger_full"]      = ps.charger_full;
        data["charger_voltage_mv"] = ps.charger_voltage_mv;
        data["charger_phase"]     = ps.charger_phase;
        data["fault"]             = ps.fault;
        data["fault_reason"]      = ps.fault_reason;

        nJson root;
        root["channel"] = "power_state";
        root["data"]    = data;
        m_wsBroadcast(root.dump());
    }
}

void
StarkRosAdapter::FillChargeState(stark_msgs::ChargeState& msg)
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

void
StarkRosAdapter::FillPowerState(stark_msgs::PowerState& msg)
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
