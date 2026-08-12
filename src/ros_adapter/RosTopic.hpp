#pragma once

#include <string>

// clang-format off
namespace stark_power_manager
{
const std::string TOPIC_TEMP_CONTROL_REQ = "/petrobot/temp_control_req";
const std::string TOPIC_MCUVER_CONTROL_REQ = "/petrobot/mcuver_info_req";
const std::string TOPIC_MCUOTA_CONTROL_REQ = "/petrobot/mcuota_req";
const std::string TOPIC_IMU        = "/petrobot/bodyimu";
const std::string TOPIC_MCU_KEY_INFO       = "/petrobot/mcu_key_info";
const std::string TOPIC_POWEROFF_MSG_REQ       = "/petrobot/poweroff_rsp";
const std::string TOPIC_QRCODE_PAIR_REQ       = "/petrobot/qrcode_pair_req";

const std::string TOPIC_MCUOTA_CONTROL_RSP = "/petrobot/mcuota_rsp";

const std::string TOPIC_MCU_CURRENT = "/petrobot/mcu_current";
const std::string TOPIC_MCU_TEMP = "/petrobot/mcu_temp";
const std::string TOPIC_MCU_EXCODE = "/petrobot/exception_report";
const std::string TOPIC_MCU_GYROPOSE = "/petrobot/imu_posture";
const std::string TOPIC_ERPCTL_MSG_REQ = "/petrobot/mcu_erp_ctl";
const std::string TOPIC_SYSTIME_SYNC_REQ = "/petrobot/key_event_notify";
const std::string TOPIC_MCURTCTIME_REQ = "/petrobot/mcu_rtc_req";
const std::string TOPIC_MCU_POWER_SET_REQ = "/petrobot/mcu_comm_ctrl_req";
const std::string TOPIC_MCU_POWER_SET_RSP = "/petrobot/mcu_comm_ctrl_rsp";
const std::string TOPIC_MCU_SET_HEART_REQ = "/petrobot/set_heart_req";
const std::string TOPIC_MCU_HEART_CONTROL_REPLY = "/petrobot/set_heart_rsp";
}  // namespace stark_power_manager
// clang-format on
