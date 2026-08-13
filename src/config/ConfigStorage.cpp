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

inline void
ParseIp2366Option(const nJson& rootJson, Ip2366Option& o)
{
    if (!rootJson.is_object() || rootJson.find("ip2366") == rootJson.end())
    {
        ECO_WARN("No 'ip2366' key, using defaults");
        return;
    }

    const auto& p = rootJson["ip2366"];
    try
    {
        if (p.count("i2cDev"))              o.i2c_dev = p["i2cDev"].get<std::string>();
        if (p.count("i2cAddr"))             o.i2c_addr = p["i2cAddr"].get<int>();
        if (p.count("intGpioChip"))         o.int_gpio_chip = p["intGpioChip"].get<std::string>();
        if (p.count("intGpioLine"))         o.int_gpio_line = p["intGpioLine"].get<int>();
        if (p.count("chargeEnGpioChip"))    o.charge_en_gpio_chip = p["chargeEnGpioChip"].get<std::string>();
        if (p.count("chargeEnGpioLine"))    o.charge_en_gpio_line = p["chargeEnGpioLine"].get<int>();
        if (p.count("pdoSelect"))           o.pdo_select = p["pdoSelect"].get<int>();
        if (p.count("chargeVoltageMv"))     o.charge_voltage_mv = p["chargeVoltageMv"].get<int>();
        if (p.count("chargeCurrentMa"))     o.charge_current_ma = p["chargeCurrentMa"].get<int>();
        if (p.count("trickleCurrentMa"))    o.trickle_current_ma = p["trickleCurrentMa"].get<int>();
        if (p.count("stopCurrentMa"))       o.stop_current_ma = p["stopCurrentMa"].get<int>();
        if (p.count("intPollIntervalMs"))   o.int_poll_interval_ms = p["intPollIntervalMs"].get<int>();

        ECO_INFO("ip2366 i2c=%s addr=0x%02X int=%s:%d chg_en=%s:%d",
                 o.i2c_dev.c_str(), o.i2c_addr,
                 o.int_gpio_chip.c_str(), o.int_gpio_line,
                 o.charge_en_gpio_chip.c_str(), o.charge_en_gpio_line);
    }
    catch (const std::exception& e)
    {
        ECO_ERROR("Parse ip2366 option failed: %s, using defaults", e.what());
    }
}

inline void
ParsePowerManagerOption(const nJson& rootJson, PowerManagerOption& o)
{
    if (!rootJson.is_object() || rootJson.find("powerManager") == rootJson.end())
    {
        ECO_WARN("No 'powerManager' key, using defaults");
        return;
    }

    const auto& p = rootJson["powerManager"];
    try
    {
        if (p.count("adapterDebounceMs"))   o.adapter_debounce_ms = p["adapterDebounceMs"].get<int>();
        if (p.count("faultDebounceMs"))     o.fault_debounce_ms = p["faultDebounceMs"].get<int>();
        if (p.count("recoveryDebounceMs"))  o.recovery_debounce_ms = p["recoveryDebounceMs"].get<int>();
        if (p.count("fullSocPercent"))      o.full_soc_percent = p["fullSocPercent"].get<int>();
        if (p.count("fullCurrentMa"))       o.full_current_ma = p["fullCurrentMa"].get<int>();
        if (p.count("rechargeHystMv"))      o.recharge_hyst_mv = p["rechargeHystMv"].get<int>();
        if (p.count("ccTimeoutMin"))        o.cc_timeout_min = p["ccTimeoutMin"].get<int>();
        if (p.count("totalTimeoutMin"))     o.total_timeout_min = p["totalTimeoutMin"].get<int>();
        if (p.count("maxTempC"))            o.max_temp_c = p["maxTempC"].get<int>();
        if (p.count("resumeTempC"))         o.resume_temp_c = p["resumeTempC"].get<int>();
        if (p.count("pdTimeoutMs"))         o.pd_timeout_ms = p["pdTimeoutMs"].get<int>();
        if (p.count("crossVerifyCharge"))   o.cross_verify_charge = p["crossVerifyCharge"].get<bool>();
        if (p.count("vbusPresentMv"))       o.vbus_present_mv = p["vbusPresentMv"].get<int>();

        ECO_INFO("powerManager crossVerify=%s maxTemp=%dC",
                 o.cross_verify_charge ? "true" : "false", o.max_temp_c);
    }
    catch (const std::exception& e)
    {
        ECO_ERROR("Parse powerManager option failed: %s, using defaults", e.what());
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
    ParseIp2366Option(rootJson, m_ip2366Option);
    ParsePowerManagerOption(rootJson, m_powerOption);
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
    else if (typeid(Ip2366Option) == type)
    {
        return m_ip2366Option;
    }
    else if (typeid(PowerManagerOption) == type)
    {
        return m_powerOption;
    }
    else
    {
        ECO_ERROR("[ConfigStorage::GetAny] type %s unsupported", type.pretty_name().c_str());
        return {};
    }
}
