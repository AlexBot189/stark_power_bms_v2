/*
 * BatteryDispatcher.cpp — 电池包串口通信调度实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "BatteryDispatcher.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <log_helper/LogHelper.h>

namespace stark_power_manager {

/* ================================================================
 * 构造 / 析构
 * ================================================================ */

BatteryDispatcher::BatteryDispatcher(const BatteryUartOption& uart_opt)
    : m_uart_opt(uart_opt)
{
}

BatteryDispatcher::~BatteryDispatcher()
{
    Destroy();
}

/* ================================================================
 * 静态工厂 / 工具方法
 * ================================================================ */

speed_t
BatteryDispatcher::IntToBaudRate(int baud)
{
    switch (baud) {
        case 0:      return B0;
        case 50:     return B50;
        case 75:     return B75;
        case 110:    return B110;
        case 134:    return B134;
        case 150:    return B150;
        case 200:    return B200;
        case 300:    return B300;
        case 600:    return B600;
        case 1200:   return B1200;
        case 1800:   return B1800;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B115200;
    }
}

std::shared_ptr<BatteryDispatcher>
BatteryDispatcher::Create(const BatteryOption& cfg)
{
    BatteryUartOption uart_opt;
    uart_opt.tty_device = cfg.strTty;
    uart_opt.baud_rate  = IntToBaudRate(cfg.iBaudRate);

    auto dispatcher = std::make_shared<BatteryDispatcher>(uart_opt);

    BatteryPollConfig poll_cfg;
    poll_cfg.period_ms            = cfg.poll_period_ms;
    poll_cfg.soc_period_ms        = cfg.soc_period_ms;
    poll_cfg.fault_period_ms      = cfg.fault_period_ms;
    poll_cfg.enable_voltage       = cfg.enable_voltage;
    poll_cfg.enable_current_temp  = cfg.enable_current_temp;
    poll_cfg.enable_soc_capacity  = cfg.enable_soc_capacity;
    poll_cfg.enable_fault         = cfg.enable_fault;
    poll_cfg.enable_charge_current = cfg.enable_charge_current;
    dispatcher->SetPollConfig(poll_cfg);

    if (!dispatcher->Init()) {
        ECO_ERROR("[BatteryDispatcher] Create failed: Init returned false");
        return nullptr;
    }

    ECO_INFO("[BatteryDispatcher] created: tty=%s baud=%d poll=%ums",
             cfg.strTty.c_str(), cfg.iBaudRate, cfg.poll_period_ms);
    return dispatcher;
}

/* ================================================================
 * 公共接口
 * ================================================================ */

bool
BatteryDispatcher::Init()
{
    if (m_running.load()) {
        ECO_WARN("[BatteryDispatcher] already running");
        return true;
    }

    if (!OpenUart()) {
        ECO_ERROR("[BatteryDispatcher] open uart failed: %s",
                      m_uart_opt.tty_device.c_str());
        return false;
    }

    m_running.store(true);

    m_recv_thread = std::thread(&BatteryDispatcher::RecvThreadFunc, this);
    m_send_thread = std::thread(&BatteryDispatcher::SendThreadFunc, this);
    m_poll_thread = std::thread(&BatteryDispatcher::PollThreadFunc, this);

    ECO_INFO("[BatteryDispatcher] started, device=%s baud=115200",
                 m_uart_opt.tty_device.c_str());
    return true;
}

void
BatteryDispatcher::Destroy()
{
    m_running.store(false);

    /* 唤醒 send 线程避免在 cv.wait 上阻塞 */
    m_send_cv.notify_all();

    /* 唤醒可能阻塞在 SendRequest 上的调用者 */
    {
        std::lock_guard<std::mutex> lk(m_req_mutex);
        m_pending_req.active = false;
        m_pending_req.done   = true;
    }
    m_req_cv.notify_all();

    /*
     * 清空 Observer 回调列表, 防止 join 线程过程中调用已析构对象.
     * 注意: BatteryRosAdapter 析构时也会调用 ClearCallbacks() 主动注销,
     * 此处为保底: 若外部未正确注销, Destroy() 再做一次清空
     */
    ClearCallbacks();

    if (m_poll_thread.joinable()) {
        m_poll_thread.join();
    }
    if (m_send_thread.joinable()) {
        m_send_thread.join();
    }
    if (m_recv_thread.joinable()) {
        m_recv_thread.join();
    }

    CloseUart();

    ECO_INFO("[BatteryDispatcher] destroyed");
}

