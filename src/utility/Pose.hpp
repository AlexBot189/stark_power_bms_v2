#pragma once

#include <cmath>
#include "utility/Defines.hpp"
#include "eros_common/Utility.hpp"

namespace stark_power_manager
{
inline float
GetPoseTheta(const TPose& pose)
{
    return pose.theta;
}

inline float
GetPoseX(const TPose& pose)
{
    return pose.x;
}

inline float
GetPoseY(const TPose& pose)
{
    return pose.y;
}

/**
*@brief 计算两个点之间的距离
*/
inline float
DotDistance(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    float ans = sqrtf(dx * dx + dy * dy);
    return ans;
}

/**
*@brief 极坐标转笛卡尔坐标
*@param theta：单位为弧度
*/
inline TPose
Polar2Cartesian(const float &r, const float &degree)
{
    TPose pose;

    /**< 将角度单位由度转为弧度 */
    pose.theta = degree * M_PI / 180.0;
    pose.x = r * std::cos(pose.theta);
    pose.y = r * std::sin(pose.theta);

    return pose;
}

inline TPose
Polar2CartesianRad(const float &r, const float &rad)
{
    TPose pose;
    pose.x = r * std::cos(rad);
    pose.y = r * std::sin(rad);
    return pose;
}

/**
*@brief 笛卡尔坐标转极坐标
*/
inline PolarCoordinate
Cartesian2Polar(const float& x, const float& y)
{
    PolarCoordinate polar;
    polar.r = std::sqrt(x * x + y * y);
    float theta = std::atan2(y, x);
    theta = std::fmod(theta, 2 * M_PI);
    if (theta < 0)
    {
        theta += 2 * M_PI;
    }

    polar.theta = theta;
    return polar;
}

/**
*@brief 点云坐标转到机器坐标系
*/
inline TLdsDot
LdsCoordinate2RobotCoordinate(const TPose& lds2Robot, const TLdsDot& pDot)
{
    TLdsDot ret;
    /**< 默认值 */
    ret.index = pDot.index;
    float theta = GetPoseTheta(lds2Robot);

    ret.x = pDot.x * cosf(theta) - pDot.y * sinf(theta) + GetPoseX(lds2Robot);

    ret.y = pDot.x * sinf(theta) + pDot.y * cosf(theta) + GetPoseY(lds2Robot);

    ret.rho = DotDistance(0, 0, ret.x, ret.y);
    ret.theta = atan2f(ret.y, ret.x);
    /**< 默认值 */
    ret.power = pDot.power;
    return ret;
}

/**
*@brief 点云坐标转到机器坐标系（重载）
*/
inline TPose
LdsCoordinate2RobotCoordinate(const TPose& lds2Robot, const TPose& pDot)
{
    TPose ret;
    float theta = lds2Robot.theta;

    /**< 此处角度应该为弧度 */
    ret.x = pDot.x * cosf(theta) + pDot.y * sinf(theta) + GetPoseX(lds2Robot);
    ret.y = -pDot.x * sinf(theta) + pDot.y * cosf(theta) + GetPoseY(lds2Robot);

    /**< 没用到，避免随机值 */
    ret.theta = 31415926;

    return ret;
}

inline TPose
PosePlus(const TPose& originPose, const TPose& dPose)
{
    TPose ret;
    ret.x = dPose.x * cosf(originPose.theta) - dPose.y * sinf(originPose.theta) + originPose.x;
    ret.y = dPose.x * sinf(originPose.theta) + dPose.y * cosf(originPose.theta) + originPose.y;
    ret.theta = eros_common::Pi2Pi(originPose.theta + dPose.theta);
    return ret;
}

inline TPose
PoseMinus(const TPose& originPose, const TPose& pose)
{
    float dy = pose.y - originPose.y;
    float dx = pose.x - originPose.x;
    TPose ret;
    ret.x = dx * cosf(originPose.theta) + dy * sinf(originPose.theta);
    ret.y = -dx * sinf(originPose.theta) + dy * cosf(originPose.theta);
    ret.theta = eros_common::Pi2Pi(pose.theta - originPose.theta);
    return ret;
}
}  // namespace stark_power_manager