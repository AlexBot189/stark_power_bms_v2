/*
 * @Author: colin yuanzhi.yang@ecovacs.com
 * @Date: 2023-11-22 18:17:58
 * @LastEditors: colin yuanzhi.yang@ecovacs.com
 * @LastEditTime: 2024-07-01 10:59:33
 * @FilePath: /stark_power_manager/src/protocol/UartDispatcher.hpp
 * @Description: 串口通信
 */
#pragma once

#include <map>
#include <tuple>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <type_traits>
#include <functional>
#include <termios.h>
#include <ctime>
#include <sys/time.h>
#include <unordered_map>
#include <boost/any.hpp>
#include <ros/ros.h>
#include "Defines.hpp"
#include "UartFrame.hpp"
#include "utility/McuUartType.hpp"
#include "IMsgInternalDispatcher.hpp"
#include <log_helper/LogHelper.h>
#include "3rd_party/concurrentqueue/concurrentqueue.h"

namespace stark_power_manager
{
class TouchMonitorAdapter;
class UartDispatcher : public IMsgInternalDispatcher
{
public:
    UartDispatcher(const UartOption& uartOption);

    UartDispatcher(const UartOption& uartOption, std::shared_ptr<ros::NodeHandle> nh);

    ~UartDispatcher();

    bool
    InitDispatcher() override;

    bool
    DestroyDispatcher() override;

    void
    Send(const std::string& data) override;

    std::string
    OnRecvData() override;

    void
    RegisterObserver(ListenerType type, std::shared_ptr<IListener> listener) override;

    void
    SetTouchAdapter(std::shared_ptr<TouchMonitorAdapter> touch_adapter);

    void
    RemoveObserver(ListenerType type, std::shared_ptr<IListener> listener) override;

    void
    NotifyObserver(const boost::any& data) override;

private:
    bool
    InitUart();

    int
    CloseUart();

    void
    HeartThreadFunc();

    void
    RecvThreadFunc();

    int
    DataWait();

    void
    SendThreadFunc();

    bool
    SendData(const std::string& data);

    void
    OnRecvData(const UartFramePkg& pkg);

    int
    GetActualBaudRate(speed_t baud);

    void
    DecodeMCUPackFastStatusMsg(const UartFramePkg& pkg);

    void
    DecodeImuPackMsg(const UartFramePkg& pkg);

    std::tuple<int, double>
    PublishOdometry(const int32_t& left_encode, const int32_t& right_encode, const double& yaw);

    void
    DecodeRtcPackMsg(const UartFramePkg& pkg);

    void
    DecodeWfSignalMsg(const UartFramePkg& pkg);

    void
    DecodeDockSignalMsg(const UartFramePkg& pkg);

    void
    DecodeOnOffMsg(const UartFramePkg& pkg);

    void
    DecodePhyKeyMsg(const UartFramePkg& pkg);

    void
    DecodeMcuStationCommMsg(const UartFramePkg& pkg);

    void
    DecodeBattInfoMsg(const UartFramePkg& pkg);

    void
    DecodeStationCtrlRspMsg(const UartFramePkg& pkg);

    void
    DecodeMotorCtrlRspMsg(const UartFramePkg& pkg);

    void
    DecodeLedCtrlRspMsg(const UartFramePkg& pkg);
    
    void
    DecodeTouchMsg(const UartFramePkg& pkg);

    void
    DecodeMCUVerionMsg(const UartFramePkg& pkg);

    void
    DecodeMCUOtaMsg(const UartFramePkg& pkg);

    void
    DecodeMCUCurrentMsg(const UartFramePkg& pkg);

    void
    DecodeMCUTempMsg(const UartFramePkg& pkg);

    void
    DecodeMCUEcMsg(const UartFramePkg& pkg);
    
    void
    DecodeFactoryResetMsg(const UartFramePkg& pkg);
    