void
BatteryDispatcher::SetPollConfig(const BatteryPollConfig& cfg)
{
    m_poll_cfg = cfg;
}

void
BatteryDispatcher::RegisterStatusCallback(BatteryStatusCallback cb)
{
    std::lock_guard<std::mutex> lk(m_cb_mutex);
    m_status_cbs.push_back(std::move(cb));
}

void
BatteryDispatcher::RegisterFaultCallback(BatteryFaultCallback cb)
{
    std::lock_guard<std::mutex> lk(m_cb_mutex);
    m_fault_cbs.push_back(std::move(cb));
}

void
BatteryDispatcher::RegisterInfoCallback(BatteryInfoCallback cb)
{
    std::lock_guard<std::mutex> lk(m_cb_mutex);
    m_info_cbs.push_back(std::move(cb));
}

void
BatteryDispatcher::ClearCallbacks()
{
    std::lock_guard<std::mutex> lk(m_cb_mutex);
    m_status_cbs.clear();
    m_fault_cbs.clear();
    m_info_cbs.clear();
    ECO_INFO("[BatteryDispatcher] callbacks cleared");
}

void
BatteryDispatcher::SendControl(uint8_t func_code, uint8_t cmd_code,
                               const std::vector<uint8_t>& data)
{
    BatteryPkg pkg;
    pkg.is_request = true;
    pkg.src_addr   = BATTERY_ADDR_HOST;
    pkg.dst_addr   = BATTERY_ADDR_BMS;
    pkg.func_code  = func_code;
    pkg.cmd_code   = cmd_code;
    pkg.data       = data;

    auto frame = m_frame.Build(pkg);

    {
        std::lock_guard<std::mutex> lk(m_send_mutex);
        m_send_queue.push_back(std::move(frame));
    }
    m_send_cv.notify_one();
}

bool
BatteryDispatcher::SendRequest(uint8_t func_code, uint8_t cmd_code,
                               const std::vector<uint8_t>& req_data,
                               std::vector<uint8_t>& rsp_data,
                               uint32_t timeout_ms)
{
    /* 设置待匹配请求 */
    {
        std::lock_guard<std::mutex> lk(m_req_mutex);
        m_pending_req.active    = true;
        m_pending_req.func_code = func_code;
        m_pending_req.cmd_code  = cmd_code;
        m_pending_req.rsp_data.clear();
        m_pending_req.done      = false;
    }

    /* 通过发送队列发出请求 */
    SendControl(func_code, cmd_code, req_data);

    /* 等待响应或超时 */
    std::unique_lock<std::mutex> lk(m_req_mutex);
    bool ok = m_req_cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                                [this]() { return m_pending_req.done; });

    if (ok && m_pending_req.active && m_pending_req.done) {
        rsp_data = std::move(m_pending_req.rsp_data);
        m_pending_req.active = false;
        return true;
    }

    if (!m_running.load()) {
        ECO_INFO("[BatteryDispatcher] SendRequest aborted: dispatcher shutting down");
    } else {
        ECO_WARN("[BatteryDispatcher] request timeout: func=0x%02X cmd=0x%02X",
                 func_code, cmd_code);
    }
    m_pending_req.active = false;
    return false;
}

BatteryStatus
BatteryDispatcher::GetStatus() const
{
    std::lock_guard<std::mutex> lk(m_data_mutex);
    return m_status;
}

BatteryFault
BatteryDispatcher::GetFault() const
{
    std::lock_guard<std::mutex> lk(m_data_mutex);
    return m_fault;
}

BatteryInfo
BatteryDispatcher::GetInfo() const
{
    std::lock_guard<std::mutex> lk(m_data_mutex);
    return m_info;
}

