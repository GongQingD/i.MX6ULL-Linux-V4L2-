# Linkage Performance Collection Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a lightweight board-side `PERF_EVENT` logging path plus Mac-side collection and analysis scripts, so the repo can compute preview FPS, motion-to-camera latency, dark-light LED latency, continuous run duration, and linkage trigger success rate from real board runs.

**Architecture:** Keep the board side minimal: `My_Project` and `Camera_Project` only emit timestamped `PERF_EVENT` lines at key state transitions and per-second FPS windows. The Mac host captures one run log through SSH, then parses it into `summary.json` and a paste-ready sentence that fills the user’s performance template without hand-counting logs.

**Tech Stack:** Qt Widgets, `QDateTime`, `QProcess`, C++11, Python 3 standard library, existing `scripts/remote_qt_build.sh` and `scripts/preload_drivers.sh`.

---

### Task 1: Add A Shared PERF_EVENT Formatter

**Files:**
- Create: `common/perf_event.h`
- Modify: `Camera_Project/Camera_Project.pro`
- Modify: `My_Project/My_Project.pro`
- Test: `scripts/tests/test_analyze_linkage_perf.py`

**Step 1: Write the failing test**

Create a Python parser test that expects log lines in this exact format:

```python
def test_parse_perf_event_line():
    line = (
        "PERF_EVENT ts_ms=123456 app=My_Project event=motion_rise "
        "session_id=7 source=sr501 value=1"
    )
    event = parse_perf_event_line(line)
    assert event["ts_ms"] == 123456
    assert event["app"] == "My_Project"
    assert event["event"] == "motion_rise"
    assert event["session_id"] == "7"
```

**Step 2: Run test to verify it fails**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: FAIL with `ModuleNotFoundError` or `parse_perf_event_line` not defined.

**Step 3: Write minimal implementation**

Create a shared header-only helper:

```cpp
#ifndef PERF_EVENT_H
#define PERF_EVENT_H

#include <QDateTime>
#include <QString>
#include <QStringList>

struct PerfKv {
    const char *key;
    QString value;
};

inline QString buildPerfEventLine(const QString &app,
                                  const QString &event,
                                  const QList<PerfKv> &fields = {}) {
    QStringList parts;
    parts << "PERF_EVENT";
    parts << QString("ts_ms=%1").arg(QDateTime::currentMSecsSinceEpoch());
    parts << QString("app=%1").arg(app);
    parts << QString("event=%1").arg(event);
    for (const PerfKv &field : fields) {
        parts << QString("%1=%2").arg(field.key, field.value);
    }
    return parts.join(' ');
}

#endif
```

Update both qmake projects to expose `../common` via `INCLUDEPATH`.

**Step 4: Run test to verify it passes**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: PASS for the parser format test after the parser exists in Task 4.

**Step 5: Commit**

```bash
git add common/perf_event.h Camera_Project/Camera_Project.pro My_Project/My_Project.pro scripts/tests/test_analyze_linkage_perf.py
git commit -m "feat: add shared perf event format"
```

### Task 2: Instrument My_Project Board-Side Linkage Events

**Files:**
- Modify: `My_Project/mainwindow.h`
- Modify: `My_Project/mainwindow.cpp`
- Modify: `My_Project/linkage_logic.h`
- Modify: `My_Project/linkage_logic.cpp`
- Test: `My_Project/tests/test_linkage_logic.cpp`

**Step 1: Write the failing test**

Add host-side state tests for trigger-edge bookkeeping:

```cpp
static void test_motion_edge_only_arms_one_trigger() {
    LinkageState state{};
    SensorSnapshot snapshot{};

    snapshot.motionDetected = true;
    LinkageDecision d1 = evaluateLinkage(state, snapshot, 1000);
    assert(d1.startCamera);
    assert(d1.motionTriggered);

    LinkageDecision d2 = evaluateLinkage(state, snapshot, 1200);
    assert(!d2.motionTriggered);
}
```

Also add a parser-driven test that expects these event names to be present in board logs:

- `my_project_started`
- `motion_rise`
- `camera_auto_start_requested`
- `camera_process_started`
- `camera_process_start_failed`
- `camera_process_finished`
- `als_dark_trigger`
- `led_auto_set`

**Step 2: Run test to verify it fails**

Run: `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic`
Expected: FAIL because `motionTriggered` and related logic do not exist yet.

**Step 3: Write minimal implementation**

Extend linkage state/decision for edge-trigger metrics:

```cpp
struct LinkageState {
    bool ledOn = false;
    bool cameraRunning = false;
    CameraOwner cameraOwner = CameraOwner::None;
    long long autoCameraDeadlineMs = 0;
    long long manualLedFreezeUntilMs = 0;
    bool lastMotionDetected = false;
};

struct LinkageDecision {
    bool setLed = false;
    bool ledOn = false;
    bool startCamera = false;
    bool stopCamera = false;
    bool motionTriggered = false;
    bool darkTriggered = false;
    CameraOwner cameraOwner = CameraOwner::None;
};
```

