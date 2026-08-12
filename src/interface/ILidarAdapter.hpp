#pragma once

namespace stark_power_manager
{
class ILidarAdapter
{
public:
    virtual ~ILidarAdapter() = default;

    virtual int
    StartNode() = 0;
};
}  // namespace stark_power_manager