void
BatteryDispatcher::QueryInfo()
{
    ECO_INFO("[BatteryDispatcher] QueryInfo() triggered");
    SendQueryRequest(BatteryFunc::BASIC_INFO, BatteryCmd::GET_VERSION);
    SendQueryRequest(BatteryFunc::BASIC_INFO, BatteryCmd::GET_ID);
}

bool
BatteryDispatcher::IsRunning() const
{
    return m_running.load();
}

const std::string&
BatteryDispatcher::GetDevice() const
{
    return m_uart_opt.tty_device;
}

/* ================================================================
 * 串口操作
 * ================================================================ */

bool
BatteryDispatcher::OpenUart()
{
    struct termios tty;

    if (access(m_uart_opt.tty_device.c_str(), R_OK | W_OK) != 0) {
        ECO_ERROR("[BatteryDispatcher] no access to %s", m_uart_opt.tty_device.c_str());
        return false;
    }

    m_fd = open(m_uart_opt.tty_device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        ECO_ERROR("[BatteryDispatcher] open(%s) failed: %s",
                      m_uart_opt.tty_device.c_str(), strerror(errno));
        return false;
    }

    if (tcgetattr(m_fd, &tty) < 0) {
        ECO_ERROR("[BatteryDispatcher] tcgetattr failed");
        close(m_fd);
        m_fd = -1;
        return false;
    }

    /* 8N1, 无流控 */
    fcntl(m_fd, F_SETFL, 0);
    tty.c_cflag |= (tcflag_t)(CLOCAL | CREAD | CS8);
    tty.c_cflag &= (tcflag_t)~(CSTOPB | PARENB);
    tty.c_lflag &= (tcflag_t)~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
    tty.c_oflag &= (tcflag_t)~(OPOST);
    tty.c_iflag &= (tcflag_t)~(IXON | IXOFF | INLCR | IGNCR | ICRNL | IGNBRK);

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 15;
    tty.c_cflag &= ~CRTSCTS;

    cfsetispeed(&tty, m_uart_opt.baud_rate);
    cfsetospeed(&tty, m_uart_opt.baud_rate);

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        ECO_ERROR("[BatteryDispatcher] tcsetattr failed");
        close(m_fd);
        m_fd = -1;
        return false;
    }

    tcflush(m_fd, TCIOFLUSH);

    ECO_INFO("[BatteryDispatcher] uart opened: %s", m_uart_opt.tty_device.c_str());
    return true;
}

void
BatteryDispatcher::CloseUart()
{
    if (m_fd >= 0) {
        tcflush(m_fd, TCIOFLUSH);
        close(m_fd);
        m_fd = -1;
    }
}

/* ================================================================
 * 接收线程
 * ================================================================ */

