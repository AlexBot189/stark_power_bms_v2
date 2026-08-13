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
#include "ros/StarkRosAdapter.h"
#include "power/PowerRegistry.h"
#include "power/PowerManager.h"
#include "power/ChargeStateMachine.h"
#include "power/plugins/ip2366/IP2366Source.h"
#include "power/plugins/bms_uart/BmsUartSource.h"

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

/* 充电状态名 (日志用) */
static const char*
ChargeStateName(ChargeState s)
{
    switch (s) {
    case ChargeState::IDLE:   return "IDLE";
    case ChargeState::DETECT: return "DETECT";
    case ChargeState::CHARGE: return "CHARGE";
    case ChargeState::FULL:   return "FULL";
    case ChargeState::FAULT:  return "FAULT";
    default:                  return "UNKNOWN";
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

    /* ================= 电源管理框架 (静态注册) ================= */
    /* 从 JSON 加载充电配置 (ip2366 + powerManager 两个 section) */
    auto ip2366Opt = ConfigStorage::GetInstance()->Get<Ip2366Option>();
    auto powerOpt  = ConfigStorage::GetInstance()->Get<PowerManagerOption>();

    IP2366Source::Config ip2366Cfg;
    ip2366Cfg.i2c_dev             = ip2366Opt.i2c_dev;
    ip2366Cfg.addr                = static_cast<uint8_t>(ip2366Opt.i2c_addr);
    ip2366Cfg.int_gpio_chip       = ip2366Opt.int_gpio_chip;
    ip2366Cfg.int_gpio_line       = ip2366Opt.int_gpio_line;
    ip2366Cfg.charge_en_gpio_chip = ip2366Opt.charge_en_gpio_chip;
    ip2366Cfg.charge_en_gpio_line = ip2366Opt.charge_en_gpio_line;
    ip2366Cfg.pdo_select          = static_cast<uint8_t>(ip2366Opt.pdo_select);
    ip2366Cfg.charge_voltage_mv   = static_cast<uint16_t>(ip2366Opt.charge_voltage_mv);
    ip2366Cfg.charge_current_ma   = static_cast<uint16_t>(ip2366Opt.charge_current_ma);
    ip2366Cfg.trickle_current_ma  = static_cast<uint16_t>(ip2366Opt.trickle_current_ma);
    ip2366Cfg.stop_current_ma     = static_cast<uint16_t>(ip2366Opt.stop_current_ma);
    ip2366Cfg.int_poll_interval_ms = static_cast<uint32_t>(ip2366Opt.int_poll_interval_ms);

    PowerManagerConfig powerCfg;
    powerCfg.adapter_debounce_ms  = powerOpt.adapter_debounce_ms;
    powerCfg.fault_debounce_ms    = powerOpt.fault_debounce_ms;
    powerCfg.recovery_debounce_ms = powerOpt.recovery_debounce_ms;
    powerCfg.full_soc_percent     = powerOpt.full_soc_percent;
    powerCfg.full_current_ma      = powerOpt.full_current_ma;
    powerCfg.recharge_hyst_mv     = powerOpt.recharge_hyst_mv;
    powerCfg.cc_timeout_min       = powerOpt.cc_timeout_min;
    powerCfg.total_timeout_min    = powerOpt.total_timeout_min;
    powerCfg.max_temp_c           = powerOpt.max_temp_c;
    powerCfg.resume_temp_c        = powerOpt.resume_temp_c;
    powerCfg.pd_timeout_ms        = powerOpt.pd_timeout_ms;
    powerCfg.cross_verify_charge  = powerOpt.cross_verify_charge;
    powerCfg.vbus_present_mv      = powerOpt.vbus_present_mv;

    /* 充电 IC 数据源 (I2C + INT GPIO), 初始化失败则降级为仅 BMS */
    std::unique_ptr<IP2366Source> ip2366(new IP2366Source(ip2366Cfg));
    if (ip2366->initialize()) {
        PowerRegistry::instance().registerSource(std::move(ip2366));
        ECO_INFO("[main] IP2366 charger source registered");
    } else {
        ECO_WARN("[main] IP2366 init failed, charger source unavailable (degraded)");
    }

    /* 电池 BMS 数据源 (包装 BatteryDispatcher) */
    std::unique_ptr<BmsUartSource> bmsSrc(new BmsUartSource(pBattery));
    PowerRegistry::instance().registerSource(std::move(bmsSrc));
    ECO_INFO("[main] battery_bms source registered");

    /* 充电管理器 (1Hz tick 驱动状态机) */
    auto powerMgr = std::make_shared<PowerManager>(powerCfg);
    powerMgr->initialize();
    powerMgr->setStateChangeCb([](ChargeState from, ChargeState to) {
        ECO_INFO("[PowerManager] state: %s -> %s",
                 ChargeStateName(from), ChargeStateName(to));
    });
    powerMgr->setFaultCb([](const char* reason) {
        ECO_ERROR("[PowerManager] fault: %s", reason);
    });
    ros::Timer powerTimer = nh->createTimer(ros::Duration(1.0),
        [powerMgr](const ros::TimerEvent&) { powerMgr->tick(); });
    ECO_INFO("[main] power manager started (1Hz tick)");

    /* 电池 + 电源 ROS 接口统一适配器 */
    auto rosAdapter = std::make_shared<StarkRosAdapter>(
        nh, pBattery, powerMgr,
        [pWebServer](const std::string& msg) {
            if (pWebServer) pWebServer->BroadcastMessageToWebApp(msg);
        });
    rosAdapter->Init();
    ECO_INFO("[main] ROS adapter started");

    if (pWebServer)
    {
        pWebServer->SetBatteryCtrlHandler(
            [rosAdapter](const std::string& msg) { rosAdapter->HandleBatteryCtrl(msg); });
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
