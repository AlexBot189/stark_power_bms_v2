/*
 * BatteryDispatcher.h — 电池包串口通信调度
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 职责:
 *   - 串口收发 (独立 /dev/tty 设备)
 *   - 请求-响应超时匹配 (单请求模式)
 *   - 周期性轮询电池数据 (SOC/电压/电流/温度)
 *   - 控制指令下发 (停止供电/充放电切换)
 *   - 数据变化通知 (Observer 模式)
 *
 * 线程模型:
 *   - recv_thread: 阻塞读串口 → 解包 → 匹配响应/丢弃 → 通知Observer
 *   - send_thread: 从发送队列取帧 → write 串口
 *   - poll_thread: 按配置周期组装查询帧 → 入队发送
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <termios.h>

#include "BatteryFrame.h"
#include "BatteryTypes.h"
#include "config/Defines.hpp"

namespace stark_power_manager {

/* 电池数据变化回调 (替代 boost::any, 类型安全) */
using BatteryStatusCallback  = std::function<void(const BatteryStatus&)>;
using BatteryFaultCallback   = std::function<void(const BatteryFault&)>;
using BatteryInfoCallback    = std::function<void(const BatteryInfo&)>;

/* 响应回调: (timeout, func_code, cmd_code, data) */
using BatteryResponseCb = std::function<void(bool timeout,
                                             uint8_t func_code,
                                             uint8_t cmd_code,
                                             const std::vector<uint8_t>& data)>;

/* 串口配置 */
struct BatteryUartOption {
    std::string tty_device{ "/dev/ttyS2" };         /* 串口设备路径 */
    speed_t     baud_rate{ B115200 };               /* 波特率 */
};

/* 轮询配置 */
struct BatteryPollConfig {
    uint32_t period_ms{ 500 };                      /* 核心数据周期 ms (2004电压+5002电流温度) */
    uint32_t soc_period_ms{ 2000 };                 /* SOC/容量周期 ms (5004, 变化缓慢) */
    uint32_t fault_period_ms{ 30000 };              /* 故障信息周期 ms (503B, 低频) */
    bool     enable_voltage{ true };                /* 轮询电芯电压 0x2004 */
    bool     enable_current_temp{ true };           /* 轮询电流温度 0x5002 */
    bool     enable_soc_capacity{ true };           /* 轮询 SOC 容量 0x5004 */
    bool     enable_fault{ true };                  /* 轮询故障信息 0x503B */
    bool     enable_fault_count{ false };           /* 轮询历史故障次数 0x503C */
    bool     enable_charge_current{ false };        /* 轮询充电电流 0x2005 */
};

class BatteryDispatcher {
public:
    explicit BatteryDispatcher(const BatteryUartOption& uart_opt);
    ~BatteryDispatcher();

    /* 禁止拷贝 */
    BatteryDispatcher(const BatteryDispatcher&) = delete;
    BatteryDispatcher& operator=(const BatteryDispatcher&) = delete;

    static std::shared_ptr<BatteryDispatcher> Create(const BatteryOption& cfg);
    static speed_t IntToBaudRate(int baud);

    /*
     * 初始化: 打开串口 + 启动收发线程
     * 返回: true=成功
     */
    bool Init();

    /*
     * 销毁: 停止所有线程 + 关闭串口
     */
    void Destroy();

    /*
     * 设置轮询配置 (Init 之前调用)
     */
    void SetPollConfig(const BatteryPollConfig& cfg);

    /*
     * 注册数据回调 (Observer 模式)
     * listener 的生命周期由调用者管理
     */
    void RegisterStatusCallback(BatteryStatusCallback cb);
    void RegisterFaultCallback(BatteryFaultCallback cb);
    void RegisterInfoCallback(BatteryInfoCallback cb);

    /*
     * 注销所有回调, 防止 shutdown 时回调调用已析构的 Observer
     */
    void ClearCallbacks();

    /*
     * 发送控制指令 (异步, 不等待响应)
     *
     * 参数:
     *   func_code — 功能码
     *   cmd_code  — 指令码
     *   data      — 数据域
     */
    void SendControl(uint8_t func_code, uint8_t cmd_code,
                     const std::vector<uint8_t>& data);

    void SetChargerStatus(bool plugged);
    void ControlMOS(uint8_t chg_mos, uint8_t dischg_mos);

