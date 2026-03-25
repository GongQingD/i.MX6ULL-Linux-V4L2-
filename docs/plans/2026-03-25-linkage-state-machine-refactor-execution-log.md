# 联动状态机回收重构执行日志

**日期：** 2026-03-25
**状态：** 进行中
**关联设计：** `docs/plans/2026-03-25-linkage-state-machine-refactor-design.md`
**关联计划：** `docs/plans/2026-03-25-linkage-state-machine-refactor.md`

---

## 步骤 1 - 建立重构基线

- 状态：已完成
- 预期：把“第二阶段等价重构”的目标、边界和执行记录先落盘
- 实际动作：
  - 回读当前 `MainWindow` 中的第一阶段联动实现
  - 回读现有 `linkage_logic.h/.cpp` 与测试
  - 建立本轮设计文档、实施计划和执行日志
- 证据：
  - `docs/plans/2026-03-25-linkage-state-machine-refactor-design.md`
  - `docs/plans/2026-03-25-linkage-state-machine-refactor.md`
  - `docs/plans/2026-03-25-linkage-state-machine-refactor-execution-log.md`
- 冲突：
  - 当前板端已通过的第一阶段逻辑还散在 `MainWindow` 内
- 决策：
  - 本轮只做状态机回收重构，不加新功能
- 影响：
  - 后续代码改动需要以“行为不变”为最高约束
- 下一步：把 `MainWindow` 联动判断切回 `linkage_logic`

## 步骤 2 - 切回 linkage_logic

- 状态：已完成
- 预期：把当前第一阶段联动判断从 `MainWindow` 内部手写逻辑切回 `linkage_logic`
- 实际动作：
  - 在 `MainWindow` 中引入 `linkage_logic.h`
  - 用 `LinkageState + SensorSnapshot` 替代当前手写的第一阶段状态变量
  - 将 `SR501` 异步通知和 `AP3216C` 定时器采样结果统一交给 `evaluateLinkage()`
  - `MainWindow` 仅保留动作执行：
    - 启停 `Camera_Project`
    - 写 `/dev/led`
    - 刷新 UI
  - 将 `linkage_logic.cpp` 正式加入 `My_Project.pro` 构建链
- 证据：
  - `My_Project/mainwindow.h`
  - `My_Project/mainwindow.cpp`
  - `My_Project/My_Project.pro`
  - `g++ -std=c++11 -I My_Project My_Project/tests/test_linkage_logic.cpp My_Project/linkage_logic.cpp -o /tmp/test_linkage_logic && /tmp/test_linkage_logic`
  - `bash scripts/remote_qt_build.sh My_Project --skip-deploy`
- 冲突：
  - 无
- 决策：
  - 保持“判断进状态机、执行留 MainWindow”的拆分边界，不引入 `SensorWorker/QThread`
- 影响：
  - 当前第一阶段联动规则已可回收到独立状态机模块，后续继续扩展时更容易测试和维护
- 下一步：部署到板端并验证行为是否与重构前一致

## 步骤 3 - 构建、部署与回归

- 状态：已完成
- 预期：完成重构版 `My_Project` 的构建、部署和板端行为回归
- 实际动作：
  - 运行 `bash scripts/remote_qt_build.sh My_Project --skip-deploy`，确认重构版远端 qmake 构建通过
  - 运行 `bash scripts/remote_qt_build.sh My_Project`，首次部署因板端旧 `My_Project` 进程占用目标文件失败
  - 停止板端 `My_Project`，手动替换新二进制并重新执行 `preload_drivers.sh`
  - 用户再次完成现场联动测试
- 证据：
  - `bash scripts/remote_qt_build.sh My_Project --skip-deploy`
  - 板端 `/lib/modules/4.1.15-g3dc0a4b/My_Project` 新大小 `69332`
  - `ps | grep My_Project` 可见新进程
  - 用户现场反馈：
    - `SR501`: 自动打开，`5 秒` 后关闭
    - `AP3216C`: 遮挡亮灯，移开灭灯
- 冲突：
  - 部署阶段需要先停掉正在运行的旧进程，不能直接覆盖目标文件
- 决策：
  - 认定本轮“状态机回收重构”已经完成，且板端行为与重构前一致
- 影响：
  - 当前第二阶段最小重构已完成
  - 后续是否继续引入 `SensorWorker/QThread`，可作为新的独立阶段再决定
- 下一步：更新根目录中文记录，并整理提交基线

## 遇到的阻塞

- 部署阶段若板端 `My_Project` 正在运行，无法直接覆盖目标文件，需要先停进程再替换。

## 计划变更

- None yet.

## 最终实现链路

- 先回读当前已通过现场验收的第一阶段联动基线。
- 再将 `MainWindow` 中的第一阶段联动判断收回到 `linkage_logic`。
- 保持 `MainWindow` 只负责采样输入、执行动作和刷新 UI。
- 完成主机侧测试与远端 qmake 构建。
- 部署到板端后再次现场回归，确认重构前后的联动行为一致。

## 剩余问题

- 当前尚未继续推进：
  - `SensorWorker/QThread`
  - 自动/手动会话接管
  - 更复杂的手动覆盖策略
