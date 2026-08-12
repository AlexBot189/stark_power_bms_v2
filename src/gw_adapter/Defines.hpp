#pragma once

#include <string>

namespace stark_power_manager
{
constexpr const char* WORK_DIR = "/data/robot/";

struct WsOption
{
    int iPort{ 40088 };
    std::string strIp{ "127.0.0.1" };
};

struct GwOption
{
    std::string strType{ "websocket" };
    WsOption wsOption;
};
}  // namespace stark_power_manager