void
BatteryDispatcher::RecvThreadFunc()
{
    prctl(PR_SET_NAME, "batt_recv");
    ECO_INFO("[BatteryDispatcher] recv thread started");

    BatteryPkg pkg;
    uint16_t   consumed  = 0;
    bool       has_frame = false;
    int        timeout_count = 0;

    while (m_running.load()) {
        if (m_fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        fd_set read_fds;
        struct timeval tv = { 0, 15000 };

        FD_ZERO(&read_fds);
        FD_SET(m_fd, &read_fds);

        int ret = select(m_fd + 1, &read_fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EBADF) {
                break;  /* fd closed */
            }
            ECO_WARN("[BatteryDispatcher] select error: %s", strerror(errno));
            continue;
        }

        if (ret == 0) {
            /* 超时, 无数据 */
            ++timeout_count;
            if (timeout_count % 300 == 1) {
                ECO_WARN("[BatteryDispatcher] select timeout x%d (~%.1fs)",
                         timeout_count, timeout_count * 0.015);
            }
            continue;
        }

        timeout_count = 0;

        if (FD_ISSET(m_fd, &read_fds) <= 0) {
            continue;
        }

        /* 读数据到环形缓冲 */
        int space = RX_BUF_SIZE - m_rx_head;
        if (space <= 0) {
            /* 缓冲满, 整体前移 */
            if (m_rx_tail > 0) {
                memmove(m_rx_buf, m_rx_buf + m_rx_tail, m_rx_head - m_rx_tail);
                m_rx_head -= m_rx_tail;
                m_rx_tail = 0;
            } else {
                /* tail==0 且 head 到头, 缓冲溢出, 清空 */
                ECO_WARN("[BatteryDispatcher] rx buffer overflow, flush");
                m_rx_head = 0;
                m_rx_tail = 0;
                continue;
            }
        }

        space = RX_BUF_SIZE - m_rx_head;
        int n = static_cast<int>(read(m_fd, m_rx_buf + m_rx_head, space));
        if (n <= 0) {
            ECO_WARN("[BatteryDispatcher] read returned %d, errno=%d", n, errno);
            continue;
        }
        m_rx_head += n;

        /* 循环解包 */
        uint16_t buf_len = m_rx_head - m_rx_tail;
        while (buf_len >= BATTERY_FRAME_MIN_LEN) {
            consumed = 0;
            has_frame = m_frame.Unpack(m_rx_buf + m_rx_tail, buf_len, pkg, consumed);

            if (has_frame) {
                /* 成功解出一帧 */
                m_rx_tail += consumed;
                buf_len    = m_rx_head - m_rx_tail;

                /* 尝试匹配待处理请求 */
                TryMatchResponse(pkg);

                /* 解析响应 (主动上报 或 轮询响应) */
                if (!pkg.is_request) {
                    ParseResponse(pkg);
                }
            } else if (consumed > 0) {
                /* 无效帧, 丢弃 consumed 字节 */
                m_rx_tail += consumed;
                buf_len    = m_rx_head - m_rx_tail;
            } else {
                /* 数据不完整, 等待更多数据 */
                break;
            }
        }

        /* 已消费全部数据, 指针归零 */
        if (m_rx_tail >= m_rx_head) {
            m_rx_tail = 0;
            m_rx_head = 0;
        }
    }

    ECO_INFO("[BatteryDispatcher] recv thread exit");
}

/* ================================================================
 * 发送线程
 * ================================================================ */

