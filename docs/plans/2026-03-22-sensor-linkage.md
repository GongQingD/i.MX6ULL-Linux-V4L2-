# Sensor Linkage Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a state-machine-based linkage flow in `My_Project` so SR501 auto-starts the camera for 5 seconds after motion, and AP3216C auto-controls the LED when ambient light is dark.

**Architecture:** Keep `Camera_Project` and the kernel drivers unchanged. Extract the linkage rules into a pure C++ module that can be tested on the host, move sensor sampling into a dedicated `QThread` worker, and keep all UI, `QProcess`, and `/dev/led` writes on the main thread.

**Tech Stack:** Qt Widgets, QThread, QProcess, Linux character devices (`/dev/sr501`, `/dev/ap3216c`, `/dev/led`), plain C++11 host-side tests compiled with `g++`.

---

### Task 1: Extract Pure Linkage Logic With Host-Side Tests

**Files:**
- Create: `My_Project/linkage_logic.h`
- Create: `My_Project/linkage_logic.cpp`
- Create: `My_Project/tests/test_linkage_logic.cpp`

**Step 1: Write the failing test**

```cpp
#include <cassert>
#include "linkage_logic.h"

static void test_motion_starts_camera_for_5_seconds() {
    LinkageState state{};
    SensorSnapshot snapshot{};
    snapshot.motionDetected = true;
    snapshot.als = 40;

    LinkageDecision d1 = evaluateLinkage(state, snapshot, 1000);
    assert(d1.startCamera);
    assert(d1.cameraOwner == CameraOwner::Auto);
    assert(state.autoCameraDeadlineMs == 6000);

    snapshot.motionDetected = false;
    LinkageDecision d2 = evaluateLinkage(state, snapshot, 6501);
    assert(d2.stopCamera);
}

static void test_dark_turns_led_on_and_bright_turns_it_off() {
    LinkageState state{};
    SensorSnapshot snapshot{};

    snapshot.als = 60;
    LinkageDecision d1 = evaluateLinkage(state, snapshot, 1000);
    assert(d1.setLed);
    assert(d1.ledOn);

    snapshot.als = 140;
    LinkageDecision d2 = evaluateLinkage(state, snapshot, 2000);
    assert(d2.setLed);
    assert(!d2.ledOn);
}

int main() {
    test_motion_starts_camera_for_5_seconds();
    test_dark_turns_led_on_and_bright_turns_it_off();
    return 0;
}
```

**Step 2: Run test to verify it fails**

Run: `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic`

Expected: FAIL with `linkage_logic.h: No such file or directory` or `My_Project/linkage_logic.cpp: No such file or directory`

**Step 3: Write minimal implementation**

```cpp
enum class CameraOwner { None, Auto, Manual };

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

LinkageDecision evaluateLinkage(LinkageState &state,
                                const SensorSnapshot &snapshot,
                                long long nowMs);
```

```cpp
static const int kAlsDarkThreshold = 80;
static const int kAlsBrightThreshold = 120;
static const long long kMotionHoldMs = 5000;

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
               nowMs > state.autoCameraDeadlineMs) {
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
```

**Step 4: Run test to verify it passes**

Run: `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`

Expected: PASS with exit code `0`

**Step 5: Commit**

```bash
git add My_Project/linkage_logic.h My_Project/linkage_logic.cpp My_Project/tests/test_linkage_logic.cpp
git commit -m "feat: add tested sensor linkage logic"
```

### Task 2: Move Sensor Sampling Into a Dedicated Worker Thread

**Files:**
- Create: `My_Project/sensor_worker.h`
- Create: `My_Project/sensor_worker.cpp`
- Modify: `My_Project/My_Project.pro`
- Modify: `My_Project/mainwindow.h`
- Modify: `My_Project/mainwindow.cpp`

**Step 1: Write the failing integration change**

```cpp
// mainwindow.h
class QThread;
class SensorWorker;

private:
    QThread *sensorThread;
    SensorWorker *sensorWorker;

private slots:
    void handleLinkageDecision(const LinkageDecision &decision);
```

```cpp
// mainwindow.cpp constructor excerpt
sensorThread = new QThread(this);
sensorWorker = new SensorWorker(sr501_drv, ap3216c_drv);
sensorWorker->moveToThread(sensorThread);
connect(sensorThread, &QThread::started, sensorWorker, &SensorWorker::start);
connect(sensorWorker, &SensorWorker::decisionReady, this, &MainWindow::handleLinkageDecision);
sensorThread->start();
```

**Step 2: Run build to verify it fails**

Run: `qmake My_Project/My_Project.pro -o /tmp/My_Project.Makefile && make -f /tmp/My_Project.Makefile -j4`

Expected: FAIL with `SensorWorker` or `LinkageDecision` not declared / undefined

**Step 3: Write minimal implementation**

```cpp
class SensorWorker : public QObject {
    Q_OBJECT
public:
    SensorWorker(const QString &sr501Path, const QString &ap3216cPath);

public slots:
    void start();
    void stop();

signals:
    void sensorSnapshotReady(int als, int ps, int ir, int motion);
    void decisionReady(const LinkageDecision &decision);
    void workerError(const QString &message);

private slots:
    void pollSensors();

private:
    int sr501Fd = -1;
    int ap3216cFd = -1;
    QTimer *pollTimer = nullptr;
    LinkageState linkageState;
};
```

```cpp
void SensorWorker::pollSensors() {
    SensorSnapshot snapshot{};
    snapshot.motionDetected = readSr501Motion(sr501Fd);
    snapshot.als = readAp3216cAls(ap3216cFd);

    const long long nowMs = QDateTime::currentMSecsSinceEpoch();
    emit decisionReady(evaluateLinkage(linkageState, snapshot, nowMs));
    emit sensorSnapshotReady(snapshot.als, currentPs, currentIr, snapshot.motionDetected ? 1 : 0);
}
```

