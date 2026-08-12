#include <cmath>
#include <chrono>
#include <log_helper/LogHelper.h>

#include "UartFrame.hpp"
#include "utility/Utility.hpp"

using namespace stark_power_manager;

uint8_t
UartFrame::UartFrameCheckCrc(const uint8_t* data, uint16_t len)
{
    unsigned char crc = 0;
//    GetStrCrc8(&crc, (unsigned char*)"SA", 2);
    GetStrCrc8(&crc, (unsigned char*)data, len);
    return crc;
}
uint8_t
UartFrame::UartFrameCheckCrc(const std::vector<uint8_t>& data, const uint8_t& start_index)
{
    unsigned char crc = 0;
//    GetStrCrc8(&crc, (unsigned char*)"SA", 2);
    GetStrCrc8(&crc, (unsigned char*)(&data[start_index]), data.size() - 3);
    ECO_INFO("data.size() = %zu, crc = 0x%x\n", data.size(), crc);
    return crc;
}

bool
UartFrame::UartFrameFindHead(const uint8_t* data, const uint8_t& len, uint8_t& pos)
{
    pos = 0;
    // not whole frame
    if (len < MIN_PKG_LEN)
    {
        return false;
    }
    // find head
    for (pos = 0; pos < (len - sizeof(HEAD)); pos++)
    {
        // find ok
        if (!std::memcmp(HEAD, data + pos, sizeof(HEAD)))
        {
            return true;
        }
        // not find , continue
    }
    return false;
}

std::vector<uint8_t>
UartFrame::DecodeEscapeCharacter(const uint8_t* input_buffer, const uint8_t& size)
{
    std::vector<uint8_t> output;
    for (int i = 0; i < size - 1; i++)
    {
        // ECO_INFO("input_buffer[%d] = 0x%x  ",i, input_buffer[i]);
        if (input_buffer[i] == '\\' && input_buffer[i + 1] == 0x01)
        {
            // ECO_INFO("push back 0x%x\r\n ",'\\');
            output.push_back('\\');
            i++;
        }
        else if (input_buffer[i] == '\\' && input_buffer[i + 1] == 0x02)
        {
            // ECO_INFO("push back 0x%x\r\n ",'`');
            output.push_back('`');
            i++;
        }
        else if (input_buffer[i] == '\\' && input_buffer[i + 1] == 0x03)
        {
            // ECO_INFO("push back 0x%x\r\n ",'\n');
            output.push_back('\n');
            i++;
        }
        else
        {
            // ECO_INFO("push back 0x%x\r\n",input_buffer[i]);
            output.push_back(input_buffer[i]);
        }
        if (i == size - 2)
        {
            // ECO_INFO("push back 0x%x\r\n",'\n');
            output.push_back('\n');
        }
    }
    // ECO_INFO("output.size = %d\r\n", output.size());
    return output;
}

std::vector<uint8_t>
UartFrame::EncodeEscapeCharacter(const std::vector<uint8_t>& data, const uint8_t& start_index)
{
    std::vector<uint8_t> res;
    res.clear();
    for (int i = 0; i < data.size(); i++)
    {
        if (i >= start_index)
        {
            if (data[i] == '\\')
            {
                res.push_back('\\');
                res.push_back(0x01);
            }
            else if (data[i] == '`')
            {
                res.push_back('\\');
                res.push_back(0x02);
            }
            else if (data[i] == '\n')
            {
                res.push_back('\\');
                res.push_back(0x03);
            }
            else
            {
                res.push_back(data[i]);
            }
        }
        else
        {
            res.push_back(data[i]);
        }
    }
    return std::move(res);
}