    void
    DecodePowerCtlMsg(const UartFramePkg& pkg);
    
    void
    DecodeMCUGyroPoseMsg(const UartFramePkg& pkg);

    void
    DecodeServoCtrlRspMsg(const UartFramePkg& pkg);

    void
    DecodeAgingTestMsg(const UartFramePkg& pkg);
public:
    inline ImuData
    GetImuData()
    {
        return m_imuData;
    }

    inline OdomData
    GetOdomData()
    {
        return m_odomData;
    }

    inline MCUPackFastStatus
    GetOdomState()
    {
        return m_odomState;
    }

    void
    InitWheelMotorSpeed(WheelMotorSpeed* speed, int realSpeed);

    bool
    SetMCUStationControl(const std::vector<MCUSetStationControl>& request,const uint32_t& task_id = 0);

    bool
    SetMCUMotorControl(const std::vector<MCUMotorControl>& request,const uint32_t& task_id = 0);

    bool
    SetMCUTempControl(const std::vector<MCUTempControl>& request,const uint32_t& task_id = 0);

    bool
    GetMCUVersionRequest(const uint32_t& task_id = 0);

    bool
    SetMcuOTARequest(const uint32_t& task_id = 0);

    bool
    SetMCUStationComm(const std::vector<uint8_t>& request,const uint32_t& task_id = 0);

    bool
    SetLedControl(const std::vector<LedControl>& request,const uint32_t& task_id = 0);

    bool
    SetEyeControl(const std::vector<EyeControl>& request,const uint32_t& task_id = 0);
    
    bool
    SetMCUPowerOffCtl(uint16_t delay_ms);
 
    bool
    SetMCUPowerReq(const std::vector<MCUPower>& request,const uint32_t& task_id = 0);

    void SendTouchEventToMCU(uint8_t part, uint8_t force);
    
    void SendRTCRequestToMCU();

    void SendSysTimeToMCU(time_t timestamp);

    bool
    SetErpCtl(uint8_t mode);

    bool
    SetCommonMotorControl(const MCUSetCommonMotorStatusRequest& request);

    bool
    SetIRCalibrationValue();

    bool
    SetGyroResetCmd(const uint8_t& option);

    bool
    SetMCUHeartControl(const MCUHeartControl& request, const uint32_t& task_id = 0);

private:
    void
    DecodeHeartControlReplyMsg(const UartFramePkg& pkg);

private:
    std::shared_ptr<ros::NodeHandle> m_rosNh{ nullptr };

    std::atomic_bool m_bIsRun{ false };
    std::atomic_int m_iOverCnt{ 0 };
    std::atomic_int m_iCollide{ 0 };

    int m_fd{ 0 };
    int m_iRxTop{ 0 };
    int m_iRxStart{ 0 };

    std::vector<u_int8_t> m_RxBuffer;

    time_t m_rtcTime;
    ImuData m_imuData;
    OdomData m_odomData;
    RtcData m_rtcData;
    MCUPackFastStatus m_odomState;
    UartOption m_uartOption;
    McuState m_eStatus{ McuState::MCU_IDLE };
    MCUOnOffSensorStatus m_mCuOnOffStatus;
    MCUPhyKey m_phyKey;
    MCUIRSignalStatus m_mCuIrSignal;

    std::thread m_sendThread;
    std::thread m_recvThread;
    std::thread m_heartThread;
    // std::mutex m_sendQueueMutex;

    moodycamel::ConcurrentQueue<std::string> m_sendQueue;
    moodycamel::ConcurrentQueue<std::string> m_receiveQueue;
    moodycamel::ConcurrentQueue<std::vector<uint8_t>> m_stationCommSendQueue;
    /**< 不同的数据源处理不同 */
    std::unordered_map<ListenerType, std::shared_ptr<IListener>> m_Listeners;

    std::weak_ptr<TouchMonitorAdapter> m_touchAdapter;

};
}  // namespace stark_power_manager
