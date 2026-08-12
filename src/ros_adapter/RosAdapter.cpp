#include <log_helper/LogHelper.h>
#include <deebot_msgs/ImuData.h>
#include <deebot_msgs/KeyInfo.h>
#include <deebot_msgs/MCUBatteryStatus.h>
#include <deebot_msgs/MotorControlReq.h>
#include <deebot_msgs/MotorControlRsp.h>
#include <deebot_msgs/TempControlReq.h>
#include <deebot_msgs/TempControlRsp.h>
#include <deebot_msgs/MotorPos.h>
#include <deebot_msgs/MCUVersionReq.h>
#include <deebot_msgs/PowerOffRsp.h>
#include <deebot_msgs/McuCurrentData.h>
#include <deebot_msgs/McuTempData.h>
#include <deebot_msgs/ExceptionEventReport.h>
#include <deebot_msgs/ImuPostureEstimate.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "RosAdapter.hpp"
#include "RosTopic.hpp"
#include "utility/Util.hpp"

#include "eros_common/FrameIdName.hpp"
#include "eros_common/TopicServiceName.hpp"
#include "3rd_party/nlohmann/json.hpp"

using nJson = nlohmann::json;
using namespace eros_common;
using namespace stark_power_manager;

RosAdapter::RosAdapter(std::shared_ptr<ros::NodeHandle> nh)
    : m_rosNh(nh)
{
    Init();
}

//boost::any_cast<std::vector<ServoData>>(data)
void
RosAdapter::Update(const boost::any& data)
{
    if (data.type() == typeid(ImuData))
    {
        PubImu(data);
    }
    else if (data.type() == typeid(std::vector<ServoData>))
    {
        PubServo(data);
    }
    else if(data.type() == typeid(MCUPhyKey))
    {
        PubPhyKey(data);
    }
    else if(data.type() == typeid(MCUBatteryStatus))
    {
        PubBatteryStatus(data);
    }
    else if(data.type() == typeid(MCUTouchControl))
    {
        PubTouchControl(data);
    }
    else if(data.type() == typeid(MCUMotorControlReply))
    {
        PubMotorCtrlRsp(data);
    }
    else if(data.type() == typeid(MCUOtaRsp))
    {
        PubMcuOtaRsp(data);
    }
    else if(data.type() == typeid(MCUCurrentInfo))
    {
        PubMcuCurrentInfo(data);
    }
    else if(data.type() == typeid(MCUTempInfo))
    {
        PubMcuTempInfo(data);
    }
    else if(data.type() == typeid(MCUEcInfo))
    {
        PubMcuExInfo(data);
    }
    else if(data.type() == typeid(MCUGyroPoseInfo))
    {
        PubMcuGyroPose(data);
    }
    else if(data.type() == typeid(MCUPowerRsp))
    {
        PubMcuPowerCtlRsp(data);
    }
    else if(data.type() == typeid(MCUHeartControlReply))
    {
        PubHeartControlReply(data);
    }
}

void
RosAdapter::SetDispatcher(std::shared_ptr<IMsgInternalDispatcher> dispatcher)
{
    m_uartDispatcher = std::dynamic_pointer_cast<UartDispatcher>(dispatcher);
}

