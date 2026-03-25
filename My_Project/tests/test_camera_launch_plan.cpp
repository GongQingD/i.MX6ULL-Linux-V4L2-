#include <cassert>
#include <string>

#include "camera_launch_plan.h"

static void test_builds_login_shell_launch_plan() {
    const CameraLaunchPlan plan =
        buildCameraLaunchPlan("/lib/modules/4.1.15-g3dc0a4b/Camera_Project", "");

    assert(plan.program == "/bin/bash");
    assert(plan.workingDirectory == "/lib/modules/4.1.15-g3dc0a4b");
    assert(plan.qtPlatform == "linuxfb");
    assert(plan.arguments.size() == 3);
    assert(plan.arguments[0] == "-l");
    assert(plan.arguments[1] == "-c");
    assert(plan.arguments[2] == "exec ./Camera_Project");
}

static void test_preserves_inherited_qt_platform_when_present() {
    const CameraLaunchPlan plan =
        buildCameraLaunchPlan("/lib/modules/4.1.15-g3dc0a4b/Camera_Project",
                              "linuxfb:tty=/dev/fb0");

    assert(plan.qtPlatform == "linuxfb:tty=/dev/fb0");
}

int main() {
    test_builds_login_shell_launch_plan();
    test_preserves_inherited_qt_platform_when_present();
    return 0;
}
