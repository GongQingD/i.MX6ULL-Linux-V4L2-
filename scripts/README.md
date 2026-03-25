# scripts 目录说明

这个目录目前用于承载 Mac 端发起的远端构建脚本。当前可用脚本是：

- `remote_qt_build.sh`：把 Mac 上的 Qt 工程同步到 Ubuntu builder 交叉编译，并按需部署到开发板。
- `collect_linkage_perf.py`：从开发板抓取一轮 `PERF_EVENT` 运行日志，并自动调用分析脚本。
- `analyze_linkage_perf.py`：离线解析 `PERF_EVENT` 日志，输出 `events.jsonl`、`summary.json` 和 `summary.txt`。

## 0. 联动性能统计脚本

如果你当前要补“预览帧率 / 人体触发时延 / 暗光补光时延 / 连续运行时长 / 联动成功率”，优先走下面这条命令：

```bash
python3 scripts/collect_linkage_perf.py \
  --board root@10.20.20.36 \
  --board-dir /lib/modules/4.1.15-g3dc0a4b \
  --run-seconds 180
```

执行后会在 `scripts/perf_runs/<run_id>/` 下生成：

- `raw/board.log`
- `events.jsonl`
- `summary.json`
- `summary.txt`

其中 `summary.txt` 是可直接粘贴到文档或简历草稿里的成句结果。

如果你已经有一份现成板端日志，也可以离线重跑：

```bash
python3 scripts/collect_linkage_perf.py \
  --log-file /path/to/board.log \
  --output-dir scripts/perf_runs/manual-replay
```

需要注意：

- 这套链路只负责“采集并统计”，不会替你判断这轮测试场景是否合格
- 没有真实板端运行数据时，`summary.txt` 只能视为工具输出格式验证，不能当成最终性能结论
- `continuous_run_hours` 在实时采集模式下按本轮抓取窗口长度统计，不要求中途持续有业务事件输出

## 1. 三台机器的角色

当前开发链路分成三端：

| 机器 | 当前连接方式 | 角色 | 是否保存最终源码 |
| --- | --- | --- | --- |
| Mac | 本地工作区 `/Users/apple/Embedded/projects/i.MX6ULL-Linux-V4L2-` | 日常改代码、提交文档、发起构建 | 是 |
| Ubuntu VM / builder | `ssh ladykaka@10.20.41.20` | 远端交叉编译机，负责 `qmake + make` | 否 |
| i.MX6ULL 开发板 | `ssh root@10.20.20.36` | 运行和验证目标程序 | 否 |

一句话关系：

> Mac 是源码主站，Ubuntu VM 是一次性构建镜像，开发板是部署和运行目标。

## 2. 为什么要这样分工

当前 Qt 工程并不是在 Mac 本机直接编译，而是借助 Ubuntu VM 上已经可用的 NXP 交叉编译环境来完成构建。这样做的原因是：

- 你之前实际可用的编译方式是在 Ubuntu 虚拟机里通过 Qt Creator / 交叉环境构建。
- 当前已经验证 builder 端可以在加载 NXP SDK 环境后直接执行 `qmake` 和 `make`。
- 开发板只负责运行 ARM 可执行文件，不适合作为主要开发和构建环境。

当前脚本中的默认配置是：

- builder：`ladykaka@10.20.41.20`
- board：`root@10.20.20.36`
- builder 同步目录：`/home/ladykaka/qt_build_from_mac`
- SDK 环境脚本：`/opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi`
- 板端部署目录：`/lib/modules/4.1.15-g3dc0a4b`

## 3. 数据流向

标准数据流如下：

```text
Mac 本地修改源码
  -> 运行 scripts/remote_qt_build.sh
  -> rsync 到 Ubuntu VM builder
  -> builder 加载交叉编译环境
  -> qmake + make 生成 ARM 可执行文件
  -> scp 到开发板
  -> 在开发板上手动运行验证
```

对应到当前脚本，实际顺序是：

1. 在 Mac 上选择 `Camera_Project`、`My_Project` 或 `all`
2. 用 `rsync --delete` 把工程目录同步到 builder
3. 在 builder 上进入对应目录，重新生成 `Makefile`
4. 在 builder 上执行交叉编译
5. 如果没有加 `--skip-deploy`，把产物复制到开发板

## 4. 源码归属规则

这是当前工作流里最容易出错的地方：

- Mac 工作区是唯一默认真源
- builder 上的目录只是同步出来的构建镜像
- 每次执行脚本都会使用 `rsync --delete`
- 所以下一次同步时，builder 上手工改过的文件会被 Mac 当前内容覆盖
- 板子上的可执行文件和 `.ko` 也不会自动回传到 Mac

