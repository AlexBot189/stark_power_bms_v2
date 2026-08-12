/*
 * IPowerSource.h — 电源设备统一抽象接口
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 所有电源驱动 (充电IC/BMS/适配器检测) 实现此接口。
 * 上层通过 PowerRegistry 查询属性, 不直接依赖具体驱动类型。
 *
 * 线程安全:
 *   getProp() 可被多线程并发调用, 实现层自行保护
 *   setProp() 仅由 PowerManager 调用, 单线程
 *   subscribe() 在初始化阶段调用, 运行时不修改
 */
#pragma once

#include <functional>
#include <string>
#include <vector>
#include "PowerProp.h"

namespace stark_power_manager {

class IPowerSource {
public:
    using ChangeCallback = std::function<void(PowerProp prop, const PowerValue& val)>;

    virtual ~IPowerSource() = default;

    /* 设备标识 */
    virtual const char* name() const = 0;
    virtual const char* type() const = 0;
    /* type() 取值: "charger" | "battery" | "ac_adapter" */

    /* 属性查询 */
    virtual std::vector<PowerProp> supportedProps() const = 0;
    virtual bool getProp(PowerProp prop, PowerValue& out) = 0;
    /* 返回 false = 不支持或读取失败 */

    /* 属性设置 */
    virtual bool setProp(PowerProp prop, const PowerValue& val) = 0;
    /* 返回 false = 不支持或写入失败 */

    /* 状态变化通知 */
    virtual void subscribe(ChangeCallback cb) = 0;
};

} /* namespace stark_power_manager */
