#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include "CRC8.hpp"

//< 数据按小端
//< Frame数据帧组成
// <`><SA><MsgType(1B)><id(4B)><cmd(1B)><subCmd(1B)><payload....><crc8(1B)><\n>
//  crc8 of (acktype + id + payload)
//----------------------------------------------------------------------------------
//|Header0 | Header1 | Header2|  MsgType | MsgId   | payload[] | crc8    |   \n    |
//----------------------------------------------------------------------------------
//| 1byte  | 1byte   | 1byte  |  1byte   | 4byte   | N bytes   |  1byte  |  1byte  |
//----------------------------------------------------------------------------------
//   |      ...
//   |      ...
//   |      ...
//   |      ...
//   |
//   |
//                                   |  CMD    | data[]   |
//----------------------------------------------------------
//                                   | 1bytes  | N-1 bytes|
//----------------------------------------------------------

///< ###############################################
///< ###############################################

namespace stark_power_manager
{
constexpr const uint8_t CMD_ROBOT_RUNING_STATE = 0x01;
constexpr const uint8_t CMD_ROBOT_WHELL_CONTROL = 0x02;
constexpr const uint8_t CMD_ROBOT_MODE_CONTROL = 0x03;
constexpr const uint8_t CMD_ROBOT_COMMON_CONTROL = 0x04;
constexpr const uint8_t CMD_ROBOT_SOFT_OTA = 0x05;

constexpr const uint8_t CMD_ROBOT_GET_INFO = 0x06;
constexpr const uint8_t CMD_ROBOT_DBG_COMMAND = 0x07;
constexpr const uint8_t CMD_ROBOT_CBT_DATA = 0x08;

constexpr const uint8_t CMD_ROBOT_STA_CONTROL = 0x11;
constexpr const uint8_t CMD_ROBOT_STA_FUNCTION = 0x12;

constexpr const uint8_t CMD_ROBOT_OPT_ARM = 0x81;
constexpr const uint8_t CMD_ROBOT_SLOW_STATUS = 0x82;
constexpr const uint8_t CMD_ROBOT_FAST_STATUS = 0x83;
constexpr const uint8_t CMD_ROBOT_PUSH_VERINFO = 0x84;

constexpr const uint8_t CMD_ROBOT_FACT_RUNINFO = 0x85;
constexpr const uint8_t CMD_ROBOT_GET_REALTIME_SENSOR = 0x88;
constexpr const uint8_t CMD_ROBOT_STA_STATUS = 0x91;
constexpr const uint8_t CMD_ROBOT_STA_FUNCSTATUS = 0x92;

//<`><SA><MsgType(1B)><id(4B)><cmd(1B)><subCmd(1B)><payload....><crc8(1B)><\n>
//< k850 msg_define
constexpr const uint8_t HEAD_KEY0 = 0x60;  //"`"
constexpr const uint8_t HEAD_KEY1 = 0x53;  //"S"
constexpr const uint8_t HEAD_KEY2 = 0x41;  //"A"
constexpr const uint8_t HEAD[] = { HEAD_KEY0, HEAD_KEY1, HEAD_KEY2 };
constexpr const uint8_t MIN_PKG_LEN = 10;

constexpr const uint8_t CMD_TYPE_REQUEST = 0x00;
constexpr const uint8_t CMD_TYPE_RESPONSE = 0x01;
constexpr const uint8_t CMD_TYPE_REPORT = 0x02;

struct UartFramePkg
{
    uint8_t type;
    uint32_t id;
    uint8_t cmd;
    uint8_t sub_cmd;
    std::vector<uint8_t> payload;
};

class UartFrame
{
public:
    UartFrame() = default;
    ~UartFrame() = default;

    uint8_t UartFrameCheckCrc(const uint8_t* data, uint16_t len);
    uint8_t UartFrameCheckCrc(const std::vector<uint8_t>& data, const uint8_t& start_index = 0);
    std::vector<uint8_t> DecodeEscapeCharacter(const uint8_t* input_buffer, const uint8_t& size);
    std::vector<uint8_t> EncodeEscapeCharacter(const std::vector<uint8_t>& data, const uint8_t& start_index = 3);

    uint16_t UartFrameUnpack(const uint8_t* data, const uint16_t& len, UartFramePkg& pkg, bool& isok);
    bool UartFrameFindHead(const uint8_t* data, const uint8_t& len, uint8_t& pos);
    const std::vector<uint8_t> UartFrameBuild(const UartFramePkg& frame);

private:
};
}  // namespace stark_power_manager