Emit lightweight `qInfo().noquote()` event lines from `MainWindow`:

```cpp
logPerfEvent("my_project_started");
logPerfEvent("motion_rise", {{"motion", "1"}});
logPerfEvent("camera_auto_start_requested", {{"session_id", QString::number(cameraSessionId_)}});
logPerfEvent("camera_process_started", {{"session_id", QString::number(cameraSessionId_)}});
logPerfEvent("camera_process_finished", {{"session_id", QString::number(lastCameraSessionId_)}});
logPerfEvent("als_dark_trigger", {{"als", QString::number(sensorSnapshot_.als)}});
logPerfEvent("led_auto_set", {{"led", on ? "1" : "0"}});
```

Add `cameraSessionId_` and `pendingMotionTriggerTsMs_` fields in `MainWindow` so each auto-open session can be matched to the later camera first-frame event.

**Step 4: Run test to verify it passes**

Run: `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`
Expected: PASS with exit code `0`.

**Step 5: Commit**

```bash
git add My_Project/mainwindow.h My_Project/mainwindow.cpp My_Project/linkage_logic.h My_Project/linkage_logic.cpp My_Project/tests/test_linkage_logic.cpp
git commit -m "feat: add my project perf events"
```

### Task 3: Instrument Camera_Project FPS And First-Frame Events

**Files:**
- Modify: `Camera_Project/mainwindow.h`
- Modify: `Camera_Project/mainwindow.cpp`
- Test: `scripts/tests/test_analyze_linkage_perf.py`

**Step 1: Write the failing test**

Add analyzer expectations for these camera-side events:

- `camera_project_started`
- `camera_first_frame_displayed`
- `camera_fps_window`
- `camera_capture_clicked`
- `camera_exit_clicked`

Test sample:

```python
def test_extract_preview_fps_from_camera_fps_window():
    events = [
        {"event": "camera_fps_window", "fps": "31.5"},
        {"event": "camera_fps_window", "fps": "33.0"},
    ]
    summary = summarize_events(events)
    assert summary["stable_preview_fps"] == 32.25
```

**Step 2: Run test to verify it fails**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: FAIL because the analyzer does not yet summarize FPS windows.

**Step 3: Write minimal implementation**

Emit camera-side events with shared formatter:

```cpp
logPerfEvent("camera_project_started", {
    {"session_id", qEnvironmentVariable("PERF_SESSION_ID")}
});
```

On the first successfully displayed frame:

```cpp
if (!firstFrameLogged_) {
    firstFrameLogged_ = true;
    logPerfEvent("camera_first_frame_displayed", {
        {"session_id", sessionId_},
        {"width", QString::number(camera->getWidth())},
        {"height", QString::number(camera->getHeight())}
    });
}
```

Inside the existing per-second FPS update:

```cpp
logPerfEvent("camera_fps_window", {
    {"session_id", sessionId_},
    {"fps", QString::number(fps, 'f', 2)},
    {"io_ms", QString::number(avg_io, 'f', 2)},
    {"process_ms", QString::number(avg_process, 'f', 2)},
    {"display_ms", QString::number(avg_display, 'f', 2)}
});
```

Emit button events in `captureImage()` and `onExitButtonClicked()`.

**Step 4: Run test to verify it passes**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: PASS for camera event summarization tests.

**Step 5: Commit**

```bash
git add Camera_Project/mainwindow.h Camera_Project/mainwindow.cpp scripts/tests/test_analyze_linkage_perf.py
git commit -m "feat: add camera perf events"
```

### Task 4: Forward Child Camera PERF_EVENT Lines Through My_Project

**Files:**
- Modify: `My_Project/mainwindow.h`
- Modify: `My_Project/mainwindow.cpp`
- Test: `scripts/tests/test_analyze_linkage_perf.py`

**Step 1: Write the failing test**

Add analyzer tests that use one mixed log stream and still match parent-child events by `session_id`:

```python
def test_motion_to_first_frame_latency_uses_same_session():
    events = [
        {"ts_ms": 1000, "event": "motion_rise", "session_id": "3"},
        {"ts_ms": 1200, "event": "camera_process_started", "session_id": "3"},
        {"ts_ms": 1650, "event": "camera_first_frame_displayed", "session_id": "3"},
    ]
    summary = summarize_events(events)
    assert summary["motion_to_first_frame_ms"] == 650
```

**Step 2: Run test to verify it fails**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: FAIL because the analyzer cannot yet correlate sessions.

**Step 3: Write minimal implementation**

When `My_Project` starts `Camera_Project`, inject session metadata:

```cpp
QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
env.insert("PERF_SESSION_ID", QString::number(cameraSessionId_));
cameraProcess->setProcessEnvironment(env);
```

Also connect child stdout:

```cpp
connect(cameraProcess, &QProcess::readyReadStandardOutput, this, [this]() {
    QByteArray data = cameraProcess->readAllStandardOutput();
    QTextStream out(stdout);
    out << data;
    out.flush();
});
```

