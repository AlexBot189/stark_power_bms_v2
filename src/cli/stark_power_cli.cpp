/*
 * stark_power_cli.cpp — 电池 + 电源 ROS 客户端工具
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 用法:
 *   stark_power_cli                    订阅并打印电池/充电信息
 *   stark_power_cli --charge on|off    充电使能/禁止
 *   stark_power_cli --discharge on|off 放电使能/禁止
 *   stark_power_cli --current <ma>     限制充电电流
 *   stark_power_cli --shutdown         关机(断开电池输出)
 *   stark_power_cli --standby          强制待机
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ros/ros.h>
#include <stark_msgs/BatteryStatus.h>
#include <stark_msgs/BatteryFault.h>
#include <stark_msgs/ChargeState.h>
#include <stark_msgs/PowerState.h>
#include <stark_msgs/PowerCtrl.h>

static const char* kChargeStateName[] = {
    "IDLE", "DETECT", "CHARGE", "FULL", "FAULT"
};

static const char*
ChargeStateStr(uint8_t s)
{
    if (s >= sizeof(kChargeStateName) / sizeof(kChargeStateName[0])) {
        return "UNKNOWN";
    }
    return kChargeStateName[s];
}

static void
OnStatus(const stark_msgs::BatteryStatus::ConstPtr& msg)
{
    printf("[battery] %umV %dmA %.1fC SOC=%u%% bms=0x%02X\n",
           msg->total_voltage_mv, msg->current_ma, msg->temperature_c,
           msg->soc_percent, msg->bms_status);
}

static void
OnFault(const stark_msgs::BatteryFault::ConstPtr& msg)
{
    printf("[fault] sys1=0x%02X sys2=0x%02X dis1=0x%02X dis2=0x%02X "
           "chg1=0x%02X chg2=0x%02X pack=0x%02X\n",
           msg->sys_fault1, msg->sys_fault2,
           msg->dischg_fault1, msg->dischg_fault2,
           msg->chg_fault1, msg->chg_fault2, msg->pack_status);
}

static void
OnChargeState(const stark_msgs::ChargeState::ConstPtr& msg)
{
    printf("[charge] %s adapter=%d %umV %dmA %dC SOC=%u%%\n",
           ChargeStateStr(msg->state), msg->adapter_online ? 1 : 0,
           msg->batt_voltage_mv, msg->batt_current_ma,
           msg->batt_temp_c, msg->batt_soc);
}

static void
OnPowerState(const stark_msgs::PowerState::ConstPtr& msg)
{
    printf("[power] %s adapter=%d batt=%umV/%dmA/%dC/SOC%u%% "
           "chg_type=%u chg_cur=%umA phase=%u fault=%d\n",
           ChargeStateStr(msg->charge_state), msg->adapter_online ? 1 : 0,
           msg->batt_voltage_mv, msg->batt_current_ma,
           msg->batt_temp_c, msg->batt_soc,
           msg->charge_type, msg->charge_current_ma,
           msg->charger_phase, msg->fault ? 1 : 0);
}

static int
SendControl(const char* op, uint8_t cmd, bool enable, uint16_t ma)
{
    ros::NodeHandle nh;
    ros::Publisher pub = nh.advertise<stark_msgs::PowerCtrl>("/stark/power_ctrl", 1);

    ros::Time start = ros::Time::now();
    while (pub.getNumSubscribers() == 0 && ros::Time::now() - start < ros::Duration(3.0)) {
        ros::Duration(0.1).sleep();
    }
    if (pub.getNumSubscribers() == 0) {
        printf("no subscriber on /stark/power_ctrl\n");
        return -1;
    }

    stark_msgs::PowerCtrl ctrl;
    ctrl.cmd = cmd;
    ctrl.enable = enable;
    ctrl.current_limit_ma = ma;
    pub.publish(ctrl);

    ros::Duration(0.2).sleep();
    printf("sent %s\n", op);
    return 0;
}

int
main(int argc, char** argv)
{
    ros::init(argc, argv, "stark_power_cli");

    if (argc > 1) {
        if (strcmp(argv[1], "--charge") == 0 && argc > 2) {
            bool en = (strcmp(argv[2], "on") == 0);
            return SendControl("charge", stark_msgs::PowerCtrl::CTRL_CHARGE, en, 0);
        }
        if (strcmp(argv[1], "--discharge") == 0 && argc > 2) {
            bool en = (strcmp(argv[2], "on") == 0);
            return SendControl("discharge", stark_msgs::PowerCtrl::CTRL_DISCHARGE, en, 0);
        }
        if (strcmp(argv[1], "--current") == 0 && argc > 2) {
            return SendControl("current", stark_msgs::PowerCtrl::CTRL_CURRENT, false,
                               static_cast<uint16_t>(atoi(argv[2])));
        }
        if (strcmp(argv[1], "--shutdown") == 0) {
            return SendControl("shutdown", stark_msgs::PowerCtrl::CTRL_SHUTDOWN, false, 0);
        }
        if (strcmp(argv[1], "--standby") == 0) {
            return SendControl("standby", stark_msgs::PowerCtrl::CTRL_STANDBY, false, 0);
        }
        printf("usage: %s [--charge on|off] [--discharge on|off] "
               "[--current ma] [--shutdown] [--standby]\n", argv[0]);
        return -1;
    }

    ros::NodeHandle nh;
    ros::Subscriber sub1 = nh.subscribe<stark_msgs::BatteryStatus>(
        "/stark/battery_status", 10, OnStatus);
    ros::Subscriber sub2 = nh.subscribe<stark_msgs::BatteryFault>(
        "/stark/battery_fault", 10, OnFault);
    ros::Subscriber sub3 = nh.subscribe<stark_msgs::ChargeState>(
        "/stark/charge_state", 10, OnChargeState);
    ros::Subscriber sub4 = nh.subscribe<stark_msgs::PowerState>(
        "/stark/power_state", 10, OnPowerState);

    printf("monitoring battery/charge state...\n");
    ros::spin();
    return 0;
}
