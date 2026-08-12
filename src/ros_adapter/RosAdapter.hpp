#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include "interface/IListener.hpp"
#include "protocol/UartDispatcher.hpp"

#include "deebot_msgs/MCUBatteryStatus.h"
#include "deebot_msgs/MotorControlReq.h"
#include "deebot_msgs/EyeControlReq.h"
#include "deebot_msgs/EyeControlRsp.h"
#include "deebot_msgs/TempControlReq.h"
#include "deebot_msgs/TouchData.h"
#include "deebot_msgs/MCUVersionReq.h"
#include "deebot_msgs/MCUOtaReq.h"
#include "deebot_msgs/McuOtaRsp.h"
#include "deebot_msgs/PowerOffRsp.h"
#include "deebot_msgs/MCUErpCtl.h"
#include "deebot_msgs/KeyEventNotify.h"
#include "deebot_msgs/MCURtcReq.h"
#include "deebot_msgs/MCUCommonControlReq.h"
#include "deebot_msgs/MCUCommonControlRsp.h"
#include "deebot_msgs/SetHeartReq.h"
#include "deebot_msgs/SetHeartRsp.h"

//#include <geometry_msgs/Twist.h>


namespace stark_power_manager
{
class UartDispatcher;

class RosAdapter : public IListener
{
public:
    RosAdapter(std::shared_ptr<ros::NodeHandle> nh);

    ~RosAdapter() = default;

    /**< 在构造函数后调用 */
    int
    Init();

    void
    Update(const boost::any& data) override;

    void
    SetDispatcher(std::shared_ptr<IMsgInternalDispatcher> dispatcher);

    void HandleTransformMsg(const std::string& msg);

private:
    void
    PubImu(const boost::any& data);
 
   void
    PubServo(const boost::any& data);

    void
    PubOdom(const boost::any& data);

    void
    PubOnOff(const boost::any& data);

    void
    PubPhyKey(const boost::any& data);

    void
    PubBatteryStatus(const boost::any& data);

    void
    PubTouchControl(const boost::any& data);

    void
    PubMcuOtaRsp(const boost::any& data);


    void
    PubIrSignal(const boost::any& data);

    void
    PubMotorCtrlRsp(const boost::any& data);

    void
    PubMcuCurrentInfo(const boost::any& data);

    void
    PubMcuTempInfo(const boost::any& data);

    void
    PubMcuExInfo(const boost::any& data);

    void
    PubMcuGyroPose(const boost::any& data);

    void
    PubMcuPowerCtlRsp(const boost::any& data);

    void
    PubHeartControlReply(const boost::any& data);

    void
    OnMotorCtrl(const deebot_msgs::MotorControlReq::ConstPtr& msg);

    void
    OnTempCtrl(const deebot_msgs::TempControlReq::ConstPtr& msg);

    void
    OnMcuVerInfo(const deebot_msgs::MCUVersionReq::ConstPtr& msg);
    
    void
    OnMcuOtaReq(const deebot_msgs::MCUOtaReq::ConstPtr& msg);
    
    void
    OnPowerOffCtl(const deebot_msgs::PowerOffRsp::ConstPtr& msg);
 
    void
    OnErpCtl(const deebot_msgs::MCUErpCtl::ConstPtr& msg);
    
    void
    OnEyeCtrl(const deebot_msgs::EyeControlReq::ConstPtr& msg);

    void
    OnSysTimeSyncCtl(const deebot_msgs::KeyEventNotify::ConstPtr& msg);

    void
    OnMcuRtcReq(const deebot_msgs::MCURtcReq::ConstPtr& msg);
 
    void
    OnMcuPowerCtlReq(const deebot_msgs::MCUCommonControlReq::ConstPtr& msg);

    void
    OnSetHeartReq(const deebot_msgs::SetHeartReq::ConstPtr& msg);

private:
    std::shared_ptr<ros::NodeHandle> m_rosNh{ nullptr };

    ros::Publisher m_imuPublisher;
    ros::Publisher m_servoPublisher;
    ros::Publisher m_onOffPublisher;
    ros::Publisher m_phyKeyPublisher; // 物理按键
    ros::Publisher m_BatteryStatusPublisher;
    ros::Publisher m_TouchControlPublisher;
    ros::Publisher m_MotorCtrlRspPub;
    ros::Publisher m_McuOtaRspPub;
    ros::Publisher m_McuCurrentPub;
    ros::Publisher m_McuTempInfoPub;
    ros::Publisher m_McuExInfoPub;
    ros::Publisher m_McuGyroPosePub;
    ros::Publisher m_McuPowerCtlRspPub;
    ros::Publisher m_HeartControlReplyPub;

    ros::Subscriber m_MotorCtrlSub;
    ros::Subscriber m_EyeCtrlSub;
    ros::Subscriber m_TempCtrlSub;
    ros::Subscriber m_McuVerInfoGetSub;
    ros::Subscriber m_McuOtaReqSub;
    ros::Subscriber m_PowerOffMsgSub;
    ros::Subscriber m_McuErpCtlSub;
    ros::Subscriber m_SysTimeSyncToMcuSub;
    ros::Subscriber m_McuRtcTimeReqSub;
    ros::Subscriber m_McuPowerReqSub;
    ros::Subscriber m_SetHeartReqSub;
private:
    std::shared_ptr<UartDispatcher> m_uartDispatcher{ nullptr };
};
}  // namespace stark_power_manager
