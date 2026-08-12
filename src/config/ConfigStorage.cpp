#include <fstream>
#include <log_helper/LogHelper.h>

#include "ConfigStorage.hpp"
#include "utility/FileUnit.hpp"
#include "3rd_party/nlohmann/json.hpp"

using nJson = nlohmann::json;
using namespace stark_power_manager;

/**< 加载JSON配置文件, 失败返回空 JSON (使用默认值) */
inline nJson
LoadConfigFile2Json(const std::string& strConfigFile, std::string& strFullPath)
{
    strFullPath = strConfigFile;

    if (!FileUnit::Instance().IsFileExist(strFullPath))
    {
        ECO_WARN("Config file not found: %s, using defaults", strFullPath.c_str());
        return nJson{};
    }

    std::ifstream file(strFullPath);
    if (!file.is_open()) {
        ECO_WARN("Cannot open config: %s, using defaults", strFullPath.c_str());
        return nJson{};
    }

    nJson rootJson;
    try
    {
        file >> rootJson;
        file.close();
        ECO_INFO("Config loaded: %s", strFullPath.c_str());
    }
    catch (const std::exception& e)
    {
        file.close();
        ECO_ERROR("Parse config failed: %s, using defaults", e.what());
        return nJson{};
    }

    return rootJson;
}

inline void
ParseBatteryOption(const nJson& rootJson, BatteryOption& batteryOption)
{
    /* 使用默认值, 逐字段读取 */
    if (!rootJson.is_object() || rootJson.find("batteryOption") == rootJson.end())
    {
        ECO_WARN("No 'batteryOption' key, using defaults");
        return;
    }

    const auto& b = rootJson["batteryOption"];
    try
    {
        if (b.count("tty"))           batteryOption.strTty = b["tty"].get<std::string>();
        if (b.count("baudRate"))      batteryOption.iBaudRate = b["baudRate"].get<int>();
        if (b.count("pollPeriodMs"))  batteryOption.poll_period_ms = b["pollPeriodMs"].get<uint32_t>();
        if (b.count("socPeriodMs"))   batteryOption.soc_period_ms = b["socPeriodMs"].get<uint32_t>();
        if (b.count("faultPeriodMs")) batteryOption.fault_period_ms = b["faultPeriodMs"].get<uint32_t>();
        if (b.count("enableVoltage"))       batteryOption.enable_voltage = b["enableVoltage"].get<bool>();
        if (b.count("enableCurrentTemp"))   batteryOption.enable_current_temp = b["enableCurrentTemp"].get<bool>();
        if (b.count("enableSocCapacity"))   batteryOption.enable_soc_capacity = b["enableSocCapacity"].get<bool>();
        if (b.count("enableFault"))         batteryOption.enable_fault = b["enableFault"].get<bool>();
        if (b.count("enableChargeCurrent")) batteryOption.enable_charge_current = b["enableChargeCurrent"].get<bool>();

        ECO_INFO("battery tty=%s baud=%d poll=%ums soc=%ums fault=%ums",
                 batteryOption.strTty.c_str(), batteryOption.iBaudRate,
                 batteryOption.poll_period_ms, batteryOption.soc_period_ms,
                 batteryOption.fault_period_ms);
    }
    catch (const std::exception& e)
    {
        ECO_ERROR("Parse batteryOption failed: %s, using defaults", e.what());
    }
}

inline void
ParseWebOption(const nJson& rootJson, WebOption& webOption)
{
    if (!rootJson.is_object() || rootJson.find("web") == rootJson.end())
    {
        ECO_WARN("No 'web' key, using defaults (enabled=false)");
        return;
    }

    const auto& w = rootJson["web"];
    try
    {
        if (w.count("enabled")) webOption.enabled = w["enabled"].get<bool>();
        if (w.count("port"))    webOption.port = w["port"].get<uint16_t>();

        ECO_INFO("web enabled=%s port=%u",
                 webOption.enabled ? "true" : "false", webOption.port);
    }
    catch (const std::exception& e)
    {
        ECO_ERROR("Parse web option failed: %s, using defaults", e.what());
    }
}

std::mutex ConfigStorage::m_singleMutex;
std::shared_ptr<ConfigStorage> ConfigStorage::m_instance{ nullptr };

std::shared_ptr<ConfigStorage>
ConfigStorage::GetInstance()
{
    std::lock_guard<std::mutex> lock(m_singleMutex);
    if (!m_instance)
    {
        m_instance = std::shared_ptr<ConfigStorage>(new ConfigStorage());
    }
    return m_instance;
}

ConfigStorage::ConfigStorage()
{
    /* 构造时不加载, 等 Init() 调用 */
}

void
ConfigStorage::Init(const std::string& configPath)
{
    LoadFromJson(configPath);
}

void
ConfigStorage::LoadFromJson(const std::string& configPath)
{
    auto rootJson = LoadConfigFile2Json(configPath, m_strConfigFile);
    ParseBatteryOption(rootJson, m_batteryOption);
    ParseWebOption(rootJson, m_webOption);
}

boost::any
ConfigStorage::GetAny(const TypeId& type)
{
    if (typeid(BatteryOption) == type)
    {
        return m_batteryOption;
    }
    else if (typeid(WebOption) == type)
    {
        return m_webOption;
    }
    else
    {
        ECO_ERROR("[ConfigStorage::GetAny] type %s unsupported", type.pretty_name().c_str());
        return {};
    }
}
