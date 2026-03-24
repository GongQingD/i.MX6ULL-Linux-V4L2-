# Camera Auto-Manual Handoff Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix the SR501 auto-camera flow so automatic camera sessions return to `My_Project`, and user interaction inside `Camera_Project` upgrades the session to manual ownership.

**Architecture:** Keep the existing `SensorWorker -> LinkageDecision -> MainWindow/QProcess` structure. Add a small pure-C++ control helper layer for camera session finish and child-process event parsing, then let `Camera_Project` emit explicit user-interaction markers to its parent process so `My_Project` can cancel auto-stop by promoting the session to manual.

**Tech Stack:** Qt Widgets, QProcess, Linux framebuffer (`linuxfb`), plain C++11 unit tests built with `g++`.

---

### Task 1: Add failing pure-logic tests for session handoff

**Files:**
- Modify: `My_Project/tests/test_linkage_logic.cpp`
- Test: `My_Project/tests/test_linkage_logic.cpp`

**Step 1: Write the failing tests**

- Add a test that starts an auto camera session, records a user interaction, and verifies the later timeout no longer auto-stops the camera.
- Add a test that verifies finished camera sessions should restore `My_Project` when the camera window had actually been shown and there is no restart/shutdown in progress.
- Add a test that verifies only explicit child-process marker lines are treated as user interaction.

**Step 2: Run test to verify it fails**

Run:

```bash
g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic
```

Expected: assertion failure because the new helpers do not exist yet.

### Task 2: Implement minimal pure helpers

**Files:**
- Modify: `My_Project/linkage_logic.h`
- Modify: `My_Project/linkage_logic.cpp`
- Test: `My_Project/tests/test_linkage_logic.cpp`

**Step 1: Write minimal implementation**

- Add a helper that promotes the camera session to manual ownership after child-side user interaction.
- Add a helper that decides whether `My_Project` should restore its own window after a child camera process finishes.
- Add a helper that parses a child-process output line into a camera runtime event.

**Step 2: Run test to verify it passes**

Run:

```bash
g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic
```

Expected: PASS.

### Task 3: Wire child-process events into `My_Project`

**Files:**
- Modify: `My_Project/mainwindow.h`
- Modify: `My_Project/mainwindow.cpp`

**Step 1: Add process-output handling**

- Read `Camera_Project` stdout/stderr line-by-line.
- Detect the explicit user-interaction marker.
- When an auto session receives that marker, promote the active/pending owner to manual and sync the worker state.

**Step 2: Fix finish behavior**

- Use the new pure helper in `handleCameraFinished()` so auto-finished sessions also restore `My_Project`.
- Keep restart/shutdown behavior unchanged.

**Step 3: Build verification**

Run:

```bash
./scripts/remote_qt_build.sh My_Project
```

Expected: remote Qt build succeeds.

### Task 4: Emit interaction markers from `Camera_Project`

**Files:**
- Modify: `Camera_Project/mainwindow.cpp`

**Step 1: Emit minimal markers**

- Print an explicit marker when the user clicks `拍照`.
- Print an explicit marker when the user clicks `退出`.

**Step 2: Build verification**

Run:

```bash
./scripts/remote_qt_build.sh Camera_Project
```

Expected: remote Qt build succeeds.

### Task 5: Update documentation and runtime log

**Files:**
- Modify: `README.md`
- Modify: `项目更新日志.md`

**Step 1: Document the new behavior**

- State that auto camera sessions now return to `My_Project` after auto-stop.
- State that clicking camera controls upgrades the session to manual ownership and prevents auto-stop from interrupting the user.

**Step 2: Record overwrite relationship**

- Add a new root-log chapter referencing the earlier SR501 auto-camera runtime chapters.
- Explain the overwrite reason as interaction design correction and runtime usability fix.

**Step 3: Final verification**

Run:

```bash
git diff --check -- My_Project/linkage_logic.h My_Project/linkage_logic.cpp My_Project/tests/test_linkage_logic.cpp My_Project/mainwindow.h My_Project/mainwindow.cpp Camera_Project/mainwindow.cpp README.md 项目更新日志.md
```

Expected: no diff-format issues.