void
BatteryDispatcher::SendThreadFunc()
{
    prctl(PR_SET_NAME, "batt_send");
    ECO_INFO("[BatteryDispatcher] send thread started");

    while (m_running.load()) {
        std::vector<uint8_t> frame;

        {
            std::unique_lock<std::mutex> lk(m_send_mutex);
            m_send_cv.wait(lk, [this]() {
                return !m_running.load() || !m_send_queue.empty();
            });

            if (!m_running.load() && m_send_queue.empty()) {
                break;
            }

            if (!m_send_queue.empty()) {
                frame = std::move(m_send_queue.front());
                m_send_queue.pop_front();
            }
        }

        if (frame.empty()) {
            continue;
        }

        if (m_fd < 0 || !m_running.load()) {
            continue;
        }

        int written = static_cast<int>(write(m_fd, frame.data(), frame.size()));
        if (written < 0) {
            ECO_ERROR("[BatteryDispatcher] write failed: %s", strerror(errno));
        } else if (static_cast<size_t>(written) != frame.size()) {
            ECO_WARN("[BatteryDispatcher] partial write: %d/%zu",
                         written, frame.size());
        }

        /* 帧间延时: BMS 需 ~33ms 处理每帧, 但 UART 全双工,收发自独立.
         * 此延时防止 BMS 接收 FIFO 溢出, 15ms 足够覆盖 ~1ms 发送窗口 + 余量 */
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    ECO_INFO("[BatteryDispatcher] send thread exit");
}

/* ================================================================
 * 轮询线程
 * ================================================================ */

void
BatteryDispatcher::PollThreadFunc()
{
    prctl(PR_SET_NAME, "batt_poll");
    ECO_INFO("[BatteryDispatcher] poll thread started, core=%ums soc=%ums fault=%ums",
                 m_poll_cfg.period_ms, m_poll_cfg.soc_period_ms,
                 m_poll_cfg.fault_period_ms);

    /* 启动时查询一次基本信息 (帧间隔由 send 线程保证) */
    SendQueryRequest(BatteryFunc::BASIC_INFO, BatteryCmd::GET_VERSION);
    SendQueryRequest(BatteryFunc::BASIC_INFO, BatteryCmd::GET_ID);

    auto last_core_time  = std::chrono::steady_clock::now();
    auto last_soc_time   = last_core_time;
    auto last_fault_time = last_core_time;

    while (m_running.load()) {
        auto now = std::chrono::steady_clock::now();
        auto core_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_core_time).count();
        auto soc_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_soc_time).count();
        auto fault_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_fault_time).count();

        /* 核心数据: 电压 + 电流温度 (帧间隔由 send 线程保证) */
        if (core_elapsed >= static_cast<int64_t>(m_poll_cfg.period_ms)) {
            if (m_poll_cfg.enable_voltage) {
                SendQueryRequest(BatteryFunc::VOLTAGE_CTRL, BatteryCmd::CELL_VOLTAGE);
            }
            if (m_poll_cfg.enable_current_temp) {
                SendQueryRequest(BatteryFunc::COMPREHENSIVE, BatteryCmd::CURRENT_TEMP);
            }
            if (m_poll_cfg.enable_charge_current) {
                SendQueryRequest(BatteryFunc::VOLTAGE_CTRL, BatteryCmd::CHARGE_CURRENT);
            }

            last_core_time = now;
        }

        /* SOC/容量 (低频) */
        if (m_poll_cfg.enable_soc_capacity &&
            soc_elapsed >= static_cast<int64_t>(m_poll_cfg.soc_period_ms)) {
            SendQueryRequest(BatteryFunc::COMPREHENSIVE, BatteryCmd::SOC_CAPACITY);
            last_soc_time = now;
        }

        /* 故障信息 (最低频) */
        if (m_poll_cfg.enable_fault &&
            fault_elapsed >= static_cast<int64_t>(m_poll_cfg.fault_period_ms)) {
            SendQueryRequest(BatteryFunc::COMPREHENSIVE, BatteryCmd::FAULT_INFO);
            last_fault_time = now;
        }

        /* 休眠到下次最近的 poll 时间 */
        auto next = std::min({
            last_core_time + std::chrono::milliseconds(m_poll_cfg.period_ms),
            last_soc_time + std::chrono::milliseconds(m_poll_cfg.soc_period_ms),
            last_fault_time + std::chrono::milliseconds(m_poll_cfg.fault_period_ms)
        });
        auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            next - std::chrono::steady_clock::now()).count();
        if (wait_ms > 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        }
    }

    ECO_INFO("[BatteryDispatcher] poll thread exit");
}

/* ================================================================
 * 请求发送
 * ================================================================ */

void
BatteryDispatcher::SendQueryRequest(uint8_t func_code, uint8_t cmd_code)
{
    BatteryPkg pkg;
    pkg.is_request = true;
    pkg.src_addr   = BATTERY_ADDR_HOST;
    pkg.dst_addr   = BATTERY_ADDR_BMS;
    pkg.func_code  = func_code;
    pkg.cmd_code   = cmd_code;
    /* 查询指令数据域为空 */

    auto frame = m_frame.Build(pkg);

    {
        std::lock_guard<std::mutex> lk(m_send_mutex);
        m_send_queue.push_back(std::move(frame));
    }
    m_send_cv.notify_one();
}

/* ================================================================
 * 响应匹配
 * ================================================================ */

void
BatteryDispatcher::TryMatchResponse(const BatteryPkg& pkg)
{
    if (pkg.is_request) {
        return;  /* 不匹配请求帧 */
    }

    std::lock_guard<std::mutex> lk(m_req_mutex);

    if (!m_pending_req.active) {
        return;
    }

    if (pkg.func_code == m_pending_req.func_code &&
        pkg.cmd_code  == m_pending_req.cmd_code) {
        m_pending_req.rsp_data = pkg.data;
        m_pending_req.done     = true;
        m_req_cv.notify_all();
    }
}

/* ================================================================
 * 响应解析
 * ================================================================ */

