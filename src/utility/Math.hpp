#pragma once

#include <iostream>
#include <cmath>
#include <eigen3/Eigen/Core>
#include <log_helper/LogHelper.h>

#include "Defines.hpp"

namespace stark_power_manager
{
inline TPose
CalNextPose(const TPose& lastPose, const TPose& currentPose, const TPose& slamPose = {})
{
    // compute transform matrix T
    float dx = currentPose.x - lastPose.x;
    float dy = currentPose.y - lastPose.y;
    float dTheta = currentPose.theta - lastPose.theta;

    Eigen::Matrix3f T;
    T << cos(dTheta), -sin(dTheta), dx, sin(dTheta), cos(dTheta), dy, 0, 0, 1;

    // transform lastPose to currentPose
    Eigen::Vector3f lastPoseVec;
    lastPoseVec << lastPose.x, lastPose.y, 1;

    Eigen::Vector3f currentPoseVec = T * lastPoseVec;

    // Pose newPose{ currentPoseVec[0], currentPoseVec[1], currentPose.theta };

    // predict next pose
    float forwardDist = 1;
    Eigen::Vector3f nextPoseVec = currentPoseVec + T * Eigen::Vector3f{ dx, dy, 0 };

    TPose nextPose{ nextPoseVec[0], nextPoseVec[1], currentPose.theta };

    ECO_LOG_THROTTLE(5000, "Next pose: (x:{}, y:{}, theta:{})", nextPose.x, nextPose.y, nextPose.theta);

    if (slamPose != TPose())
    {
        Eigen::Vector3f currentSlamPoseVec;
        currentSlamPoseVec << slamPose.x, slamPose.y, 1;

        Eigen::Vector3f nextSlamPoseVec = currentSlamPoseVec + T * Eigen::Vector3f{ dx, dy, 0 };
        TPose nextSlamPose{ nextSlamPoseVec[0], nextSlamPoseVec[1], slamPose.theta };

        ECO_LOG_THROTTLE(5000, "Next pose: (x:%f, y:%f, theta:%f)", nextSlamPose.x, nextSlamPose.y, nextSlamPose.theta);
        return nextSlamPose;
    }

    return nextPose;
}
}  // namespace stark_power_manager