This keeps the board side single-stream so the Mac collector only needs one SSH log.

**Step 4: Run test to verify it passes**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: PASS for cross-process latency correlation tests.

**Step 5: Commit**

```bash
git add My_Project/mainwindow.h My_Project/mainwindow.cpp scripts/tests/test_analyze_linkage_perf.py
git commit -m "feat: forward camera perf events through my project"
```

### Task 5: Build Mac-Side Collector And Analyzer

**Files:**
- Create: `scripts/collect_linkage_perf.py`
- Create: `scripts/analyze_linkage_perf.py`
- Create: `scripts/tests/test_analyze_linkage_perf.py`
- Create: `scripts/perf_runs/.gitignore`

**Step 1: Write the failing test**

Create analyzer tests for all required output fields:

```python
def test_summary_contains_template_fields():
    summary = summarize_events(sample_events())
    assert "stable_preview_fps" in summary
    assert "motion_to_first_frame_ms" in summary
    assert "dark_to_led_on_ms" in summary
    assert "continuous_run_hours" in summary
    assert "linkage_success_rate_percent" in summary
```

Add a rendering test:

```python
def test_render_summary_sentence():
    summary = {
        "stable_preview_fps": 33.0,
        "motion_to_first_frame_ms": 620,
        "dark_to_led_on_ms": 180,
        "continuous_run_hours": 1.5,
        "linkage_success_rate_percent": 100.0,
    }
    sentence = render_summary_sentence(summary)
    assert "33.0" in sentence
    assert "620 ms" in sentence
    assert "100.0\\%" in sentence
```

**Step 2: Run test to verify it fails**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: FAIL with `ModuleNotFoundError`.

**Step 3: Write minimal implementation**

Collector responsibilities:

```python
# scripts/collect_linkage_perf.py
# 1. ssh to board
# 2. run bash -l -c 'cd ... && ./preload_drivers.sh'
# 3. tee stdout/stderr into scripts/perf_runs/<run_id>/raw/board.log
# 4. stop on Ctrl+C
# 5. invoke analyze_linkage_perf.py
```

Analyzer responsibilities:

```python
# scripts/analyze_linkage_perf.py
# 1. parse PERF_EVENT lines
# 2. match motion_rise -> camera_first_frame_displayed by session_id
# 3. match als_dark_trigger -> led_auto_set(led=1)
# 4. compute average stable_preview_fps from camera_fps_window after first frame
# 5. compute uptime from first to last PERF_EVENT
# 6. compute success rate = successful auto camera starts / motion_rise count
# 7. write summary.json and summary.txt
```

Output layout:

```text
scripts/perf_runs/<run_id>/
  raw/board.log
  processed/events.jsonl
  summary.json
  summary.txt
```

**Step 4: Run test to verify it passes**

Run: `python3 -m unittest scripts/tests/test_analyze_linkage_perf.py -v`
Expected: PASS.

**Step 5: Commit**

```bash
git add scripts/collect_linkage_perf.py scripts/analyze_linkage_perf.py scripts/tests/test_analyze_linkage_perf.py scripts/perf_runs/.gitignore
git commit -m "feat: add linkage perf collector and analyzer"
```

### Task 6: Document How To Collect And Fill The Performance Sentence

**Files:**
- Modify: `README.md`
- Modify: `scripts/README.md`
- Modify: `项目更新日志.md`

**Step 1: Write the failing doc check**

Add a grep-based acceptance check:

```bash
rg -n "collect_linkage_perf.py|analyze_linkage_perf.py|summary.txt|stable_preview_fps|motion_to_first_frame_ms" README.md scripts/README.md 项目更新日志.md
```

Expected: FAIL before the docs are added.

**Step 2: Run check to verify it fails**

Run the command above.
Expected: no matches for the new perf workflow.

**Step 3: Write minimal documentation**

Document one exact workflow:

```bash
python3 scripts/collect_linkage_perf.py \
  --board root@10.20.20.36 \
  --board-dir /lib/modules/4.1.15-g3dc0a4b \
  --run-seconds 180
```

And state that `summary.txt` is used to fill:

```text
实现 \textbf{33.0 FPS} 视频预览，人体触发到摄像头稳定输出画面耗时 \textbf{620 ms}，
暗光联动补光响应时间 \textbf{180 ms}；系统连续运行 \textbf{1.5 h} 保持稳定，
联动触发成功率 \textbf{100.0\%}。
```

The docs must explicitly distinguish:

- “脚本可统计出来的字段”
- “尚未跑实测时不可手填的字段”

**Step 4: Run check to verify it passes**

Run: `rg -n "collect_linkage_perf.py|analyze_linkage_perf.py|summary.txt|stable_preview_fps|motion_to_first_frame_ms" README.md scripts/README.md 项目更新日志.md`
Expected: PASS with matches in all three files.

**Step 5: Commit**

```bash
git add README.md scripts/README.md 项目更新日志.md
git commit -m "docs: add linkage perf collection workflow"
```

