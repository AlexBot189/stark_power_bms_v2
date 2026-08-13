/*
 * StarkRosAdapter.h — 电池 + 电源 ROS 接口统一适配器
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 集中管理本进程所有 ROS 发布/订阅, 后续新增 topic 都在此扩展。
 *   - 电池: 发布 battery_status/battery_fault/battery_info, 订阅 battery_ctrl
 *   - 电源: 发布 charge_state/power_state, 订阅 power_ctrl
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <ros/ros.h>
#include <stark_msgs/BatteryStatus.h>
#include <stark_msgs/BatteryFault.h>
#include <stark_msgs/BatteryInfo.h>
#include <stark_msgs/BatteryCtrl.h>
#include <stark_msgs/PowerCtrl.h>
#include <stark_msgs/ChargeState.h>
#include <stark_msgs/PowerState.h>

#include "battery/BatteryDispatcher.h"
#include "power/PowerManager.h"

namespace stark_power_manager {

class StarkRosAdapter
{
public:
    using WsBroadcastFn = std::function<void(const std::string&)>;

    StarkRosAdapter(std::shared_ptr<ros::NodeHandle> nh,
                    std::shared_ptr<BatteryDispatcher> dispatcher,
                    std::shared_ptr<PowerManager> mgr,
                    WsBroadcastFn ws_broadcast);

    ~StarkRosAdapter();

    /* 注册 pub/sub 与数据回调 */
    void Init();

    /* WebSocket 电池控制入口 */
    void HandleBatteryCtrl(const std::string& json_msg);

private:
    /* 电池数据回调 */
    void OnStatus(const BatteryStatus& status);
    void OnFault(const BatteryFault& fault);
    void OnInfo(const BatteryInfo& info);

    /* 控制指令回调 */
    void OnBatteryCtrlMsg(const stark_msgs::BatteryCtrl::ConstPtr& msg);
    void OnPowerCtrl(const stark_msgs::PowerCtrl::ConstPtr& msg);

    /* 电源周期发布 */
    void PublishPowerState(const ros::TimerEvent&);
    void FillChargeState(stark_msgs::ChargeState& msg);
    void FillPowerState(stark_msgs::PowerState& msg);

    std::shared_ptr<ros::NodeHandle> m_nh;
    std::shared_ptr<BatteryDispatcher> m_dispatcher;
    std::shared_ptr<PowerManager> m_mgr;
    WsBroadcastFn m_wsBroadcast;

    /* 电池 */
    ros::Publisher m_statusPub;
    ros::Publisher m_faultPub;
    ros::Publisher m_infoPub;
    ros::Subscriber m_batteryCtrlSub;

    /* 电源 */
    ros::Publisher m_chargeStatePub;
    ros::Publisher m_powerStatePub;
    ros::Subscriber m_powerCtrlSub;
    ros::Timer m_timer;
};

}  // namespace stark_power_manager
