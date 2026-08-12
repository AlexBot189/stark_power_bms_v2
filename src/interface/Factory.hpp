/*
 * @Author: liuning ning.liu@ecovacs.com
 * @Date: 2024-01-29 19:37:28
 * @LastEditors: colin yuanzhi.yang@ecovacs.com
 * @LastEditTime: 2024-06-19 09:23:11
 * @FilePath: /stark_power_manager/src/interface/Factory.hpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once

#include <ros/ros.h>
#include <memory>
#include "IListener.hpp"
#include "ILidarAdapter.hpp"
#include "ILineLaserAdapter.hpp"
#include "lidar_adpter/Defines.hpp"
#include "protocol/Defines.hpp"
#include "gw_adapter/Defines.hpp"
#include "protocol/IMsgInternalDispatcher.hpp"
#include "ros_adapter/RosAdapter.hpp"
#include "config/Defines.hpp"
#include "drivers/gpio_touch/TouchMonitorAdapter.hpp"
#include "drivers/gpio_touch/Defines.hpp"

namespace stark_power_manager
{
class Factory
{
public:
    // static std::shared_ptr<IMsgInternalDispatcher>
    // CreateDispatcher(const DeviceOption& deviceOption, std::shared_ptr<ros::NodeHandle> nh = nullptr);

    static std::shared_ptr<IMsgInternalDispatcher>
    CreateSingletonDispatcher(const DeviceOption& deviceOption, std::shared_ptr<ros::NodeHandle> nh = nullptr);

    static std::shared_ptr<IListener>
    CreateWebListener(const std::shared_ptr<RosAdapter>& ros_adapter = nullptr, const GwOption& option = {});

    /**< lds */
    static std::shared_ptr<ILidarAdapter>
    CreateLidarAdapter(const LidarOption& lidarOption, std::shared_ptr<ros::NodeHandle> nh);

    /**< 线激光 */
    static std::shared_ptr<ILineLaserAdapter>
    CreateLineLaserAdapter(const LineLaserOption& lineLaserOption, std::shared_ptr<ros::NodeHandle> nh);


    /**< gpio触摸 */
    static std::shared_ptr<TouchMonitorAdapter>
    CreateTouchAdapter(const TouchOption& touchOption, std::shared_ptr<ros::NodeHandle> nh);
};
}  // namespace stark_power_manager
