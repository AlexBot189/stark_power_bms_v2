#pragma once

namespace stark_power_manager
{
class ILineLaserAdapter
{
public:
    virtual ~ILineLaserAdapter() = default;

    virtual int
    StartNode() = 0;
};
}  // namespace stark_power_manager