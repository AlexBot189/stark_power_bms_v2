/*
 * @Author: colin yuanzhi.yang@ecovacs.com
 * @Date: 2023-11-22 17:58:57
 * @LastEditors: colin yuanzhi.yang@ecovacs.com
 * @LastEditTime: 2023-11-30 14:33:32
 * @FilePath: /stark_power_manager/src/protocol/IMsgInternalDispatcher.hpp
 * @Description: 内部消息处理基类（被观察者基类）
 */
#pragma once

#include <boost/any.hpp>
#include <boost/optional.hpp>
#include "interface/Defines.hpp"
#include "interface/IListener.hpp"

namespace stark_power_manager
{
class IMsgInternalDispatcher
{
public:
    IMsgInternalDispatcher() = default;

    virtual ~IMsgInternalDispatcher() = default;

    virtual bool
    InitDispatcher() = 0;

    virtual bool
    DestroyDispatcher() = 0;

    virtual void
    Send(const std::string& data) = 0;

    virtual std::string
    OnRecvData() = 0;

    virtual void
    RegisterObserver(ListenerType type, std::shared_ptr<IListener> listener) = 0;

    virtual void
    RemoveObserver(ListenerType type, std::shared_ptr<IListener> listener) = 0;

    virtual void
    NotifyObserver(const boost::any& data) = 0;
};
}  // namespace stark_power_manager