/* 格式化 BMS 返回字符串: 全 ASCII → 原样保留; 含非法字节 → hex dump (防 JSON 崩溃) */
static void FormatBmsString(std::string& s)
{
    bool has_binary = false;
    for (unsigned char c : s) {
        if (c < 0x20 || c > 0x7E) { has_binary = true; break; }
    }
    if (!has_binary) return;  /* 纯 ASCII, 无需转换 */

    /* 含二进制字节: 转 hex dump, 如 "20 FF FF 30 30..." */
    std::string hex;
    hex.reserve(s.size() * 3);
    char buf[4];
    for (size_t i = 0; i < s.size(); ++i) {
        snprintf(buf, sizeof(buf), "%02X", static_cast<unsigned char>(s[i]));
        if (i > 0) hex += ' ';
        hex += buf;
    }
    s = std::move(hex);
}

void
BatteryDispatcher::ParseResponse(const BatteryPkg& pkg)
{
    uint16_t combined = (static_cast<uint16_t>(pkg.func_code) << 8) | pkg.cmd_code;

    switch (combined) {
    case 0x2004: ParseCellVoltage(pkg);    break;
    case 0x5002: ParseCurrentTemp(pkg);    break;
    case 0x5004: ParseSocCapacity(pkg);    break;
    case 0x503B: ParseFaultInfo(pkg);      break;
    case 0x1003: ParseVersion(pkg);        break;
    case 0x1005: ParseId(pkg);             break;
    case 0x2005: ParseChargeCurrent(pkg);  break;
    case 0x9001: ParsePowerCtrlAck(pkg);   break;
    case 0x9003: ParseChargeSwitchAck(pkg);break;
    case 0x1007: ParseQrCode(pkg);         break;
    default:
        break;
    }
}

void
BatteryDispatcher::ParseCellVoltage(const BatteryPkg& pkg)
{
    const auto& d = pkg.data;
    if (d.size() < 16) {
        ECO_WARN("[BatteryDispatcher] 0x2004 data too short: %zu bytes", d.size());
        return;
    }

    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    /* 总电压: 4 字节大端, mV */
    m_status.total_voltage_mv = (static_cast<uint32_t>(d[0]) << 24) |
                                (static_cast<uint32_t>(d[1]) << 16) |
                                (static_cast<uint32_t>(d[2]) << 8)  |
                                 static_cast<uint32_t>(d[3]);

    /* 各节电芯电压: 每 2 字节大端, mV */
    for (int i = 0; i < 6 && (4 + i * 2 + 1) < d.size(); i++) {
        uint16_t offset = 4 + i * 2;
        m_status.cell_voltage_mv[i] = (static_cast<uint16_t>(d[offset]) << 8) |
                                       static_cast<uint16_t>(d[offset + 1]);
    }

    ECO_INFO("[BatteryDispatcher] 0x2004 voltage: %u mV, cells: %d/%d/%d/%d/%d/%d mV",
             m_status.total_voltage_mv,
             m_status.cell_voltage_mv[0], m_status.cell_voltage_mv[1],
             m_status.cell_voltage_mv[2], m_status.cell_voltage_mv[3],
             m_status.cell_voltage_mv[4], m_status.cell_voltage_mv[5]);

    m_status.has_voltage = true;
    m_status.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NotifyStatusCallbacks();
}