int
RosAdapter::Init()
{
    /**< 发布者 */
    m_imuPublisher = m_rosNh->advertise<deebot_msgs::ImuData>(TOPIC_IMU, 20);
    m_servoPublisher = m_rosNh->advertise<deebot_msgs::MotorPos>(TOPIC_MOTOR_POS, 20);
    m_phyKeyPublisher = m_rosNh->advertise<deebot_msgs::KeyInfo>(TOPIC_MCU_KEY_INFO,20);
    m_BatteryStatusPublisher = m_rosNh->advertise<deebot_msgs::MCUBatteryStatus>(TOPIC_MCU_BATTERY_STATUS,20);
    m_TouchControlPublisher = m_rosNh->advertise<deebot_msgs::TouchData>(TOPIC_TOUCH_CONTROL_RSP,20);
    m_McuOtaRspPub = m_rosNh->advertise<deebot_msgs::McuOtaRsp>(TOPIC_MCUOTA_CONTROL_RSP,10);
    
    m_McuCurrentPub = m_rosNh->advertise<deebot_msgs::McuCurrentData>(TOPIC_MCU_CURRENT,10);
    m_McuTempInfoPub = m_rosNh->advertise<deebot_msgs::McuTempData>(TOPIC_MCU_TEMP,10);
    m_McuGyroPosePub = m_rosNh->advertise<deebot_msgs::ImuPostureEstimate>(TOPIC_MCU_GYROPOSE,10);
    m_McuPowerCtlRspPub = m_rosNh->advertise<deebot_msgs::MCUCommonControlRsp>(TOPIC_MCU_POWER_SET_RSP,10);
    m_McuExInfoPub = m_rosNh->advertise<deebot_msgs::ExceptionEventReport>(TOPIC_MCU_EXCODE,10, true);
    m_HeartControlReplyPub = m_rosNh->advertise<deebot_msgs::SetHeartRsp>(TOPIC_MCU_HEART_CONTROL_REPLY,10);

    m_MotorCtrlRspPub = m_rosNh->advertise<deebot_msgs::MotorControlRsp>(TOPIC_CTRL_MCU_MOTOR_RSP, 20);

    m_MotorCtrlSub = m_rosNh->subscribe<deebot_msgs::MotorControlReq>(TOPIC_CTRL_MCU_MOTOR_REQ, 10, &RosAdapter::OnMotorCtrl, this);
    
//    m_EyeCtrlSub = m_rosNh->subscribe<deebot_msgs::EyeControlReq>(TOPIC_EYE_CONTROL_REQ, 10, &RosAdapter::OnEyeCtrl, this);
    
    m_TempCtrlSub = m_rosNh->subscribe<deebot_msgs::TempControlReq>(TOPIC_TEMP_CONTROL_REQ, 10, &RosAdapter::OnTempCtrl, this);
    
    m_McuVerInfoGetSub = m_rosNh->subscribe<deebot_msgs::MCUVersionReq>(TOPIC_MCUVER_CONTROL_REQ, 10, &RosAdapter::OnMcuVerInfo, this);
    
    m_McuOtaReqSub = m_rosNh->subscribe<deebot_msgs::MCUOtaReq>(TOPIC_MCUOTA_CONTROL_REQ, 10, &RosAdapter::OnMcuOtaReq, this);

    m_PowerOffMsgSub = m_rosNh->subscribe<deebot_msgs::PowerOffRsp>(TOPIC_POWEROFF_MSG_REQ, 10, &RosAdapter::OnPowerOffCtl, this);
    
    m_McuErpCtlSub = m_rosNh->subscribe<deebot_msgs::MCUErpCtl>(TOPIC_ERPCTL_MSG_REQ, 10, &RosAdapter::OnErpCtl, this);
    
    m_SysTimeSyncToMcuSub = m_rosNh->subscribe<deebot_msgs::KeyEventNotify>(TOPIC_SYSTIME_SYNC_REQ, 10, &RosAdapter::OnSysTimeSyncCtl, this);
    
    m_McuRtcTimeReqSub = m_rosNh->subscribe<deebot_msgs::MCURtcReq>(TOPIC_MCURTCTIME_REQ, 10, &RosAdapter::OnMcuRtcReq, this);
    
    m_McuPowerReqSub = m_rosNh->subscribe<deebot_msgs::MCUCommonControlReq>(TOPIC_MCU_POWER_SET_REQ, 10, &RosAdapter::OnMcuPowerCtlReq, this);
    
    m_SetHeartReqSub = m_rosNh->subscribe<deebot_msgs::SetHeartReq>(TOPIC_MCU_SET_HEART_REQ, 10, &RosAdapter::OnSetHeartReq, this);
    
//    m_EyeCtrlRspPub = m_rosNh->advertise<deebot_msgs::EyeControlRsp>(TOPIC_EYE_CONTROL_RSP, 10);
    return 0;
}

void
RosAdapter::HandleTransformMsg(const std::string& msg)
{
    if (msg.empty())
    {
        ECO_ERROR("Received a pad data, json parse error...");
        return;
    }
    ECO_INFO_STREAM("HandleCalibrationRequest strMsg: " << msg.c_str());
    nJson root = nJson::parse(msg);
    std::string channel = root.at("channel").get<std::string>();
    if (channel.empty())
    {
        ECO_ERROR("Channel is empty.");
        return;
    }
    else
    {
        // do nothing
    }
}

void
RosAdapter::PubImu(const boost::any& data)
{
    auto anyData = boost::any_cast<ImuData>(data);

    deebot_msgs::ImuData msg;

    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = FRAME_ID_IMU;

    msg.pitch = anyData.pitch;
    msg.roll = anyData.roll;
    msg.yaw = anyData.yaw;
    msg.vx = anyData.vx;
    msg.vy = anyData.vy;
    msg.vz = anyData.vz;
    msg.ax = anyData.ax;
    msg.ay = anyData.ay;
    msg.az = anyData.az;

    // ECO_INFO("666 pubImu");
    m_imuPublisher.publish(msg);
}

