#include <cassert>

#include "linkage_logic.h"

static void test_motion_starts_camera_and_stops_at_timeout_boundary() {
    LinkageState state{};
    SensorSnapshot snapshot{};

    snapshot.motionDetected = true;
    LinkageDecision d1 = evaluateLinkage(state, snapshot, 1000);
    assert(d1.startCamera);
    assert(!d1.stopCamera);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Auto);
    assert(state.autoCameraDeadlineMs == 6000);

    snapshot.motionDetected = false;
    LinkageDecision d2 = evaluateLinkage(state, snapshot, 5999);
    assert(!d2.stopCamera);
    assert(state.cameraRunning);

    LinkageDecision d3 = evaluateLinkage(state, snapshot, 6000);
    assert(d3.stopCamera);
    assert(!state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::None);
}

static void test_motion_retrigger_extends_auto_camera_deadline() {
    LinkageState state{};
    SensorSnapshot snapshot{};

    snapshot.motionDetected = true;
    LinkageDecision d1 = evaluateLinkage(state, snapshot, 1000);
    assert(d1.startCamera);
    assert(state.autoCameraDeadlineMs == 6000);

    LinkageDecision d2 = evaluateLinkage(state, snapshot, 4000);
    assert(!d2.startCamera);
    assert(state.autoCameraDeadlineMs == 9000);
    assert(state.cameraRunning);

    snapshot.motionDetected = false;
    LinkageDecision d3 = evaluateLinkage(state, snapshot, 8999);
    assert(!d3.stopCamera);
    assert(state.cameraRunning);

    LinkageDecision d4 = evaluateLinkage(state, snapshot, 9000);
    assert(d4.stopCamera);
    assert(!state.cameraRunning);
}

static void test_als_hysteresis_controls_led() {
    LinkageState state{};
    SensorSnapshot snapshot{};

    snapshot.als = 80;
    LinkageDecision d1 = evaluateLinkage(state, snapshot, 1000);
    assert(d1.setLed);
    assert(d1.ledOn);
    assert(state.ledOn);

    snapshot.als = 100;
    LinkageDecision d2 = evaluateLinkage(state, snapshot, 1500);
    assert(!d2.setLed);
    assert(state.ledOn);

    snapshot.als = 120;
    LinkageDecision d3 = evaluateLinkage(state, snapshot, 2000);
    assert(d3.setLed);
    assert(!d3.ledOn);
    assert(!state.ledOn);

    snapshot.als = 100;
    LinkageDecision d4 = evaluateLinkage(state, snapshot, 2500);
    assert(!d4.setLed);
    assert(!state.ledOn);
}

static void test_state_has_manual_led_freeze_until_ms() {
    LinkageState state{};
    assert(state.manualLedFreezeUntilMs == 0);
    state.manualLedFreezeUntilMs = 1234;
    assert(state.manualLedFreezeUntilMs == 1234);
}

static void test_motion_does_not_override_manual_camera_owner_when_running() {
    LinkageState state{};
    state.cameraRunning = true;
    state.cameraOwner = CameraOwner::Manual;

    SensorSnapshot snapshot{};
    snapshot.motionDetected = true;

    LinkageDecision d = evaluateLinkage(state, snapshot, 1000);
    assert(!d.startCamera);
    assert(!d.stopCamera);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Manual);
    assert(state.autoCameraDeadlineMs == 6000);
}

static void test_manual_camera_is_not_stopped_by_auto_timeout() {
    LinkageState state{};
    state.cameraRunning = true;
    state.cameraOwner = CameraOwner::Manual;
    state.autoCameraDeadlineMs = 6000;

    SensorSnapshot snapshot{};
    snapshot.motionDetected = false;

    LinkageDecision d = evaluateLinkage(state, snapshot, 6501);
    assert(!d.startCamera);
    assert(!d.stopCamera);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Manual);
    assert(state.autoCameraDeadlineMs == 6000);
}

static void test_manual_led_freeze_blocks_auto_toggle() {
    LinkageState state{};
    state.ledOn = false;
    state.manualLedFreezeUntilMs = 61000;

    SensorSnapshot snapshot{};
    snapshot.als = 10;

    LinkageDecision d = evaluateLinkage(state, snapshot, 5000);
    assert(!d.setLed);
    assert(!state.ledOn);
}

static void test_camera_user_interaction_promotes_auto_session_to_manual() {
    LinkageState state{};
    SensorSnapshot snapshot{};

    snapshot.motionDetected = true;
    LinkageDecision started = evaluateLinkage(state, snapshot, 1000);
    assert(started.startCamera);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Auto);

    recordCameraUserInteraction(state);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Manual);
    assert(state.autoCameraDeadlineMs == 0);

    snapshot.motionDetected = false;
    LinkageDecision afterTimeout = evaluateLinkage(state, snapshot, 7001);
    assert(!afterTimeout.stopCamera);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Manual);
}

static void test_restore_main_window_when_camera_session_finishes() {
    assert(!shouldRestoreMainWindowAfterCameraFinish(false, false, false));
    assert(!shouldRestoreMainWindowAfterCameraFinish(true, true, false));
    assert(!shouldRestoreMainWindowAfterCameraFinish(true, false, true));
    assert(shouldRestoreMainWindowAfterCameraFinish(true, false, false));
}

static void test_parse_camera_child_output_line() {
    assert(parseCameraChildOutputLine("random warning") == CameraChildEvent::None);
    assert(parseCameraChildOutputLine("CAMERA_EVENT USER_INTERACTION CAPTURE")
           == CameraChildEvent::UserInteraction);
    assert(parseCameraChildOutputLine("CAMERA_EVENT USER_INTERACTION EXIT")
           == CameraChildEvent::UserInteraction);
}

static void test_manual_helpers_update_state() {
    LinkageState state{};

    recordManualLedToggle(state, true, 1000);
    assert(state.ledOn);
    assert(state.manualLedFreezeUntilMs == 61000);

    recordManualCameraStart(state);
    assert(state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::Manual);

    recordManualCameraStop(state);
    assert(!state.cameraRunning);
    assert(state.cameraOwner == CameraOwner::None);
    assert(state.autoCameraDeadlineMs == 0);
}

int main() {
    test_motion_starts_camera_and_stops_at_timeout_boundary();
    test_motion_retrigger_extends_auto_camera_deadline();
    test_als_hysteresis_controls_led();
    test_state_has_manual_led_freeze_until_ms();
    test_motion_does_not_override_manual_camera_owner_when_running();
    test_manual_camera_is_not_stopped_by_auto_timeout();
    test_manual_led_freeze_blocks_auto_toggle();
    test_camera_user_interaction_promotes_auto_session_to_manual();
    test_restore_main_window_when_camera_session_finishes();
    test_parse_camera_child_output_line();
    test_manual_helpers_update_state();
    return 0;
}
