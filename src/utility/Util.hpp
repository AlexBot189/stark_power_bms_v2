#pragma once

#include <cmath>
#include <bitset>
#include <unistd.h>
#include <syscall.h>
#include <signal.h>
#include <execinfo.h>
// #include <sys/statfs.h>

namespace stark_power_manager
{
inline pid_t
GetPid()
{
    return (pid_t)syscall(SYS_gettid);
}

/**
*@brief 将数字转成二进制字符串
*/
template<typename Type, std::size_t N = sizeof(Type) * 8>
inline std::string
Num2BinaryString(Type num)
{
    std::bitset<N> bits(num);
    return bits.to_string();
}
}  // namespace stark_power_manager