    /*
     * 发送请求并等待响应 (同步, 带超时)
     *
     * 参数:
     *   func_code   — 功能码
     *   cmd_code    — 指令码
     *   req_data    — 请求数据域
     *   rsp_data    — [输出] 响应数据域
     *   timeout_ms  — 超时 ms (默认 100ms)
     *
     * 返回: true=成功收到响应, false=超时或错误
     */
    bool SendRequest(uint8_t func_code, uint8_t cmd_code,
                     const std::vector<uint8_t>& req_data,
                     std::vector<uint8_t>& rsp_data,
                     uint32_t timeout_ms = 100);

    /*
     * 获取当前电池状态
     */
    BatteryStatus GetStatus() const;

    /*
     * 获取当前故障信息
     */
    BatteryFault GetFault() const;

    FaultCounters GetFaultCounters() const;

    /*
     * 获取当前电池信息
     */
    BatteryInfo GetInfo() const;

    /*
     * 主动查询基本信息 (版本+ID)
     * 结果通过 RegisterInfoCallback 回调通知
     */
    void QueryInfo();

    /*
     * 查询运行状态
     */
    bool IsRunning() const;

    /*
     * 获取串口设备路径
     */
    const std::string& GetDevice() const;

private:
    /* 线程入口 */
    void RecvThreadFunc();
    void SendThreadFunc();
    void PollThreadFunc();

    /* 串口操作 */
    bool OpenUart();
    void CloseUart();

    /* 解析响应帧为结构化数据 */
    void ParseResponse(const BatteryPkg& pkg);

    /* 解析各指令响应 */
    void ParseCellVoltage(const BatteryPkg& pkg);        /* 0x2004 */
    void ParseCurrentTemp(const BatteryPkg& pkg);        /* 0x5002 */
    void ParseSocCapacity(const BatteryPkg& pkg);        /* 0x5004 */
    void ParseFaultInfo(const BatteryPkg& pkg);          /* 0x503B */
    void ParseVersion(const BatteryPkg& pkg);            /* 0x1003 */
    void ParseId(const BatteryPkg& pkg);                 /* 0x1005 */
    void ParseChargeCurrent(const BatteryPkg& pkg);      /* 0x2005 */
    void ParsePowerCtrlAck(const BatteryPkg& pkg);       /* 0x9001 */
    void ParseChargeSwitchAck(const BatteryPkg& pkg);    /* 0x9003 */
    void ParseQrCode(const BatteryPkg& pkg);             /* 0x1007 */
    void ParseFaultCounters(const BatteryPkg& pkg);    /* 0x503C */

    void SendQueryRequest(uint8_t func_code, uint8_t cmd_code);
    void TryMatchResponse(const BatteryPkg& pkg);

    /* 通知所有 Observer */
    void NotifyStatusCallbacks();
    void NotifyFaultCallbacks();
    void NotifyInfoCallbacks();

    BatteryUartOption m_uart_opt;
    BatteryPollConfig m_poll_cfg;

    int m_fd{ -1 };

    /* 线程 */
    std::thread m_recv_thread;
    std::thread m_send_thread;
    std::thread m_poll_thread;
    std::atomic<bool> m_running{ false };

    /* 发送队列 */
    std::deque<std::vector<uint8_t>> m_send_queue;
    std::mutex m_send_mutex;
    std::condition_variable m_send_cv;

    /* 接收缓冲 (环形, 固定大小) */
    static constexpr uint16_t RX_BUF_SIZE = 4096;
    uint8_t  m_rx_buf[RX_BUF_SIZE];
    uint16_t m_rx_head{ 0 };
    uint16_t m_rx_tail{ 0 };

    /* 请求-响应匹配 (单请求模式) */
    struct PendingRequest {
        bool     active{ false };
        uint8_t  func_code;
        uint8_t  cmd_code;
        std::vector<uint8_t> rsp_data;
        bool     done{ false };
    };
    PendingRequest m_pending_req;
    std::mutex      m_req_mutex;
    std::condition_variable m_req_cv;

    /* 电池数据 (mutex 保护) */
    mutable std::mutex m_data_mutex;
    BatteryStatus m_status;
    BatteryFault  m_fault;
    FaultCounters m_fault_counters;
    BatteryInfo   m_info;

    /* Observer 回调列表 */
    std::mutex m_cb_mutex;
    std::vector<BatteryStatusCallback> m_status_cbs;
    std::vector<BatteryFaultCallback>  m_fault_cbs;
    std::vector<BatteryInfoCallback>   m_info_cbs;

    BatteryFrame m_frame;
};

}  /* namespace stark_power_manager */