因此建议遵守下面的规则：

- 改代码只在 Mac 上改
- builder 只做编译，不长期手改源码
- 开发板只做运行、日志查看和硬件联调

## 5. 当前推荐开发流程

### 5.1 修改 Qt 工程

在 Mac 上直接修改：

- `My_Project/`
- `Camera_Project/`
- `README.md`
- `BOARD.md`

### 5.2 触发远端编译

常用命令：

```bash
# 只编译 My_Project，不部署
./scripts/remote_qt_build.sh My_Project --skip-deploy

# 编译并部署 My_Project
./scripts/remote_qt_build.sh My_Project

# 编译并部署 Camera_Project
./scripts/remote_qt_build.sh Camera_Project

# 两个工程都重新编译并部署
./scripts/remote_qt_build.sh all
```

如果默认地址变化，也可以临时覆盖：

```bash
./scripts/remote_qt_build.sh My_Project \
  --builder ladykaka@10.20.41.20 \
  --board root@10.20.20.36 \
  --builder-base /home/ladykaka/qt_build_from_mac \
  --deploy-dir /lib/modules/4.1.15-g3dc0a4b
```

### 5.3 到板端验证

部署后，默认推荐直接复用仓库里的预加载脚本，并通过 login shell 远程触发，而不是手写一串 `insmod + nohup`：

```bash
cp /path/to/repo/scripts/preload_drivers.sh ./preload_drivers.sh
chmod +x ./preload_drivers.sh
ssh root@10.20.20.36 "bash -l -c 'cd /lib/modules/4.1.15-g3dc0a4b && ./preload_drivers.sh'"
```

如果要单独跑相机程序：

```bash
ssh root@10.20.20.36
cd /lib/modules/4.1.15-g3dc0a4b
QT_QPA_PLATFORM=linuxfb ./Camera_Project
```

## 6. 为什么默认改成 `preload_drivers.sh`

实际板端排查表明，远程触发 `My_Project` 的稳定路径应该默认满足三个条件：

- 仍然走板端现有的 `preload_drivers.sh`
- 通过 `bash -l -c` 进入 login shell，再 `cd /lib/modules/4.1.15-g3dc0a4b && ./preload_drivers.sh`
- 不再默认使用后台 `nohup` 或直接 `ssh 'cd ... && ./preload_drivers.sh'`

用户板端实测显示：同一个 `My_Project` 二进制，在“板端本地前台执行 `./preload_drivers.sh`”路径下，`SR501` 检测和按钮点击都正常；远程场景下，`ssh root@10.20.20.36 "bash -l -c 'cd /lib/modules/4.1.15-g3dc0a4b && ./preload_drivers.sh'"` 更接近这条手工基线，因此现在应视为默认远程运行方案。

## 7. 为什么板端运行要带 `QT_QPA_PLATFORM=linuxfb`

当前是从 SSH 会话里直接启动 Qt 程序，不是从板端桌面环境里点开。此时如果不显式指定平台插件，Qt 可能默认走 `xcb`，导致启动失败。

当前在板端经实际验证可用的方式是：

```bash
QT_QPA_PLATFORM=linuxfb ./My_Project
QT_QPA_PLATFORM=linuxfb ./Camera_Project
```

## 8. 常见误区

- 误区 1：在 Ubuntu VM 上改源码，再期待自动同步回 Mac
  说明：不会自动同步，下一次 `rsync --delete` 还可能把这些改动冲掉。

- 误区 2：把开发板当成编译机
  说明：当前流程里开发板只负责部署和运行验证，不承担主编译职责。

- 误区 3：认为 Mac 上的 Qt Creator 可以直接替代 builder
  说明：当前项目实际可用的交叉编译环境在 Ubuntu VM 上，不在 Mac 本机。

- 误区 4：只要 `./My_Project` 能跑，启动方式就无所谓
  说明：当前板端更稳的路径是“预加载驱动 + 前台 `linuxfb` 启动”。同一个二进制，启动环境不同，触摸和传感器行为可能不同。

## 9. 你现在应该怎么用

如果只是日常改 `My_Project` 或 `Camera_Project`，按这个顺序即可：

1. 在 Mac 上改代码
2. 在仓库根目录执行 `./scripts/remote_qt_build.sh My_Project` 或 `./scripts/remote_qt_build.sh Camera_Project`
3. SSH 登录开发板
4. 执行 `ssh root@10.20.20.36 "bash -l -c 'cd /lib/modules/4.1.15-g3dc0a4b && ./preload_drivers.sh'"`
5. 观察主界面、触摸和传感器行为
6. 看屏幕效果和串口/SSH 输出，继续回到 Mac 修改

这就是当前这套三端开发流程的最小闭环。
