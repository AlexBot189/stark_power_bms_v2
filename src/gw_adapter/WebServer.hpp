#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <atomic>

#include "Defines.hpp"
#include "3rd_party/crow/crow_all.h"

namespace stark_power_manager
{

class WebServer
{
public:
    explicit WebServer(uint16_t port);

    ~WebServer()
    {
        Stop();
    }

    void
    BroadcastMessageToWebApp(const std::string& message);

    void
    SetBatteryCtrlHandler(std::function<void(const std::string&)> handler);

    void
    SetBatteryInfoHandler(std::function<void()> handler);

    void
    Stop();

    bool
    IsRunning() const;

private:
    void
    RunServer();

    void
    HandleWebAppMsg(const std::string& msg);

    void
    ResponseStaticResource(const crow::request& req, crow::response& resp, const std::string& path);

    void
    BuildAndSendResponse(crow::response& resp, int code, const std::string& message);

private:
    uint16_t m_port;
    std::atomic<bool> m_running{ false };
    std::thread m_serverThread;

    crow::SimpleApp m_app;
    std::mutex m_mtx;
    std::unordered_set<crow::websocket::connection*> m_users;

    std::function<void(const std::string&)> m_batteryCtrlHandler;
    std::function<void()> m_batteryInfoHandler;
};

}  // namespace stark_power_manager
