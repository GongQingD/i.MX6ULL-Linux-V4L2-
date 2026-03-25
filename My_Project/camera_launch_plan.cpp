#include "camera_launch_plan.h"

#include <cstddef>

namespace {

std::string dirnameOf(const std::string &path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return "/";
    }
    return path.substr(0, slash);
}

std::string basenameOf(const std::string &path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

} // namespace

CameraLaunchPlan buildCameraLaunchPlan(const std::string &cameraAppPath,
                                       const std::string &inheritedQtPlatform) {
    CameraLaunchPlan plan;
    const std::string executableName = basenameOf(cameraAppPath);

    plan.program = "/bin/bash";
    plan.arguments.push_back("-l");
    plan.arguments.push_back("-c");
    plan.arguments.push_back("exec ./" + executableName);
    plan.workingDirectory = dirnameOf(cameraAppPath);
    plan.qtPlatform = inheritedQtPlatform.empty() ? "linuxfb" : inheritedQtPlatform;
    return plan;
}