void RosAdapter::PubServo(const boost::any& data)
{
    auto servoData = boost::any_cast<std::vector<ServoData>>(data);
    
        deebot_msgs::MotorPos msg;

        // 填充消息头
        msg.header.stamp = ros::Time::now();
        msg.header.frame_id = FRAME_ID_SERVO;

        // 为每个数据项赋值
	for(const auto& data:servoData)
	{
		msg.angle.push_back(data.angle);
		msg.type.push_back(data.type);
	}

        // 发布消息
        m_servoPublisher.publish(msg);

}

void
RosAdapter::PubPhyKey(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUPhyKey>(data);
    deebot_msgs::KeyInfo info;

    info.header.stamp = ros::Time::now();
    info.header.frame_id = "mcu_phy_key";

    info.idx = anyData.key_type;
    info.event = anyData.key_event;

    ECO_INFO_NEW("pubPhyKey, key_type: {}, key_event: {}", info.idx, info.event);
    m_phyKeyPublisher.publish(info);
}

void
RosAdapter::PubBatteryStatus(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUBatteryStatus>(data);
    deebot_msgs::MCUBatteryStatus batterystatus;

    batterystatus.header.stamp = ros::Time::now();
    batterystatus.header.frame_id = "petrobot_battery_status";

    batterystatus.battery_percent = anyData.battery_percent;
    batterystatus.charge_status = anyData.charge_state;//
    batterystatus.battery_voltage = anyData.battery_voltage;//单位 0.01V
    batterystatus.battery_temperature = anyData.battery_temp;//
    batterystatus.charge_voltage = anyData.charge_voltage;//单位 0.01V
    batterystatus.battery_current = anyData.charge_current;//单位 0.01A
    batterystatus.charge_type = anyData.charge_type;

    m_BatteryStatusPublisher.publish(batterystatus);
}

void
RosAdapter::PubTouchControl(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUTouchControl>(data);
    deebot_msgs::TouchData tControl;

    tControl.header.stamp = ros::Time::now();
    tControl.header.frame_id = "TouchData_Pub";

    tControl.touch_part = anyData.touch_part;
    tControl.touch_mode = anyData.touch_mode;
    tControl.touch_force = anyData.touch_force;
    tControl.touch_ch = anyData.touch_ch;

    m_TouchControlPublisher.publish(tControl);
}

void
RosAdapter::PubMotorCtrlRsp(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUMotorControlReply>(data);

    deebot_msgs::MotorControlRsp rsp;

    rsp.task_id = anyData.task_id;
    rsp.state = anyData.status;

    m_MotorCtrlRspPub.publish(rsp);
}
	
void
RosAdapter::PubMcuOtaRsp(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUOtaRsp>(data);

    deebot_msgs::McuOtaRsp rsp;

    rsp.ota_flag = anyData.ota_flag;
    
    ECO_INFO_NEW("recv MCU OTA Flag and pub: {}", rsp.ota_flag);

    m_McuOtaRspPub.publish(rsp);
}

void
RosAdapter::PubMcuCurrentInfo(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUCurrentInfo>(data);

    deebot_msgs::McuCurrentData currentData;
 
    currentData.header.stamp = ros::Time::now();
    currentData.header.frame_id = "McuCurrentInfo";

    currentData.tailMotor = anyData.tailMotor;
    currentData.tailMotorCur = anyData.tailMotorCur;

    currentData.heater = anyData.heater;
    currentData.heaterCur = anyData.heaterCur;

    currentData.rollServo = anyData.rollServo;
    currentData.rollServoCur = anyData.rollServoCur;

    currentData.yawServo = anyData.yawServo;
    currentData.yawServoCur = anyData.yawServoCur;

    currentData.pitchServo = anyData.pitchServo;
    currentData.pitchServoCur = anyData.pitchServoCur;

    m_McuCurrentPub.publish(currentData);
}

void
RosAdapter::PubMcuTempInfo(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUTempInfo>(data);

    deebot_msgs::McuTempData tempData;
 
    tempData.header.stamp = ros::Time::now();
    tempData.header.frame_id = "McuTempInfo";

    tempData.env = anyData.env;
    tempData.envTemp_value = anyData.envTemp_value;

    tempData.heaterLeft = anyData.heaterLeft;
    tempData.heaterLeftTemp_value = anyData.heaterLeftTemp_value;

    tempData.heaterRight = anyData.heaterRight;
    tempData.heaterRightTemp_value = anyData.heaterRightTemp_value;

    tempData.rollServo = anyData.rollServo;
    tempData.rollServoTemp_value = anyData.rollServoTemp_value;

    tempData.yawServo = anyData.yawServo;
    tempData.yawServoTemp_value = anyData.yawServoTemp_value;

    tempData.pitchServo = anyData.pitchServo;
    tempData.pitchServoTemp_value = anyData.pitchServoTemp_value;

    m_McuTempInfoPub.publish(tempData);
}

