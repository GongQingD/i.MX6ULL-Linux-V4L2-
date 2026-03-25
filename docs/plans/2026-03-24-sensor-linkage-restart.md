# 多传感器联动重启实施计划

> **For Codex:** Use `plan-execution-with-trace` before and during execution.

**目标：** 基于当前稳定的 `My_Project` 基线，重新打开第一阶段多传感器自动动作，并完成构建、部署与记录。

**实现思路：** 保留现有 `SR501` 异步通知和 `AP3216C/DHT11` 定时器读取结构，不恢复历史上的 `SensorWorker/QThread` 整包方案。先在 `MainWindow` 内以最小状态机恢复 `SR501 -> Camera_Project` 与 `AP3216C -> LED` 自动动作，待板端稳定后再考虑第二阶段抽象。

**技术栈：** Qt Widgets, QTimer, QProcess, QSocketNotifier, Linux 字符设备(`/dev/sr501`, `/dev/ap3216c`, `/dev/dht11`, `/dev/led`), qmake/make, 远端 Ubuntu 交叉编译机

---

## Task 1: 固化本轮设计与执行痕迹

**文件：**
- 新建：`docs/plans/2026-03-24-sensor-linkage-restart-design.md`
- 新建：`docs/plans/2026-03-24-sensor-linkage-restart.md`
- 新建：`docs/plans/2026-03-24-sensor-linkage-restart-execution-log.md`
- 修改：`联动逻辑.md`
- 验证：`git diff --check -- 联动逻辑.md docs/plans/2026-03-24-sensor-linkage-restart-design.md docs/plans/2026-03-24-sensor-linkage-restart.md docs/plans/2026-03-24-sensor-linkage-restart-execution-log.md`

**步骤：**
1. 将当前阶段的联动边界、设计和执行日志文件落盘
2. 在执行日志中记录“从旧方案回退后重新启动联动”的起点
3. 运行 `git diff --check` 确认文档格式无问题

## Task 2: 在 MainWindow 中恢复第一阶段自动动作

**文件：**
- 修改：`My_Project/mainwindow.h`
- 修改：`My_Project/mainwindow.cpp`
- 验证：`git diff --check -- My_Project/mainwindow.h My_Project/mainwindow.cpp`

**步骤：**
1. 增加第一阶段所需的最小联动状态变量和小型 helper
2. 在 `SR501` 异步通知处理链中接入“启动或续期自动相机会话”
3. 在现有定时器链中接入：
   - 自动相机会话超时关闭
   - `AP3216C -> LED` 自动控灯
4. 保持当前手动按钮和主界面显示逻辑可用

## Task 3: 完成构建、部署与基础回归

**文件：**
- 修改：若需要，同步调整 `README.md`、`项目更新日志.md`、`联动逻辑.md`
- 验证：`bash scripts/remote_qt_build.sh My_Project`
- 验证：板端 `/lib/modules/4.1.15-g3dc0a4b/My_Project` 已替换且进程重新启动

**步骤：**
1. 运行远端 qmake/make 构建 `My_Project`
2. 若部署被运行中的进程阻塞，则先停进程再替换二进制
3. 通过板端进程、启动日志和文件信息确认新版本已生效

## Task 4: 更新中文记录并收口提交状态

**文件：**
- 修改：`项目更新日志.md`
- 修改：`联动逻辑.md`
- 验证：`git status --short`

**步骤：**
1. 在根目录中文记录中补记本轮“联动重启”的章节
2. 明确本轮已实现与未实现边界，避免把第二阶段内容写成已完成
3. 汇总当前未提交文件和验证结果，作为下一步提交基线
