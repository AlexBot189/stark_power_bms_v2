#include "BatteryRosAdapter.hpp"

#include <log_helper/LogHelper.h>
#include <stark_msgs/BatteryStatus.h>
#include <stark_msgs/BatteryFault.h>
#include <stark_msgs/BatteryInfo.h>
#include <stark_msgs/BatteryCtrl.h>
#include "3rd_party/nlohmann/json.hpp"

using nJson = nlohmann::json;
using namespace stark_power_manager;

BatteryRosAdapter::BatteryRosAdapter(std::shared_ptr<ros::NodeHandle> nh,
                                     std::shared_ptr<BatteryDispatcher> dispatcher,
                                     WsBroadcastFn ws_broadcast)
    : m_nh(nh)
    , m_dispatcher(dispatcher)
    , m_wsBroadcast(ws_broadcast)
{
    Init();
}

BatteryRosAdapter::~BatteryRosAdapter()
{
    /*
     * 析构时主动注销回调, 防止 recv 线程后续通过悬空的 this 调用 OnStatus/OnFault/OnInfo.
     * 时机: main() 退出时 pBatteryRos 先于 pBattery 析构, 在 pBattery 的 Destroy()
     * join 线程之前, recv 线程可能仍在触发原始回调
     */
    if (m_dispatcher)
    {
        m_dispatcher->ClearCallbacks();
    }
}

void
BatteryRosAdapter::Init()
{
    m_statusPub = m_nh->advertise<stark_msgs::BatteryStatus>("/stark/battery_status", 10);
    m_faultPub  = m_nh->advertise<stark_msgs::BatteryFault>("/stark/battery_fault", 10, true);
    m_infoPub   = m_nh->advertise<stark_msgs::BatteryInfo>("/stark/battery_info", 10, true);

    m_ctrlSub = m_nh->subscribe<stark_msgs::BatteryCtrl>(
        "/stark/battery_ctrl", 10, &BatteryRosAdapter::OnCtrlMsg, this);

    m_dispatcher->RegisterStatusCallback(
        std::bind(&BatteryRosAdapter::OnStatus, this, std::placeholders::_1));
    m_dispatcher->RegisterFaultCallback(
        std::bind(&BatteryRosAdapter::OnFault, this, std::placeholders::_1));
    m_dispatcher->RegisterInfoCallback(
        std::bind(&BatteryRosAdapter::OnInfo, this, std::placeholders::_1));
}

void
BatteryRosAdapter::OnStatus(const BatteryStatus& status)
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
BatteryRosAdapter::OnFault(const BatteryFault& fault)
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
BatteryRosAdapter::OnInfo(const BatteryInfo& info)
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

void
BatteryRosAdapter::OnCtrlMsg(const stark_msgs::BatteryCtrl::ConstPtr& msg)
{
    if (!m_dispatcher) {
        return;
    }

    uint8_t p1 = msg->param1;
    uint8_t p2 = msg->param2;

    switch (msg->cmd) {
    case 1:
        m_dispatcher->SendControl(0x90, 0x01, {p1, p2});
        break;
    case 2:
        m_dispatcher->SendControl(0x90, 0x03, {p1});
        break;
    default:
        ECO_WARN("[BatteryRosAdapter] unknown ctrl cmd: %u", msg->cmd);
        break;
    }
}

void
BatteryRosAdapter::HandleBatteryCtrl(const std::string& json_msg)
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
        ECO_ERROR("[BatteryRosAdapter] parse battery_ctrl failed: %s", e.what());
    }
}
