# 联动状态机回收重构实施计划

> **For Codex:** Use `plan-execution-with-trace` before and during execution.

**目标：** 将第一阶段联动判断从 `MainWindow` 中抽离回 `linkage_logic`，保持行为不变。

**实现思路：** 保留当前稳定的 `SR501` 异步通知、`AP3216C` 定时器和 `MainWindow` 动作执行结构，只把“该不该开/关相机、该不该开/关灯”的判断交给现有 `linkage_logic`。通过主机侧测试、远端 qmake 构建和板端回归确认重构不改变效果。

**技术栈：** Qt Widgets, QProcess, QSocketNotifier, QTimer, 纯 C++ 状态机模块, qmake/make

---

## Task 1: 建立本轮重构文档基线

**文件：**
- 新建：`docs/plans/2026-03-25-linkage-state-machine-refactor-design.md`
- 新建：`docs/plans/2026-03-25-linkage-state-machine-refactor.md`
- 新建：`docs/plans/2026-03-25-linkage-state-machine-refactor-execution-log.md`
- 修改：`联动逻辑.md`
- 验证：`git diff --check -- 联动逻辑.md docs/plans/2026-03-25-linkage-state-machine-refactor-design.md docs/plans/2026-03-25-linkage-state-machine-refactor.md docs/plans/2026-03-25-linkage-state-machine-refactor-execution-log.md`

**步骤：**
1. 记录本轮“行为不变的状态机回收重构”目标
2. 明确当前行为基线和不做事项
3. 建立执行日志入口

## Task 2: 把 MainWindow 联动判断切回 linkage_logic

**文件：**
- 修改：`My_Project/mainwindow.h`
- 修改：`My_Project/mainwindow.cpp`
- 修改：`My_Project/My_Project.pro`
- 验证：`g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`

**步骤：**
1. 在 `MainWindow` 中引入 `linkage_logic.h`
2. 用 `LinkageState + SensorSnapshot` 替代当前手写的第一阶段状态变量
3. 把当前联动判断改为 `evaluateLinkage()`
4. 保持动作执行函数仍由 `MainWindow` 调用

## Task 3: 构建、部署和行为回归

**文件：**
- 修改：必要时更新 `项目更新日志.md`、`联动逻辑.md`
- 验证：`bash scripts/remote_qt_build.sh My_Project`
- 验证：板端重新启动后的第一阶段联动行为与重构前一致

**步骤：**
1. 完成远端 qmake 构建
2. 部署并重启板端 `My_Project`
3. 回归验证 `SR501 -> Camera_Project` 与 `AP3216C -> LED`
