#pragma once

#include <functional>
#include <memory>
#include <string>
#include <ros/ros.h>
#include <stark_msgs/BatteryStatus.h>
#include <stark_msgs/BatteryFault.h>
#include <stark_msgs/BatteryInfo.h>
#include <stark_msgs/BatteryCtrl.h>
#include "BatteryDispatcher.h"

namespace stark_power_manager {

class BatteryRosAdapter
{
public:
    using WsBroadcastFn = std::function<void(const std::string&)>;

    BatteryRosAdapter(std::shared_ptr<ros::NodeHandle> nh,
                      std::shared_ptr<BatteryDispatcher> dispatcher,
                      WsBroadcastFn ws_broadcast);

    ~BatteryRosAdapter();

    /**< 注册 pub/sub 和回调 */
    void Init();

    /**< WebSocket 控制消息入口 */
    void HandleBatteryCtrl(const std::string& json_msg);

private:
    void OnStatus(const BatteryStatus& status);
    void OnFault(const BatteryFault& fault);
    void OnInfo(const BatteryInfo& info);
    void OnCtrlMsg(const stark_msgs::BatteryCtrl::ConstPtr& msg);

    std::shared_ptr<ros::NodeHandle> m_nh;
    std::shared_ptr<BatteryDispatcher> m_dispatcher;
    WsBroadcastFn m_wsBroadcast;

    ros::Publisher m_statusPub;
    ros::Publisher m_faultPub;
    ros::Publisher m_infoPub;
    ros::Subscriber m_ctrlSub;
};

}  // namespace stark_power_manager
