#pragma once

#include <string>
#include <iostream>
// #include <cstdio>
// #include <cstring>
#include <regex>

#define _X86

namespace stark_power_manager
{
inline std::string
GetIPAddress()
{
    std::string ipAddress = "";
    std::string interfaceName = "wlan0";

// FIXME(colin): 真机调试必须注释掉
/**< BUILD_PLATFORM 在cmake中设置 */
// #if defined(_X86)
//     interfaceName = "ens160";
// #endif

    // 构建系统命令
    std::string command = "ifconfig " + std::string(interfaceName);

    // 执行系统命令并读取输出
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe)
    {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            std::string line(buffer);

            // 适应不同的输出格式
            std::regex regex1("inet ([0-9\\.]+)");       // 适应 x86 平台的输出格式
            std::regex regex2("inet addr:([0-9\\.]+)");  // 适应其他平台的输出格式

            std::smatch match;
            if (std::regex_search(line, match, regex1) || std::regex_search(line, match, regex2))
            {
                ipAddress = match[1];
                break;
            }
        }
        pclose(pipe);
    }

    return ipAddress;
}
}  // namespace stark_power_manager