#pragma once

#include <mutex>
#include <memory>
#include <string>
#include <boost/any.hpp>
#include "config/Defines.hpp"

using TypeId = boost::typeindex::stl_type_index;

namespace stark_power_manager
{
class ConfigStorage
{
public:
    static std::shared_ptr<ConfigStorage>
    GetInstance();

    void Init(const std::string& configPath);

    ~ConfigStorage() = default;

    template<typename T>
    T
    Get()
    {
        auto any = GetAny(typeid(T));
        return boost::any_cast<T>(any);
    }

private:
    ConfigStorage();
    ConfigStorage(const ConfigStorage&) = delete;
    ConfigStorage(const ConfigStorage&&) = delete;
    ConfigStorage&
    operator=(const ConfigStorage&) = delete;

    void
    LoadFromJson(const std::string& configPath);

    boost::any
    GetAny(const TypeId& type);

private:
    std::string m_strConfigFile;
    BatteryOption m_batteryOption;
    WebOption m_webOption;
    Ip2366Option m_ip2366Option;
    PowerManagerOption m_powerOption;

    static std::mutex m_singleMutex;
    static std::shared_ptr<ConfigStorage> m_instance;
};
}  // namespace stark_power_manager
