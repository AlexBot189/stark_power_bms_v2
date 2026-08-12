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

    msg.fault_0               = fault.records[0];
    msg.fault_1               = fault.records[1];
    msg.fault_2               = fault.records[2];
    msg.fault_3               = fault.records[3];
    msg.fault_4               = fault.records[4];
    msg.fault_5               = fault.records[5];
    msg.fault_6               = fault.records[6];
    msg.fault_7               = fault.records[7];
    msg.fault_8               = fault.records[8];
    msg.fault_9               = fault.records[9];
    msg.charger_overvoltage   = fault.charger_overvoltage;
    msg.charge_overcurrent    = fault.charge_overcurrent;
    msg.ntc_short             = fault.ntc_short;
    msg.ntc_open              = fault.ntc_open;
    msg.cell_voltage_diff     = fault.cell_voltage_diff;
    msg.charge_timeout        = fault.charge_timeout;
    msg.discharge_overcurrent = fault.discharge_overcurrent;
    msg.discharge_short       = fault.discharge_short;
    msg.secondary_overcharge  = fault.secondary_overcharge;

    m_faultPub.publish(msg);

    if (m_wsBroadcast) {
        nJson data;
        data["records"] = nJson::array();
        for (int i = 0; i < 10; i++) {
            data["records"].push_back(fault.records[i]);
        }
        data["charger_overvoltage"]   = fault.charger_overvoltage;
        data["charge_overcurrent"]    = fault.charge_overcurrent;
        data["ntc_short"]             = fault.ntc_short;
        data["ntc_open"]              = fault.ntc_open;
        data["cell_voltage_diff"]     = fault.cell_voltage_diff;
        data["charge_timeout"]        = fault.charge_timeout;
        data["discharge_overcurrent"] = fault.discharge_overcurrent;
        data["discharge_short"]       = fault.discharge_short;
        data["secondary_overcharge"]  = fault.secondary_overcharge;

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
