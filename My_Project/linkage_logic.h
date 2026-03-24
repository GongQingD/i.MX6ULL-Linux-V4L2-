#ifndef LINKAGE_LOGIC_H
#define LINKAGE_LOGIC_H

#include <string>

enum class CameraOwner {
    None,
    Auto,
    Manual,
};

enum class CameraChildEvent {
    None,
    UserInteraction,
};

struct SensorSnapshot {
    bool motionDetected = false;
    int als = 0;
};

struct LinkageState {
    bool ledOn = false;
    bool cameraRunning = false;
    CameraOwner cameraOwner = CameraOwner::None;
    long long autoCameraDeadlineMs = 0;
    long long manualLedFreezeUntilMs = 0;
};

struct LinkageDecision {
    bool setLed = false;
    bool ledOn = false;
    bool startCamera = false;
    bool stopCamera = false;
    CameraOwner cameraOwner = CameraOwner::None;
};

void recordManualLedToggle(LinkageState &state, bool ledOn, long long nowMs);
void recordManualCameraStart(LinkageState &state);
void recordManualCameraStop(LinkageState &state);
void recordCameraUserInteraction(LinkageState &state);

bool shouldRestoreMainWindowAfterCameraFinish(bool cameraWasShown,
                                              bool shouldRestart,
                                              bool shuttingDown);

CameraChildEvent parseCameraChildOutputLine(const std::string &line);

LinkageDecision evaluateLinkage(LinkageState &state,
                                const SensorSnapshot &snapshot,
                                long long nowMs);

#endif  // LINKAGE_LOGIC_H
