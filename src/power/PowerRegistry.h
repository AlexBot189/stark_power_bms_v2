/*
 * PowerRegistry — 全局电源设备注册表
 * Copyright (c) 2026 zhiqiang.yang
 *
 * 单例, 管理所有 IPowerSource 实例的生命周期和属性访问。
 * 参考 Linux power_supply_class 的 psy 链表模型。
 *
 * 使用:
 *   auto& reg = PowerRegistry::instance();
 *   reg.registerSource(make_unique<IP2366Source>(cfg));
 *   PowerValue v;
 *   reg.getProp("charger", PowerProp::ONLINE, v);
 */
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "IPowerSource.h"

namespace stark_power_manager {

class PowerRegistry {
public:
    static PowerRegistry& instance();

    /* 注册/注销 */
    void registerSource(std::unique_ptr<IPowerSource> src);
    void unregisterSource(const std::string& name);

    /* 按名查找 */
    IPowerSource* getSource(const std::string& name);

    /* 按类型查找 */
    std::vector<IPowerSource*> findSources(const std::string& type);

    /* 便捷方法: 读属性 */
    bool getProp(const std::string& srcName, PowerProp prop, PowerValue& out);

    /* 便捷方法: 写属性 */
    bool setProp(const std::string& srcName, PowerProp prop, const PowerValue& val);

    /* 获取所有已注册设备名 */
    std::vector<std::string> listSources() const;

private:
    PowerRegistry() = default;
    PowerRegistry(const PowerRegistry&) = delete;
    PowerRegistry& operator=(const PowerRegistry&) = delete;

    mutable std::mutex m_mutex;
    std::vector<std::unique_ptr<IPowerSource>> m_sources;
};

} /* namespace stark_power_manager */