Add these files to `My_Project/My_Project.pro`:

```pro
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    sensor_worker.cpp \
    linkage_logic.cpp

HEADERS += \
    mainwindow.h \
    sensor_worker.h \
    linkage_logic.h
```

**Step 4: Run build to verify it passes**

Run: `qmake My_Project/My_Project.pro -o /tmp/My_Project.Makefile && make -f /tmp/My_Project.Makefile -j4`

Expected: PASS and produce `My_Project`

**Step 5: Commit**

```bash
git add My_Project/My_Project.pro My_Project/mainwindow.h My_Project/mainwindow.cpp My_Project/sensor_worker.h My_Project/sensor_worker.cpp
git commit -m "feat: move sensor linkage into worker thread"
```

### Task 3: Add Manual Override Rules and Main-Thread Actions

**Files:**
- Modify: `My_Project/linkage_logic.h`
- Modify: `My_Project/linkage_logic.cpp`
- Modify: `My_Project/tests/test_linkage_logic.cpp`
- Modify: `My_Project/mainwindow.cpp`
- Modify: `My_Project/mainwindow.h`

**Step 1: Write the failing test**

```cpp
static void test_manual_camera_is_not_stopped_by_auto_timeout() {
    LinkageState state{};
    state.cameraRunning = true;
    state.cameraOwner = CameraOwner::Manual;
    state.autoCameraDeadlineMs = 6000;

    SensorSnapshot snapshot{};
    snapshot.motionDetected = false;

    LinkageDecision d = evaluateLinkage(state, snapshot, 6501);
    assert(!d.stopCamera);
}

static void test_manual_led_freeze_blocks_auto_toggle_for_60_seconds() {
    LinkageState state{};
    state.manualLedFreezeUntilMs = 61000;
    state.ledOn = false;

    SensorSnapshot snapshot{};
    snapshot.als = 10;

    LinkageDecision d = evaluateLinkage(state, snapshot, 5000);
    assert(!d.setLed);
    assert(!state.ledOn);
}
```

**Step 2: Run test to verify it fails**

Run: `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`

Expected: FAIL on the new manual-override assertions

**Step 3: Write minimal implementation**

```cpp
static const long long kManualLedFreezeMs = 60000;

void recordManualLedToggle(LinkageState &state, bool ledOn, long long nowMs) {
    state.ledOn = ledOn;
    state.manualLedFreezeUntilMs = nowMs + kManualLedFreezeMs;
}

void recordManualCameraStart(LinkageState &state) {
    state.cameraRunning = true;
    state.cameraOwner = CameraOwner::Manual;
}

void recordManualCameraStop(LinkageState &state) {
    state.cameraRunning = false;
    state.cameraOwner = CameraOwner::None;
    state.autoCameraDeadlineMs = 0;
}
```

```cpp
void MainWindow::handleLinkageDecision(const LinkageDecision &decision) {
    if (decision.setLed) {
        writeLedState(decision.ledOn ? 1 : 0);
    }

    if (decision.startCamera) {
        startCameraProcess(CameraOwner::Auto);
    }

    if (decision.stopCamera) {
        stopCameraProcess(CameraOwner::Auto);
    }
}
```

Also update the manual UI handlers so:
- LED button calls `recordManualLedToggle(...)` before writing `/dev/led`
- Camera button calls `recordManualCameraStart(...)`
- Camera exit path calls `recordManualCameraStop(...)`

**Step 4: Run tests and build to verify they pass**

Run: `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`

Expected: PASS with exit code `0`

Run: `qmake My_Project/My_Project.pro -o /tmp/My_Project.Makefile && make -f /tmp/My_Project.Makefile -j4`

Expected: PASS

**Step 5: Commit**

```bash
git add My_Project/linkage_logic.h My_Project/linkage_logic.cpp My_Project/tests/test_linkage_logic.cpp My_Project/mainwindow.h My_Project/mainwindow.cpp
git commit -m "feat: add manual override aware sensor actions"
```

### Task 4: Update Repo Documentation and Board Verification Notes

**Files:**
- Modify: `README.md`

**Step 1: Write the failing documentation check**

Run: `rg -n "5 seconds|manual override|worker thread|AP3216C auto-controls the LED" README.md`

Expected: FAIL with no matches

**Step 2: Add the minimal documentation update**

Add one short subsection under the `My_Project` runtime logic:

```md
- Sensor linkage runs in a dedicated worker thread.
- SR501 motion auto-starts `Camera_Project` and keeps it alive for 5 seconds after the last motion event.
- AP3216C ALS uses hysteresis thresholds to auto-control the LED.
- Manual camera actions override auto-stop, and manual LED toggles freeze auto-lighting for 60 seconds.
```

**Step 3: Verify the documentation is present**

Run: `rg -n "5 seconds|manual override|worker thread|AP3216C ALS uses hysteresis" README.md`

Expected: PASS with matching lines

**Step 4: Record board-side verification commands**

Run on board after deployment:

```bash
ls -l /dev/led /dev/ap3216c /dev/sr501
./My_Project
```

Manual verification:
- Trigger SR501 once and confirm the camera starts without pressing the UI button.
- Stop moving and confirm the auto-started camera exits about 5 seconds later.
- Cover the light sensor and confirm LED turns on.
- Restore bright light and confirm LED turns off.
- Manually start the camera and confirm the 5-second auto-stop does not kill the manual session.

**Step 5: Commit**

```bash
git add README.md
git commit -m "docs: describe sensor linkage runtime rules"
```
