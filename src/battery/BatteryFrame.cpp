/*
 * BatteryFrame.cpp — 电池协议帧打包/解包实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "BatteryFrame.h"
#include <cstring>
#include <log_helper/LogHelper.h>

namespace stark_power_manager {

uint8_t
BatteryFrame::CalcChecksum(uint8_t src_addr, uint8_t dst_addr,
                           uint8_t func_code, uint8_t cmd_code,
                           const uint8_t* data, uint8_t data_len)
{
    uint32_t sum = 0;

    sum += src_addr;
    sum += dst_addr;
    sum += func_code;
    sum += cmd_code;
    sum += data_len;

    for (uint8_t i = 0; i < data_len; i++) {
        sum += data[i];
    }

    return static_cast<uint8_t>(sum & 0xFF);
}

int
BatteryFrame::FindHeader(const uint8_t* buf, uint16_t buf_len)
{
    if (buf_len == 0) {
        return -1;
    }

    for (uint16_t i = 0; i < buf_len; i++) {
        if (buf[i] == BATTERY_FRAME_HEAD_REQ || buf[i] == BATTERY_FRAME_HEAD_RSP) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int
BatteryFrame::FindHeaderFrom(const uint8_t* buf, uint16_t buf_len, uint16_t start)
{
    if (start >= buf_len) {
        return -1;
    }

    for (uint16_t i = start; i < buf_len; i++) {
        if (buf[i] == BATTERY_FRAME_HEAD_REQ || buf[i] == BATTERY_FRAME_HEAD_RSP) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool
BatteryFrame::Unpack(const uint8_t* buf, uint16_t buf_len,
                     BatteryPkg& pkg, uint16_t& consumed)
{
    consumed = 0;

    /* 数据不足最小帧长 */
    if (buf_len < BATTERY_FRAME_MIN_LEN) {
        return false;
    }

    /* 查找帧头 */
    int hdr_pos = FindHeader(buf, buf_len);
    if (hdr_pos < 0) {
        /* 无有效帧头, 丢弃全部数据 */
        consumed = buf_len;
        return false;
    }

    /* 帧头不在起始位置, 丢弃前方数据 */
    if (hdr_pos > 0) {
        consumed = static_cast<uint16_t>(hdr_pos);
        ECO_WARN("[BatteryFrame] skip %d bytes before header", hdr_pos);
        return false;
    }

    /* hdr_pos==0 已确保 buf[0] 为有效帧头 */
    bool is_request = (buf[0] == BATTERY_FRAME_HEAD_REQ);

    uint8_t src_addr  = buf[1];
    uint8_t dst_addr  = buf[2];
    uint8_t func_code = buf[3];
    uint8_t cmd_code  = buf[4];
    uint8_t data_len  = buf[5];

    /* 完整帧需要: 1(帧头) + 5(定长头) + data_len(数据) + 1(校验) + 1(帧尾) */
    uint16_t total_len = static_cast<uint16_t>(1 + 5 + data_len + 2);

    if (buf_len < total_len) {
        /* 数据不完整, 等待更多数据 */
        consumed = 0;
        return false;
    }

    /* 取校验和字段 */
    uint8_t rx_checksum = buf[1 + 5 + data_len];

    /* 取帧尾 */
    uint8_t rx_tail = buf[1 + 5 + data_len + 1];

    /* 校验帧尾 */
    uint8_t expected_tail = is_request ? BATTERY_FRAME_TAIL_REQ : BATTERY_FRAME_TAIL_RSP;
    if (rx_tail != expected_tail) {
        ECO_WARN("[BatteryFrame] tail mismatch: rx=0x%02X expect=0x%02X",
                 rx_tail, expected_tail);
        /* 帧头误判, 搜索下一个帧头位置重新同步 */
        int next = FindHeaderFrom(buf, buf_len, 1);
        consumed = (next > 0) ? static_cast<uint16_t>(next) : static_cast<uint16_t>(buf_len);
        return false;
    }

    /* 计算期望校验和 */
    uint8_t calc_cs = CalcChecksum(src_addr, dst_addr, func_code, cmd_code,
                                   buf + 1 + 5, data_len);

    if (rx_checksum != calc_cs) {
        ECO_WARN("[BatteryFrame] checksum error: rx=0x%02X calc=0x%02X "
                 "func=0x%02X cmd=0x%02X len=%d",
                 rx_checksum, calc_cs, func_code, cmd_code, data_len);
        /* 帧头误判, 搜索下一个帧头位置重新同步 */
        int next = FindHeaderFrom(buf, buf_len, 1);
        consumed = (next > 0) ? static_cast<uint16_t>(next) : static_cast<uint16_t>(buf_len);
        return false;
    }

    /* 填充输出 */
    pkg.is_request = is_request;
    pkg.src_addr   = src_addr;
    pkg.dst_addr   = dst_addr;
    pkg.func_code  = func_code;
    pkg.cmd_code   = cmd_code;

    if (data_len > 0) {
        pkg.data.assign(buf + 1 + 5, buf + 1 + 5 + data_len);
    } else {
        pkg.data.clear();
    }

    consumed = total_len;
    return true;
}

std::vector<uint8_t>
BatteryFrame::Build(const BatteryPkg& pkg)
{
    std::vector<uint8_t> frame;
    uint8_t data_len = static_cast<uint8_t>(pkg.data.size());

    /* 帧头 */
    frame.push_back(pkg.is_request ? BATTERY_FRAME_HEAD_REQ
                                   : BATTERY_FRAME_HEAD_RSP);

    /* 定长头 */
    frame.push_back(pkg.src_addr);
    frame.push_back(pkg.dst_addr);
    frame.push_back(pkg.func_code);
    frame.push_back(pkg.cmd_code);
    frame.push_back(data_len);

    /* 数据域 */
    if (data_len > 0) {
        frame.insert(frame.end(), pkg.data.begin(), pkg.data.end());
    }

    /* 校验和 */
    uint8_t cs = CalcChecksum(pkg.src_addr, pkg.dst_addr,
                              pkg.func_code, pkg.cmd_code,
                              pkg.data.data(), data_len);
    frame.push_back(cs);

    /* 帧尾 */
    frame.push_back(pkg.is_request ? BATTERY_FRAME_TAIL_REQ
                                   : BATTERY_FRAME_TAIL_RSP);

    return frame;
}

}  /* namespace stark_power_manager */
