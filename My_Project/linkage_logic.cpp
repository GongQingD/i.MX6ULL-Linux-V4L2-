#include "linkage_logic.h"

namespace {
constexpr int kAlsDarkThreshold = 80;
constexpr int kAlsBrightThreshold = 120;
constexpr long long kMotionHoldMs = 5000;
constexpr long long kManualLedFreezeMs = 60000;
}  // namespace

void recordManualLedToggle(LinkageState &state, bool ledOn, long long nowMs) {
    state.ledOn = ledOn;
    if (nowMs > 0) {
        state.manualLedFreezeUntilMs = nowMs + kManualLedFreezeMs;
    }
}

void recordManualCameraStart(LinkageState &state) {
    state.cameraRunning = true;
    state.cameraOwner = CameraOwner::Manual;
    state.autoCameraDeadlineMs = 0;
}

void recordManualCameraStop(LinkageState &state) {
    state.cameraRunning = false;
    state.cameraOwner = CameraOwner::None;
    state.autoCameraDeadlineMs = 0;
}

void recordCameraUserInteraction(LinkageState &state) {
    state.cameraRunning = true;
    state.cameraOwner = CameraOwner::Manual;
    state.autoCameraDeadlineMs = 0;
}

bool shouldRestoreMainWindowAfterCameraFinish(bool cameraWasShown,
                                              bool shouldRestart,
                                              bool shuttingDown) {
    return cameraWasShown && !shouldRestart && !shuttingDown;
}

CameraChildEvent parseCameraChildOutputLine(const std::string &line) {
    if (line.find("CAMERA_EVENT USER_INTERACTION") == 0) {
        return CameraChildEvent::UserInteraction;
    }

    return CameraChildEvent::None;
}

LinkageDecision evaluateLinkage(LinkageState &state,
                                const SensorSnapshot &snapshot,
                                long long nowMs) {
    LinkageDecision decision{};

    if (snapshot.motionDetected) {
        state.autoCameraDeadlineMs = nowMs + kMotionHoldMs;
        if (!state.cameraRunning) {
            state.cameraRunning = true;
            state.cameraOwner = CameraOwner::Auto;
            decision.startCamera = true;
            decision.cameraOwner = CameraOwner::Auto;
        }
    } else if (state.cameraRunning &&
               state.cameraOwner == CameraOwner::Auto &&
               nowMs >= state.autoCameraDeadlineMs) {
        state.cameraRunning = false;
        state.cameraOwner = CameraOwner::None;
        decision.stopCamera = true;
    }

    if (nowMs >= state.manualLedFreezeUntilMs) {
        if (!state.ledOn && snapshot.als <= kAlsDarkThreshold) {
            state.ledOn = true;
            decision.setLed = true;
            decision.ledOn = true;
        } else if (state.ledOn && snapshot.als >= kAlsBrightThreshold) {
            state.ledOn = false;
            decision.setLed = true;
            decision.ledOn = false;
        }
    }

    return decision;
}
