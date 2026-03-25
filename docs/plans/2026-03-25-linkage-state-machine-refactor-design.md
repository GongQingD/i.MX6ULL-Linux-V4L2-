# 联动状态机回收重构设计文档

**日期：** 2026-03-25
**状态：** 已确认

---

## 目标

在不改变当前板端已验证行为的前提下，把第一阶段联动判断从 `My_Project/mainwindow.cpp` 中抽离回独立状态机逻辑，降低 `MainWindow` 的耦合度。

## 范围

- 修改 `My_Project/mainwindow.h`
- 修改 `My_Project/mainwindow.cpp`
- 修改 `My_Project/My_Project.pro`
- 复用现有 `My_Project/linkage_logic.h/.cpp`
- 补充本轮重构的设计、计划和执行日志

## 约束

- 不改变当前板端已通过的第一阶段行为：
  - `SR501` 自动打开 `Camera_Project`
  - 无人约 `5 秒` 后自动关闭
  - `AP3216C` 遮挡亮灯、移开灭灯
- 不引入 `SensorWorker/QThread`
- 不恢复复杂的自动/手动会话接管
- 执行动作仍留在 `MainWindow`，只抽离“判断逻辑”

## 预期架构

- `MainWindow`
  - 负责采样输入：
    - `SR501` 异步状态
    - `AP3216C` 当前 `ALS`
  - 负责执行动作：
    - 启停 `Camera_Project`
    - 写 `/dev/led`
    - 刷新 UI
- `linkage_logic`
  - 负责联动状态机判断：
    - 是否该启动相机
    - 是否该关闭相机
    - 是否该开灯
    - 是否该关灯
- `MainWindow` 只把当前传感器快照交给 `evaluateLinkage()`，再根据返回的 `LinkageDecision` 执行动作

## 风险

- `MainWindow` 当前手动按钮逻辑与状态机内部状态需要保持同步，否则会出现“状态机认为开着，实际进程已结束”之类的问题
- 如果 `My_Project.pro` 接入 `linkage_logic.cpp` 后有构建问题，会影响当前稳定部署链
- 若重构时把人工同步状态遗漏，可能导致行为偏离刚通过的现场验收结果

## 成功标准

- `linkage_logic.cpp` 正式进入 `My_Project` 构建链
- `MainWindow` 内不再自己维护第一阶段联动判断规则
- 远端 qmake 构建通过
- 板端重新部署后，第一阶段联动行为与重构前一致
