/*
 * main.cpp — stark_power_manager 电池监控服务入口
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 用法:
 *   stark_power_manager_node                   使用默认配置
 *   stark_power_manager_node -c /path/to/config.json  指定配置文件
 *
 * 配置文件 (JSON):
 *   {
 *     "batteryOption": { "tty": "/dev/ttyS2", "baudRate": 115200, ... },
 *     "web": { "enabled": true, "port": 50080 }
 *   }
 */
#include <signal.h>
#include <execinfo.h>
#include <cstring>
#include <ros/ros.h>
#include <log_helper/LogHelper.h>

#include "utility/Util.hpp"
#include "config/ConfigStorage.hpp"
#include "gw_adapter/WebServer.hpp"
#include "battery/BatteryDispatcher.h"
#include "battery/BatteryRosAdapter.hpp"

using namespace stark_power_manager;

inline void
PrintBacktrace(int signo)
{
#ifdef MODULE_NAME
#define NAME MODULE_NAME
#else
#define NAME "Default Backtrace Name"
#endif

    int j, nptrs;
    void* buffer[10];
    char** strings;
    ECO_INFO("[%s][%s][%d]: signo: %d.", NAME, __func__, __LINE__, signo);

    nptrs = backtrace(buffer, 10);
    ECO_INFO("[%s][%s][%d]: backtrace() returned %d addresses. \n", NAME, __func__, __LINE__, nptrs);

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL)
    {
        ECO_INFO("[%s][%s][%d]: backtrace_symbols.", NAME, __func__, __LINE__);
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
    {
        ECO_INFO("[%s][%s][%d]: %s.", NAME, __func__, __LINE__, strings[j]);
    }

    free(strings);
    if (SIGSEGV == signo || SIGQUIT == signo)
    {
        exit(0);
    }
}

int
main(int argc, char** argv)
{
    signal(SIGSEGV, PrintBacktrace);
    signal(SIGQUIT, PrintBacktrace);

    ECO_WARN("Compile time:%s %s", __DATE__, __TIME__);
    ECO_WARN("ThreadID of stark_power_manager: %d\n", GetPid());

    /* 解析 -c 参数 */
    std::string configPath = "/userdata/stark/stark_PowerConfig.json";
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            configPath = argv[++i];
        }
    }
    ECO_WARN("Config file: %s", configPath.c_str());

    ros::init(argc, argv, "stark_power_manager");
    std::shared_ptr<ros::NodeHandle> nh = std::make_shared<ros::NodeHandle>();

    /* 加载配置 (读失败用默认值保底) */
    ConfigStorage::GetInstance()->Init(configPath);
    auto batteryOption = ConfigStorage::GetInstance()->Get<BatteryOption>();
    auto webOption      = ConfigStorage::GetInstance()->Get<WebOption>();

    ECO_WARN("Battery: tty=%s baud=%d", batteryOption.strTty.c_str(), batteryOption.iBaudRate);
    ECO_WARN("Web: enabled=%s port=%u",
             webOption.enabled ? "YES" : "NO", webOption.port);

    /* 初始化电池串口 */
    auto pBattery = BatteryDispatcher::Create(batteryOption);
    if (!pBattery)
    {
        ECO_ERROR("Failed to init battery dispatcher, exit");
        return -1;
    }

    /* 创建 WebServer (仅 enabled=true 时) */
    std::shared_ptr<WebServer> pWebServer;
    if (webOption.enabled)
    {
        pWebServer = std::make_shared<WebServer>(webOption.port);
        ECO_INFO("WebServer started on port %u", webOption.port);
    }
    else
    {
        ECO_WARN("WebServer disabled by config");
    }

    /* 电池 ROS/WebSocket 适配器 */
    auto pBatteryRos = std::make_shared<BatteryRosAdapter>(
        nh, pBattery,
        [pWebServer](const std::string& msg) {
            if (pWebServer) pWebServer->BroadcastMessageToWebApp(msg);
        });

    if (pWebServer)
    {
        pWebServer->SetBatteryCtrlHandler(
            [pBatteryRos](const std::string& msg) { pBatteryRos->HandleBatteryCtrl(msg); });
        pWebServer->SetBatteryInfoHandler(
            [pBattery]() { pBattery->QueryInfo(); });
    }

    /* 主循环 */
    ros::Rate rate(150);
    while (ros::ok())
    {
        ros::spinOnce();
        rate.sleep();
    }

    ECO_WARN("stark_power_manager exiting");
    ros::shutdown();
    return 0;
}