void
BatteryDispatcher::ParseCurrentTemp(const BatteryPkg& pkg)
{
    const auto& d = pkg.data;
    if (d.size() < 8) {
        ECO_WARN("[BatteryDispatcher] 0x5002 data too short: %zu bytes (expect 8)", d.size());
        return;
    }

    ECO_INFO("[BatteryDispatcher] 0x5002 raw: %02X %02X %02X %02X %02X %02X %02X %02X",
             d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    /* 充放电电流: 4 字节大端有符号, mA, 充电为正 */
    m_status.current_ma = static_cast<int32_t>(
        (static_cast<uint32_t>(d[0]) << 24) |
        (static_cast<uint32_t>(d[1]) << 16) |
        (static_cast<uint32_t>(d[2]) << 8)  |
         static_cast<uint32_t>(d[3]));

    /* A08: 温度1: 2 字节大端, 0.1K; 温度2: 2 字节大端, 0.1K */
    uint16_t temp1_raw = (static_cast<uint16_t>(d[4]) << 8) | d[5];
    uint16_t temp2_raw = (static_cast<uint16_t>(d[6]) << 8) | d[7];
    m_status.temperature_k_raw = temp1_raw;
    m_status.temperature_c = static_cast<double>(temp1_raw) / 10.0 - 273.15;

    ECO_INFO("[BatteryDispatcher] 0x5002 current=%dmA temp1=%.1fC temp2=%.1fC",
             m_status.current_ma, m_status.temperature_c,
             static_cast<double>(temp2_raw) / 10.0 - 273.15);

    m_status.has_current_temp = true;
    m_status.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NotifyStatusCallbacks();
}

void
BatteryDispatcher::ParseSocCapacity(const BatteryPkg& pkg)
{
    const auto& d = pkg.data;
    if (d.size() < 8) {
        ECO_WARN("[BatteryDispatcher] 0x5004 data too short: %zu bytes", d.size());
        return;
    }

    ECO_INFO("[BatteryDispatcher] 0x5004 raw: %02X %02X %02X %02X %02X %02X %02X %02X",
             d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    m_status.soc_percent        = d[0];
    m_status.remain_capacity_mah = (static_cast<uint16_t>(d[1]) << 8) |
                                    static_cast<uint16_t>(d[2]);
    m_status.total_capacity_mah = (static_cast<uint16_t>(d[3]) << 8) |
                                   static_cast<uint16_t>(d[4]);
    m_status.cycle_count        = (static_cast<uint16_t>(d[5]) << 8) |
                                   static_cast<uint16_t>(d[6]);
    m_status.bms_status         = d[7];

    ECO_INFO("[BatteryDispatcher] 0x5004 soc=%d%% remain=%dmAh total=%dmAh cycles=%d status=0x%02X",
             m_status.soc_percent, m_status.remain_capacity_mah,
             m_status.total_capacity_mah, m_status.cycle_count, m_status.bms_status);

    m_status.has_soc_capacity = true;
    m_status.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NotifyStatusCallbacks();
}

void
BatteryDispatcher::ParseFaultInfo(const BatteryPkg& pkg)
{
    const auto& d = pkg.data;
    if (d.size() < 20) {
        ECO_WARN("[BatteryDispatcher] 0x503B data too short: %zu bytes", d.size());
        return;
    }

    ECO_INFO("[BatteryDispatcher] 0x503B fault raw: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
             d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15],
             d[16], d[17], d[18], d[19]);

    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    /* 最近 10 次故障, 每 2 字节小端 */
    for (int i = 0; i < 10; i++) {
        uint16_t offset = i * 2;
        m_fault.records[i] = static_cast<uint16_t>(d[offset]) |
                            (static_cast<uint16_t>(d[offset + 1]) << 8);
    }

    /* 解析最近一次故障 */
    DecodeFaultBits(m_fault.records[0], m_fault);

    ECO_INFO("[BatteryDispatcher] 0x503B latest=0x%04X overvol=%d overcur=%d ntcShort=%d",
             m_fault.records[0],
             m_fault.charger_overvoltage, m_fault.charge_overcurrent,
             m_fault.ntc_short);

    m_fault.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NotifyFaultCallbacks();
}

void
BatteryDispatcher::ParseVersion(const BatteryPkg& pkg)
{
    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    m_info.version.assign(
        reinterpret_cast<const char*>(pkg.data.data()), pkg.data.size());
    FormatBmsString(m_info.version);
    m_info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    ECO_INFO("[BatteryDispatcher] BMS version: %s", m_info.version.c_str());
    }

    NotifyInfoCallbacks();
}

void
BatteryDispatcher::ParseId(const BatteryPkg& pkg)
{
    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    m_info.id.assign(
        reinterpret_cast<const char*>(pkg.data.data()), pkg.data.size());
    FormatBmsString(m_info.id);
    m_info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    ECO_INFO("[BatteryDispatcher] BMS ID: %s", m_info.id.c_str());
    }

    NotifyInfoCallbacks();
}