uint16_t
UartFrame::UartFrameUnpack(const uint8_t* data, const uint16_t& len, UartFramePkg& pkg, bool& isOk)
{
    // ECO_INFO("----------------");
    // ECO_INFO("\r\n----------------\r\n");
    // 完整性检查
    //< 查找数据帧尾: '\n'， 一直查, 直到查到最后一个'\n'(即碰到帧头)
    isOk = false;
    int end_index = 0;
    for (int i = 0; i < len; i++)
    {
        // ECO_INFO("--->>>data[%d] = 0x%x\r\n", i, *(data + i));

        if (*(data + i) == 0x0a)
        {
            end_index = i;
            // ECO_INFO("666found frame_end, end_index = %d\n", end_index);
            //< 校验是否是粘包,粘包直接裁出第一包数据
            if (end_index + 3 < len)
            {
                //0x60, 0x53, 0x41
                if (((*(data + i + 1)) == 0x60) && ((*(data + i + 2)) == 0x53) && ((*(data + i + 3)) == 0x41))
                {
                    // TODO(colin): 先注释
                    // ECO_INFO("Found another header,break... \r\n");
                    break;
                }
            }
        }
    }

    //< 未找到帧尾'\n'
    if (end_index == 0)
    {
        // for (int i = 0; i < len; i++)
        // {
        //     ECO_INFO("--->>> 1212 data[%d] = 0x%x", i, *(data + i));
        // }
        ECO_TRACE_NEW("Frame is not ok, wait to complete");
        return 0;
    }
    int frame_size = end_index + 1;  //< 补全\n

    /**< 处理单帧异常数据：一帧数据出现多个帧头的情况 */
    auto headerIndex = GetTrimHeaderIndex(data, frame_size);

    static int dropCount = 0;
    if (headerIndex)
    {
        dropCount++;
        ECO_WARN_NEW("Find multi headers, one tail, drop incomplete data, count: {}", dropCount);

        auto tmpVec = std::vector<uint8_t>(data, data + headerIndex);
        PrintHexVector(tmpVec, "[error]Drop incomplete data");
    }

    /**< 单帧数据长度 */
    auto singleSize = frame_size - headerIndex;

    //< 判定是否有转义字符,如果有转义字符,实现转义，转义前后,数据的长度可能会变化，返回到外部的数据长度以frame_size为准
    auto decode_buffer = DecodeEscapeCharacter(data + headerIndex, singleSize);
    // ECO_INFO("frame_size = %d, decode_buffer.size() = %d", frame_size, decode_buffer.size());

    //< 计算从msg_type 到crc中间所有数据的crc8
    uint8_t crc = UartFrameCheckCrc(&decode_buffer[sizeof(HEAD)], decode_buffer.size() - 5);
    if (decode_buffer[decode_buffer.size() - 2] != crc)
    {
        ECO_WARN("frame check crc error error: pkg_type = %d, decode_buffer.size() = %zu, input_crc = %x, cac_crc = %x, "
                 "singleSize = %d",
                 decode_buffer[3], decode_buffer.size(), decode_buffer[decode_buffer.size() - 2], crc,
                 static_cast<int>(singleSize));

        PrintHexVector(decode_buffer, "[error]Crc error");
        std::cout << std::endl;

        return frame_size;
    }
    else
    {
        // TODO(colin): 先注释
        // ECO_INFO("check crc pass: pkg_type = %d, decode_buffer.size() = %zu, input_crc = %x, cac_crc = %x\r\n",
        //          decode_buffer[3], decode_buffer.size(), decode_buffer[decode_buffer.size() - 2], crc);
    }
    pkg.type = decode_buffer[sizeof(HEAD)];
    uint8_t id_buffer[4];
    id_buffer[0] = decode_buffer[sizeof(HEAD) + 1];
    id_buffer[1] = decode_buffer[sizeof(HEAD) + 2];
    id_buffer[2] = decode_buffer[sizeof(HEAD) + 3];
    id_buffer[3] = decode_buffer[sizeof(HEAD) + 4];

    uint32_t id = id_buffer[0];
    id += (id_buffer[1] << 8);
    id += (id_buffer[2] << 16);
    id += (id_buffer[3] << 24);
    pkg.id = id;
    pkg.cmd = decode_buffer[8];
    pkg.sub_cmd = decode_buffer[9];

    if (frame_size > MIN_PKG_LEN)
    {
        pkg.payload.resize(decode_buffer.size() - MIN_PKG_LEN - 2);
        bcopy(&decode_buffer[MIN_PKG_LEN], pkg.payload.data(), decode_buffer.size() - MIN_PKG_LEN - 2);
    }
    isOk = true;
    return frame_size;
}


const std::vector<uint8_t>
UartFrame::UartFrameBuild(const UartFramePkg& frame)
{
    std::vector<uint8_t> tx;
    tx.clear();
    //< 添加framez帧头
    for (int i = 0; i < static_cast<int>(sizeof(HEAD)); i++)
    {
        tx.push_back(HEAD[i]);
    }
    uint8_t id_buffer[4];
    id_buffer[0] = frame.id;
    id_buffer[1] = frame.id >> 8;
    id_buffer[2] = frame.id >> 16;
    id_buffer[3] = frame.id >> 24;

    tx.push_back(frame.type);
    tx.push_back(id_buffer[0]);
    tx.push_back(id_buffer[1]);
    tx.push_back(id_buffer[2]);
    tx.push_back(id_buffer[3]);
    tx.push_back(frame.cmd);
    tx.push_back(frame.sub_cmd);
    // ECO_INFO("tx.size1 = %d\n", tx.size());
    if (frame.payload.size())
    {
        tx.insert(tx.end(), frame.payload.begin(), frame.payload.end());
    }
    // ECO_INFO("before to caculate crc: \r\n");
    // for(int i = 0 ; i < tx.size(); i++)
    // {
    //     ECO_INFO("0x%x, ",tx[i]);
    // }
    // ECO_INFO("\r\n");
    // ECO_INFO("tx.size2 = %d\n", tx.size());
    auto crc = UartFrameCheckCrc(tx, 3);  //< head不参与计算crc8
    tx.push_back(crc);
    // ECO_INFO("tx.size3 = %d\n", tx.size());
    // ECO_INFO("after to caculate crc: \r\n");
    // for(int i = 0 ; i < tx.size(); i++)
    // {
    //     ECO_INFO("0x%x, ",tx[i]);
    // }
    // ECO_INFO("\r\n");
    //< 转换转义字符
    auto res = EncodeEscapeCharacter(tx);
    // ECO_INFO("after to convert: \r\n");
    // for(int i = 0 ; i < res.size(); i++)
    // {
    //     ECO_INFO("0x%x, ",res[i]);
    // }
    // ECO_INFO("\r\n");
    //< 添加frame结束符
    res.push_back('\n');
    // ECO_INFO("tx.size4 = %d\n", res.size());
    return res;
}
