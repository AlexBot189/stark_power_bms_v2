/*
 * PowerRosAdapter.h — 电源管理 ROS 接口适配器
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 职责:
 *   - 订阅 PowerCtrl 控制指令 → PowerRegistry::setProp()
 *   - 1Hz 发布 ChargeState / PowerState 汇总状态
 *
 * 不修改 PowerManager / PowerRegistry / IPowerSource 等框架层。
 */
#pragma once

#include <memory>
#include <ros/ros.h>
#include <stark_msgs/PowerCtrl.h>
#include <stark_msgs/ChargeState.h>
#include <stark_msgs/PowerState.h>

#include "power/PowerManager.h"

namespace stark_power_manager {

class PowerRosAdapter {
public:
    PowerRosAdapter(std::shared_ptr<ros::NodeHandle> nh,
                    std::shared_ptr<PowerManager> mgr);

    /* 注册 pub/sub + 1Hz 定时器 */
    void Init();

private:
    void OnPowerCtrl(const stark_msgs::PowerCtrl::ConstPtr& msg);
    void PublishPeriodic(const ros::TimerEvent&);

    void FillChargeState(stark_msgs::ChargeState& msg);
    void FillPowerState(stark_msgs::PowerState& msg);

    std::shared_ptr<ros::NodeHandle> m_nh;
    std::shared_ptr<PowerManager> m_mgr;

    ros::Publisher  m_chargeStatePub;
    ros::Publisher  m_powerStatePub;
    ros::Subscriber m_powerCtrlSub;
    ros::Timer      m_timer;
};

}  // namespace stark_power_manager
