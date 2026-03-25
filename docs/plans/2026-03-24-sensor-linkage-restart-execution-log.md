# 多传感器联动重启执行日志

**日期：** 2026-03-24
**状态：** 已完成
**关联设计：** `docs/plans/2026-03-24-sensor-linkage-restart-design.md`
**关联计划：** `docs/plans/2026-03-24-sensor-linkage-restart.md`

---

## 步骤 1 - 重新建立本轮设计与计划基线

- 状态：已完成
- 预期：在旧方案回退后的当前仓库基线上，重新建立本轮联动恢复的设计、计划和执行日志入口
- 实际动作：
  - 回读历史计划 `docs/plans/2026-03-22-sensor-linkage.md`
  - 回读根目录 `项目更新日志.md`
  - 回读当前 `My_Project` 代码，确认当前稳定基线是“主界面 + 手动摄像头 + SR501 异步状态显示”
  - 新建本轮设计文档、实施计划和执行日志
- 证据：
  - `docs/plans/2026-03-24-sensor-linkage-restart-design.md`
  - `docs/plans/2026-03-24-sensor-linkage-restart.md`
  - `docs/plans/2026-03-24-sensor-linkage-restart-execution-log.md`
  - `联动逻辑.md`
- 冲突：
  - 历史日志中存在“联动曾经部分落地”的记录，但当前 `My_Project.pro` 并未编入 `linkage_logic/sensor_worker`
- 决策：
  - 本轮不直接恢复历史整包方案
  - 先采用“SR501 异步 + AP3216C 定时器 + MainWindow 最小状态机”的第一阶段方案
- 影响：
  - 后续代码实现只改当前稳定基线相关文件，减少排障面
- 下一步：开始在 `My_Project/mainwindow.h/.cpp` 中恢复第一阶段自动动作

## 步骤 2 - 恢复第一阶段自动动作

- 状态：已完成
- 预期：在不引入 `SensorWorker/QThread` 的前提下，重新打开 `SR501 -> Camera_Project` 与 `AP3216C -> LED`
- 实际动作：
  - 在 `My_Project/mainwindow.h/.cpp` 中新增第一阶段自动动作所需状态变量
  - 保留当前 `SR501` 异步通知链路，在收到“有人”状态时自动启动或续期相机会话
  - 在当前 `AP3216C` 定时器链中接入：
    - `AP3216C -> LED` 迟滞阈值自动控灯
    - `SR501` 自动相机会话超时关闭
  - 保留当前手动 LED 按钮和手动摄像头启动入口
- 证据：
  - `My_Project/mainwindow.h`
  - `My_Project/mainwindow.cpp`
  - `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`
- 冲突：
  - 计划阶段曾讨论“直接复用 `linkage_logic` 接回当前基线”，但当前实际实现采用了 `MainWindow` 内最小状态机
- 决策：
  - 保持与本轮设计一致，优先以当前稳定基线上的最小状态机恢复自动动作
  - `linkage_logic` 本轮先保留为规则参考和主机侧验证资产，不强行接入运行时
- 影响：
  - 当前运行时改动面集中在 `MainWindow`，便于板端继续排障
- 下一步：完成远端构建、部署和板端回读

## 步骤 3 - 远端构建、部署与板端回读

- 状态：已完成
- 预期：完成 `My_Project` 的远端 qmake 构建、部署与板端重启
- 实际动作：
  - 运行 `bash scripts/remote_qt_build.sh My_Project`
  - 远端 Ubuntu 构建机完成 `qmake + make`
  - 脚本进入部署阶段后，连接板端 `root@10.20.20.36:22` 超时
  - 在最新工作区上追加运行 `bash scripts/remote_qt_build.sh My_Project --skip-deploy`，确认当前代码仍可编译
  - 板端恢复在线后，重新执行 `bash scripts/remote_qt_build.sh My_Project`，成功把新二进制部署到 `/lib/modules/4.1.15-g3dc0a4b/My_Project`
  - 通过 `setsid bash -l -c "./preload_drivers.sh"` 重新启动板端 `My_Project`
  - 回读进程、字符设备节点和启动日志，确认新版本已处于运行态
- 证据：
  - `bash scripts/remote_qt_build.sh My_Project`
  - `bash scripts/remote_qt_build.sh My_Project --skip-deploy`
  - `ssh -o ConnectTimeout=5 root@10.20.20.36 "echo ok"` 当前已恢复可连通
  - 板端 `/lib/modules/4.1.15-g3dc0a4b/My_Project` 大小为 `68640`
  - `ps | grep My_Project` 可见进程
  - `/dev/led /dev/ap3216c /dev/dht11 /dev/sr501` 已重新出现
  - 启动日志中已出现 `auto LED state changed to: 0`
- 冲突：
  - 部署阶段曾被板端离线阻塞，但恢复在线后已解除
- 决策：
  - 部署与重启已完成，下一步进入现场联动验收
- 影响：
  - 当前已确认“代码接线 + 远端构建 + 板端部署 + 进程运行”链路成立
  - 剩余工作收敛为“现场触发联动动作是否符合预期”
- 下一步：执行 `SR501 -> Camera_Project` 与 `AP3216C -> LED` 现场联动测试

## 步骤 4 - 第一阶段现场联动验收

- 状态：已完成
- 预期：在板端真实环境下确认第一阶段两条自动动作都能按预期触发
- 实际动作：
  - 用户在板前触发 `SR501`
  - 观察 `Camera_Project` 是否自动打开，以及无人后是否约 `5 秒` 自动关闭
  - 用户遮挡并移开 `AP3216C`
  - 观察 LED 是否随暗/亮变化自动切换
- 证据：
  - 用户现场反馈：
    - `SR501`: 自动打开，`5 秒` 后关闭
    - `AP3216C`: 遮挡亮灯，移开灭灯
- 冲突：
  - 无
- 决策：
  - 认定第一阶段最小可用联动方案已在板端通过现场验收
- 影响：
  - 当前可以把本轮状态更新为“第一阶段已完成”
  - 后续是否进入第二阶段，应单独决定，不再把旧线程方案默认算作正在推进
- 下一步：更新根目录中文记录，并整理提交基线

## 遇到的阻塞

- `10.20.20.36:22` 曾经 SSH 连接超时，暂时阻塞部署；当前已解除。

## 计划变更

- 当前没有修改实施范围，但部署阶段因板端离线转入阻塞状态。

## 最终实现链路

- 从当前稳定 git 基线出发，保留 `SR501` 异步通知和 `AP3216C/DHT11` 定时器读取结构。
- 在 `My_Project/mainwindow.h/.cpp` 中加入第一阶段最小状态机，重新打开：
  - `SR501 -> Camera_Project`
  - `AP3216C -> LED`
- 先完成主机侧规则验证与远端 Ubuntu `qmake + make` 构建。
- 板端恢复在线后，重新部署 `My_Project`，通过 `preload_drivers.sh` 重启程序。
- 最终由用户在板前完成现场触发，确认两条自动动作均已生效。

## 剩余问题

- 当前尚未决定是否进入第二阶段：
  - 是否把联动判断重新抽回纯逻辑模块
  - 是否恢复 `SensorWorker/QThread`
  - 是否重新引入复杂的自动/手动会话接管
