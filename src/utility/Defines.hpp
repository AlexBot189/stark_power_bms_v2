#pragma once

#include <iostream>

namespace stark_power_manager
{
struct TPose
{
    float x;
    float y;
    float theta;

    bool operator !=(const TPose& other) const
    {
        return !(x == other.x && y == other.y && theta == other.theta);
    }
};

struct TLdsDot
{
    unsigned int index{ 0 };
    float theta{ 0.0 };  //-pi~pi
    float rho{ 0.0 };
    float power{ 0.0 };
    float x{ 0.0 };
    float y{ 0.0 };
};

struct PolarCoordinate
{
    double r;
    double theta;
};
}  // namespace stark_power_manager