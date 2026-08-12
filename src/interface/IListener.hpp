/*
 * @Author: colin yuanzhi.yang@ecovacs.com
 * @Date: 2023-11-24 17:34:57
 * @LastEditors: colin yuanzhi.yang@ecovacs.com
 * @LastEditTime: 2023-11-30 11:34:04
 * @FilePath: /stark_power_manager/src/interface/IListener.hpp
 * @Description: 观察者基类
 */
#pragma once

#include <memory>
#include <boost/any.hpp>
#include "interface/Defines.hpp"

namespace stark_power_manager
{
class IMsgInternalDispatcher;

class IListener
{
public:
    virtual ~IListener() = default;

    virtual void
    Update(const boost::any& data) = 0;
};
}  // namespace stark_power_manager