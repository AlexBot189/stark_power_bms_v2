/*
 * BatteryFrame.h — 电池协议帧打包/解包
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 帧格式 (8+N 字节, 无转义):
 *   Byte0:    帧头  0xF1(请求)/0xF2(响应)
 *   Byte1-2:  发送地址 + 接收地址
 *   Byte3-4:  功能码 + 指令码
 *   Byte5:    数据长度 N
 *   Byte6..5+N: 数据域 (N 字节)
 *   Byte6+N:  校验和 (发送地址~数据域累加, 取低8位)
 *   Byte7+N:  帧尾  0xF2(请求)/0xF1(响应)
 */
#pragma once

#include <cstdint>
#include <vector>
#include "BatteryTypes.h"

namespace stark_power_manager {

class BatteryFrame {
public:
    BatteryFrame() = default;
    ~BatteryFrame() = default;

    /*
     * 计算累加和校验: 从 src_addr 到 data 末尾逐字节累加, 取低 8 位
     *
     * 参数:
     *   src_addr  — 发送地址
     *   dst_addr  — 接收地址
     *   func_code — 功能码
     *   cmd_code  — 指令码
     *   data      — 数据域指针
     *   data_len  — 数据域长度
     *
     * 返回: 1 字节校验和
     */
    static uint8_t CalcChecksum(uint8_t src_addr, uint8_t dst_addr,
                                uint8_t func_code, uint8_t cmd_code,
                                const uint8_t* data, uint8_t data_len);

    /*
     * 解包: 从原始缓冲区提取单帧
     *
     * 参数:
     *   buf     — 原始数据缓冲区
     *   buf_len — 缓冲区有效数据长度
     *   pkg     — [输出] 解包后的数据包
     *   consumed— [输出] 消耗的字节数 (含帧尾), 0=数据不完整需继续接收
     *
     * 返回: true=解包成功, false=数据不完整或帧无效
     */
    bool Unpack(const uint8_t* buf, uint16_t buf_len,
                BatteryPkg& pkg, uint16_t& consumed);

    /*
     * 打包: 将 BatteryPkg 组装为发送字节流
     *
     * 参数:
     *   pkg — 待发送的数据包 (is_request 决定帧头帧尾)
     *
     * 返回: 完整帧字节数组 (含帧头+数据+校验+帧尾)
     */
    std::vector<uint8_t> Build(const BatteryPkg& pkg);

private:
    /*
     * 在缓冲区中搜索帧头 (0xF1 或 0xF2)
     *
     * 返回: 帧头在 buf 中的偏移, -1=未找到
     */
    int FindHeader(const uint8_t* buf, uint16_t buf_len);

    /*
     * 从指定偏移开始搜索下一个帧头
     *
     * 返回: 相对 offset 的帧头偏移, -1=未找到
     */
    int FindHeaderFrom(const uint8_t* buf, uint16_t buf_len, uint16_t start);
};

}  /* namespace stark_power_manager */
