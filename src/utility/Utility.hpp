#pragma once

#include <regex>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace stark_power_manager
{
inline void
PrintHexVector(const std::vector<uint8_t>& hexVector, const std::string& tag = "TX")
{
    std::cout << "*" << tag << ": ";
    for (const auto& val : hexVector)
    {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(val) << " ";
    }
    std::cout << std::endl;

    // for (const auto& val : hexVector)
    // {
    //     std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << val << " ";
    // }
    // std::cout << std::endl;
}

/**
*@brief 获取最后一个header的索引
*@param
*@return
*/
inline size_t
GetTrimHeaderIndex(const uint8_t* data, size_t size)
{
    size_t index = 0;
    for (size_t i = 0; i < size; ++i)
    {
        if (data[i] == 0x60 && i + 2 < size && data[i + 1] == 0x53 && data[i + 2] == 0x41)
        {
            /**< 找最后一个 */
            index = i;
        }
    }

    return index;
}
}  // namespace stark_power_manager