void
RosAdapter::PubMcuExInfo(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUEcInfo>(data);

    deebot_msgs::ExceptionEventReport McuExInfo;
 
    McuExInfo.header.stamp = ros::Time::now();
    McuExInfo.header.frame_id = "McuExInfo";

    McuExInfo.exceptionInfo.code = anyData.Ex_Code;
    McuExInfo.exceptionInfo.level = static_cast<uint8_t>(anyData.Ex_type);
    McuExInfo.exceptionInfo.eventType = (static_cast<uint8_t>(anyData.Ex_status) == 0) ? 2 : 1;

    ECO_INFO_NEW("MCU EcInfo Pub Ex_level:{}, Ex_eventType:{}, Ex_Code:{}", McuExInfo.exceptionInfo.level, McuExInfo.exceptionInfo.eventType, McuExInfo.exceptionInfo.code);

    m_McuExInfoPub.publish(McuExInfo);
}

void
RosAdapter::PubMcuGyroPose(const boost::any& data)
{
    auto anyData = boost::any_cast<MCUGyroPoseInfo>(data);

    deebot_msgs::ImuPostureEstimate GyroPose;
 
    GyroPose.header.stamp = ros::Time::now();
    GyroPose.header.frame_id = "ImuPostureEstimate";

    GyroPose.static_posture = anyData.static_pose;
    GyroPose.dynamic_posture = anyData.dynamic_pose;

    m_McuGyroPosePub.publish(GyroPose);
}

void RosAdapter::PubMcuPowerCtlRsp(const boost::any& data)
{
    auto pInfo = boost::any_cast<MCUPowerRsp>(data);

    deebot_msgs::MCUCommonControlRsp msg;

    msg.msgId = 9;
    msg.code  = static_cast<int8_t>(pInfo.code);
    msg.data  = static_cast<uint32_t>(pInfo.power);

    m_McuPowerCtlRspPub.publish(msg);

    ECO_INFO_NEW("[PubMcuPowerCtlRsp] code={}, data={}", msg.code, msg.data);
}

void RosAdapter::PubHeartControlReply(const boost::any& data)
{
    auto reply = boost::any_cast<MCUHeartControlReply>(data);

    deebot_msgs::SetHeartRsp msg;

    msg.mode = reply.mode;
    msg.code = reply.code_int32;

    m_HeartControlReplyPub.publish(msg);

    ECO_INFO_NEW("[PubHeartControlReply] mode={}, code={} (0=success, !0=fail)", 
                 msg.mode, msg.code);
}

void
RosAdapter::OnMotorCtrl(const deebot_msgs::MotorControlReq::ConstPtr& msg)
{
    std::vector<MCUMotorControl> McuMotorCtrl;
    MCUMotorControl ctrl;

    McuMotorCtrl.clear();

    for(auto it = msg->motor_controls.begin();it != msg->motor_controls.end();it++)
    {
        ctrl.index = it->idx;
        ctrl.action = it->type;
        ctrl.content = it->content;
        ctrl.time = it->time;
        ctrl.cmdType = it->cmdType;

        McuMotorCtrl.push_back(ctrl);
    }

    m_uartDispatcher->SetMCUMotorControl(McuMotorCtrl,msg->task_id);
}

void
RosAdapter::OnTempCtrl(const deebot_msgs::TempControlReq::ConstPtr& msg)
{
    std::vector<MCUTempControl> McuTempCtrl;
    MCUTempControl ctrl;

    McuTempCtrl.clear();

    for(auto it = msg->temp_controls.begin();it != msg->temp_controls.end();it++)
    {
        ctrl.index = it->index;
        ctrl.mode = it->mode;
        ctrl.value = it->value;

        McuTempCtrl.push_back(ctrl);
    }

    m_uartDispatcher->SetMCUTempControl(McuTempCtrl,msg->msg_id);
}

void
RosAdapter::OnMcuVerInfo(const deebot_msgs::MCUVersionReq::ConstPtr& msg)
{
    if (m_uartDispatcher){
    	m_uartDispatcher->GetMCUVersionRequest(msg->msg_id);
    }
}

