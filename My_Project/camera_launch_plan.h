#ifndef CAMERA_LAUNCH_PLAN_H
#define CAMERA_LAUNCH_PLAN_H

#include <string>
#include <vector>

struct CameraLaunchPlan {
    std::string program;
    std::vector<std::string> arguments;
    std::string workingDirectory;
    std::string qtPlatform;
};

CameraLaunchPlan buildCameraLaunchPlan(const std::string &cameraAppPath,
                                       const std::string &inheritedQtPlatform);

#endif // CAMERA_LAUNCH_PLAN_H
