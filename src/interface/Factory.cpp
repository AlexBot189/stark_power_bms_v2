#include "Factory.hpp"
#include "gw_adapter/WebServer.hpp"
#include "protocol/UartDispatcher.hpp"

using namespace stark_power_manager;

std::shared_ptr<IMsgInternalDispatcher>
Factory::CreateSingletonDispatcher(const DeviceOption& option, std::shared_ptr<ros::NodeHandle> nh)
{
    if (option.strType == "UART")
    {
        static std::mutex mtx;
        static std::shared_ptr<UartDispatcher> pUartDispatcher = nullptr;

        std::lock_guard<std::mutex> lock(mtx);
        if (!pUartDispatcher)
        {
            pUartDispatcher = std::make_shared<UartDispatcher>(option.uartOption);
        }

        return pUartDispatcher;
    }

    std::string strError = "Msg type '" + option.strType + "' not support";
    throw std::runtime_error(strError.c_str());
}

std::shared_ptr<IListener>
Factory::CreateWebListener(const std::shared_ptr<RosAdapter>& ros_adapter, const GwOption& option)
{
    return std::make_shared<WebServer>(ros_adapter);
}

std::shared_ptr<TouchMonitorAdapter>
Factory::CreateTouchAdapter(const TouchOption& touchOption, std::shared_ptr<ros::NodeHandle> nh)
{
    DeviceOption devOption;
    devOption.strType = "UART";

    auto uartDispatcherBase = Factory::CreateSingletonDispatcher(devOption, nh);
    auto uartDispatcher = std::dynamic_pointer_cast<UartDispatcher>(uartDispatcherBase);

    if (!uartDispatcher)
    {
        ECO_INFO_NEW("Failed to get UartDispatcher singleton");
    }

    // 创建 TouchMonitorAdapter
    auto touchAdapter = std::make_shared<TouchMonitorAdapter>(
        touchOption.gpioChipPath,
        touchOption.headLine,
        touchOption.chinLine,
        touchOption.LeftEarLine,
        touchOption.RightEarLine,
        nh,
        uartDispatcher
    );
   
    if (uartDispatcher)
    {
        uartDispatcher->SetTouchAdapter(touchAdapter);
    }
    
    return touchAdapter;
}
