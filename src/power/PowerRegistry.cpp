/*
 * PowerRegistry.cpp — 全局电源设备注册表实现
 * Copyright (c) 2026 zhiqiang.yang
 */
#include "PowerRegistry.h"
#include <algorithm>

namespace stark_power_manager {

PowerRegistry& PowerRegistry::instance()
{
    static PowerRegistry s_instance;
    return s_instance;
}

void PowerRegistry::registerSource(std::unique_ptr<IPowerSource> src)
{
    if (!src) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    /* 检查同名冲突 */
    auto it = std::find_if(m_sources.begin(), m_sources.end(),
        [&src](const std::unique_ptr<IPowerSource>& existing) {
            return existing->name() == std::string(src->name());
        });

    if (it != m_sources.end()) {
        /* 同名设备已存在, 覆盖注册 */
        *it = std::move(src);
    } else {
        m_sources.push_back(std::move(src));
    }
}

void PowerRegistry::unregisterSource(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sources.erase(
        std::remove_if(m_sources.begin(), m_sources.end(),
            [&name](const std::unique_ptr<IPowerSource>& src) {
                return src->name() == name;
            }),
        m_sources.end());
}

IPowerSource* PowerRegistry::getSource(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& src : m_sources) {
        if (src->name() == name) {
            return src.get();
        }
    }
    return nullptr;
}

std::vector<IPowerSource*> PowerRegistry::findSources(const std::string& type)
{
    std::vector<IPowerSource*> result;
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& src : m_sources) {
        if (src->type() == type) {
            result.push_back(src.get());
        }
    }
    return result;
}

bool PowerRegistry::getProp(const std::string& srcName, PowerProp prop, PowerValue& out)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    for (auto& src : m_sources) {
        if (src->name() == srcName) {
            IPowerSource* target = src.get();
            lock.unlock();
            return target->getProp(prop, out);
        }
    }
    return false;
}

bool PowerRegistry::setProp(const std::string& srcName, PowerProp prop, const PowerValue& val)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    for (auto& src : m_sources) {
        if (src->name() == srcName) {
            IPowerSource* target = src.get();
            lock.unlock();
            return target->setProp(prop, val);
        }
    }
    return false;
}

std::vector<std::string> PowerRegistry::listSources() const
{
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& src : m_sources) {
        names.push_back(src->name());
    }
    return names;
}

} /* namespace stark_power_manager */