void
BatteryDispatcher::ParseChargeCurrent(const BatteryPkg& pkg)
{
    const auto& d = pkg.data;
    if (d.size() < 2) {
        return;
    }

    {
    std::lock_guard<std::mutex> lk(m_data_mutex);

    m_status.charge_current_ma = (static_cast<uint16_t>(d[0]) << 8) |
                                  static_cast<uint16_t>(d[1]);

    ECO_INFO("[BatteryDispatcher] 0x2005 charge_current=%d mA",
             m_status.charge_current_ma);

    m_status.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    NotifyStatusCallbacks();
}

void
BatteryDispatcher::ParsePowerCtrlAck(const BatteryPkg& pkg)
{
    if (!pkg.data.empty()) {
        ECO_INFO("[BatteryDispatcher] power ctrl ack: 0x%02X", pkg.data[0]);
    }
}

void
BatteryDispatcher::ParseChargeSwitchAck(const BatteryPkg& pkg)
{
    if (!pkg.data.empty()) {
        ECO_INFO("[BatteryDispatcher] charge switch ack: 0x%02X", pkg.data[0]);
    }
}


void
BatteryDispatcher::ParseQrCode(const BatteryPkg& pkg)
{
    std::string qr(reinterpret_cast<const char*>(pkg.data.data()),
                   pkg.data.size());
    ECO_INFO("[BatteryDispatcher] 0x1007 QR: %s", qr.c_str());
}

/* ================================================================
 * 故障位解析
 * ================================================================ */

void
BatteryDispatcher::DecodeFaultBits(uint16_t fault_code, BatteryFault& fault)
{
    fault.charger_overvoltage   = (fault_code & BatteryFaultBit::CHARGER_OVERVOLTAGE)   == 0;
    fault.charge_overcurrent    = (fault_code & BatteryFaultBit::CHARGE_OVERCURRENT)    == 0;
    fault.ntc_short             = (fault_code & BatteryFaultBit::NTC_SHORT)             == 0;
    fault.ntc_open              = (fault_code & BatteryFaultBit::NTC_OPEN)              == 0;
    fault.cell_voltage_diff     = (fault_code & BatteryFaultBit::CELL_VOLTAGE_DIFF)     == 0;
    fault.charge_timeout        = (fault_code & BatteryFaultBit::CHARGE_TIMEOUT)        == 0;
    fault.discharge_overcurrent = (fault_code & BatteryFaultBit::DISCHARGE_OVERCURRENT) == 0;
    fault.discharge_short       = (fault_code & BatteryFaultBit::DISCHARGE_SHORT)       == 0;
    fault.secondary_overcharge  = (fault_code & BatteryFaultBit::SECONDARY_OVERCHARGE)  == 0;
}

/* ================================================================
 * Observer 通知
 * ================================================================ */

void
BatteryDispatcher::NotifyStatusCallbacks()
{
    BatteryStatus status_copy;
    {
        std::lock_guard<std::mutex> lk(m_data_mutex);
        status_copy = m_status;
    }

    std::lock_guard<std::mutex> lk(m_cb_mutex);
    for (auto& cb : m_status_cbs) {
        if (cb) {
            cb(status_copy);
        }
    }
}

void
BatteryDispatcher::NotifyFaultCallbacks()
{
    BatteryFault fault_copy;
    {
        std::lock_guard<std::mutex> lk(m_data_mutex);
        fault_copy = m_fault;
    }

    std::lock_guard<std::mutex> lk(m_cb_mutex);
    for (auto& cb : m_fault_cbs) {
        if (cb) {
            cb(fault_copy);
        }
    }
}

void
BatteryDispatcher::NotifyInfoCallbacks()
{
    BatteryInfo info_copy;
    {
        std::lock_guard<std::mutex> lk(m_data_mutex);
        info_copy = m_info;
    }

    std::lock_guard<std::mutex> lk(m_cb_mutex);
    for (auto& cb : m_info_cbs) {
        if (cb) {
            cb(info_copy);
        }
    }
}

}  /* namespace stark_power_manager */