void
RosAdapter::OnMcuOtaReq(const deebot_msgs::MCUOtaReq::ConstPtr& msg)
{
    if (m_uartDispatcher){
    	m_uartDispatcher->SetMcuOTARequest(msg->msg_id);
    }
}

void
RosAdapter::OnPowerOffCtl(const deebot_msgs::PowerOffRsp::ConstPtr& msg)
{
    uint16_t delay_ms = msg->delayTime;

    ros::Time stamp = msg->header.stamp;
    uint32_t seq = msg->header.seq;

    ECO_INFO("Received PowerOffRsp (seq=%u, time=%.3f): delayTime = %u ms", 
             seq, stamp.toSec(), delay_ms);

    m_uartDispatcher->SetMCUPowerOffCtl(delay_ms);
}

void
RosAdapter::OnErpCtl(const deebot_msgs::MCUErpCtl::ConstPtr& msg)
{
    uint8_t erp_mode = msg->mode;

    ros::Time stamp = msg->header.stamp;
    uint32_t seq = msg->header.seq;

    ECO_INFO_NEW("Received Erpctl (seq={}, time={}): erp_mode={}", 
             seq, stamp.toSec(), erp_mode);

    m_uartDispatcher->SetErpCtl(erp_mode);
}

void 
RosAdapter::OnMcuRtcReq(const deebot_msgs::MCURtcReq::ConstPtr& msg)
{
    ros::Time stamp = msg->header.stamp;
    uint32_t seq = msg->header.seq;

    ECO_INFO_NEW("Received MCU RTC Request (seq={}, time={})", seq, stamp.toSec());

    m_uartDispatcher->SendRTCRequestToMCU();
}

void RosAdapter::OnSysTimeSyncCtl(const deebot_msgs::KeyEventNotify::ConstPtr& msg)
{
    const std::string& eventName = msg->eventName;
    const std::string& eventData = msg->eventData;

    ECO_INFO_NEW("Received eventName:{}, eventData:{}", eventName.c_str(),eventData.c_str());
   
    if (eventName != "TimeSynchronize")
        return;
    
    try
    {
	nJson j = nJson::parse(eventData, nullptr, false);
        if (!j.contains("time"))
        {
            ECO_ERROR_NEW("Missing 'time' field in eventData: {}", eventData);
            return;
        }

        time_t timestamp = static_cast<time_t>(j["time"].get<int64_t>());

        ECO_INFO_NEW("Parsed timestamp from eventData: {}", timestamp);

        m_uartDispatcher->SendSysTimeToMCU(timestamp);
    }
    catch (const std::exception& e)
    {
        ECO_ERROR_NEW("Failed to parse eventData JSON: {}", e.what());
    }
}

void
RosAdapter::OnMcuPowerCtlReq(const deebot_msgs::MCUCommonControlReq::ConstPtr& msg)
{
    std::vector<MCUPower> powerCtl;
    MCUPower pctl;

    powerCtl.clear();

    pctl.type   = msg->type;
    pctl.power = msg->data;

    powerCtl.push_back(pctl);

    ECO_INFO_NEW("OnMcuPowerCtlReq type:{}, power:{}", pctl.type, pctl.power);
    m_uartDispatcher->SetMCUPowerReq(powerCtl,msg->msgId);
}

void
RosAdapter::OnSetHeartReq(const deebot_msgs::SetHeartReq::ConstPtr& msg)
{
    MCUHeartControl heartCtrl;

    heartCtrl.mode = msg->mode;
    heartCtrl.heart = msg->heart;
    heartCtrl.time = msg->time;

    ECO_INFO_NEW("OnSetHeartReq mode={}, heart={}, time={}", 
                 heartCtrl.mode, heartCtrl.heart, heartCtrl.time);
    
    m_uartDispatcher->SetMCUHeartControl(heartCtrl, 0);  // task_id 设为 0
}

#if 0
void
RosAdapter::OnEyeCtrl(const deebot_msgs::EyeControlReq::ConstPtr& msg)
{
    EyeControl ctrl;
    ctrl.mode   = msg->mode;
    ctrl.status = msg->status;
    ctrl.freq   = msg->freq;
    ctrl.count  = msg->count;

    std::vector<EyeControl> vec;
    vec.push_back(ctrl);

    ECO_INFO("EyeCtl Sub mode:%d, status:%d, freq:%d, count:%d\n", ctrl.mode, ctrl.status, ctrl.freq, ctrl.count);
    m_uartDispatcher->SetEyeControl(vec,msg->msg_id);
}
#endif
