# ATK-IMX6U 开发板当前快照

## 1. 采样说明

- 本文信息通过 `ssh root@10.20.20.36` 于 **2026-03-22 19:22:05 CST** 从开发机远程采样获得。
- 当前账号信息：
  - 用户名：`root`
  - IP：`10.20.20.36`
-  - 密码：已脱敏，不在仓库中记录
- 已实际验证可免密登录：`ssh -o BatchMode=yes root@10.20.20.36 'hostname'` 返回成功。
- 板端自己报告的时间是 **2024-12-05 23:23:13 UTC**，明显落后于当前采样时间；因此本文中的“文件修改时间”“系统时间”只可视为板端时钟值，不能当作真实当前时间。

## 2. 当前硬件情况

### 2.1 板卡与 SoC

| 项目 | 当前实测 |
| --- | --- |
| 主机名 | `ATK-IMX6U` |
| 设备树 model | `Freescale i.MX6 ULL 14x14 EVK Board` |
| compatible | `fsl,imx6ull-14x14-evk`, `fsl,imx6ull` |
| SoC/平台 | `Freescale i.MX6 Ultralite (Device Tree)` |
| CPU | `ARMv7 Processor rev 5 (v7l)` |
| CPU 特性 | `neon`, `vfpv3`, `vfpv4`, `idiva`, `idivt`, `lpae` |
| 可见内存 | `MemTotal: 506860 kB` |

说明：
- `/proc/cpuinfo` 当前只看到 `processor : 0`，即当前系统只暴露一个逻辑 CPU。
- 这份文档只记录“板上当前跑起来的实际配置”，不额外推断未直接验证到的硬件参数。

### 2.2 存储与启动介质

| 项目 | 当前实测 |
| --- | --- |
| 启动参数 | `console=ttymxc0,115200 root=/dev/mmcblk1p2 rootwait rw` |
| 根文件系统 | `/dev/mmcblk1p2` 挂载到 `/`，文件系统为 `ext3` |
| Boot 分区 | `/dev/mmcblk1p1` 挂载到 `/run/media/mmcblk1p1`，文件系统为 `vfat` |
| 板载存储标识 | `mmcblk1`，设备名 `8GTF4R` |
| 容量 | `7634944` blocks，约 8 GB eMMC |
| eMMC 生产信息 | `date: 03/2022` |

Boot 分区当前可见：
- `zImage`
- 多个 `imx6ull-14x14-emmc-*.dtb`
- 包含 `imx6ull-14x14-emmc-7-1024x600-c.dtb`

### 2.3 显示、触摸、摄像头与总线设备

| 类别 | 当前实测 |
| --- | --- |
| Framebuffer | `/dev/fb0`，名称 `mxs-lcdif` |
| 屏幕模式 | `1024x600`，`59 Hz` |
| 像素深度 | `16 bpp` |
| 视频节点 | `/dev/video0`=`PxP`，`/dev/video1`=`mx6s-csi` |
| 输入设备 | `snvs-powerkey`、`EP0790M09`、`gpio_keys@0` |
| 触摸节点 | `/dev/input/touchscreen0 -> event1` |
| I2C 设备 | `es8388`、`wm8960`、`my_ap3216c`、`edt-ft5306`、`ov5640`、`gt9xx` |
| SPI 设备 | `spi2.0`，`modalias=spi:icm20608` |

从当前内核导出的设备名看：
- 摄像头链路已经枚举出 `mx6s-csi` 和 `PxP`。
- I2C 总线上已经能看到 `ov5640` 摄像头、`my_ap3216c` 传感器，以及两种触摸相关器件 `edt-ft5306` / `gt9xx`。

## 3. 当前软件环境

### 3.1 系统基础信息

| 项目 | 当前实测 |
| --- | --- |
| 内核 | `Linux 4.1.15-g3dc0a4b` |
| 内核编译时间 | `Thu Aug 18 09:27:40 CST 2022` |
| 发行版标识 | `Freescale i.MX Release Distro 4.1.15-2.1.0` |
| BusyBox | `v1.24.1` |
| Qt 运行库 | `Qt 5.12.9` (`libQt5Core/Gui/Widgets.so.5.12.9`) |
| qmake | `/usr/bin/qmake` |

补充：
- 板上没有 `ip` 命令，网络排查应优先使用 `ifconfig` 和 `route -n`。
- `/etc/os-release` 当前为空，系统识别应以 `uname -a`、`/etc/issue` 和板上实际工具链为准。

### 3.2 网络与 SSH

| 项目 | 当前实测 |
| --- | --- |
| 活跃网口 | `eth0` |
| 当前 IPv4 | `10.20.20.36/17` |
| 广播地址 | `10.20.127.255` |
| 默认网关 | `10.20.127.254` |
| DNS 配置 | `/etc/resolv.conf` 指向 `::1` 和 `127.0.0.1` |
| SSH 服务 | `dropbear` 正在运行 |

当前还确认到：
- `/etc/dropbear/authorized_keys` 存在，共 `2` 行。
- `/home/root/.ssh/authorized_keys` 存在，共 `1` 行。
- 因此现在是“密码仍可作为兜底，但已经配置了公钥免密登录”的状态。

### 3.3 板上已部署的软件/文件

当前目录 `/lib/modules/4.1.15-g3dc0a4b/` 下可见：

| 文件 | 当前实测 |
| --- | --- |
| Qt 应用 | `Camera_Project`、`My_Project` |
| 传感器/外设驱动 | `ap3216c_drv.ko`、`dht11_drv.ko`、`led_drv.ko`、`sr501_drv.ko` |
| 摄像头相关 | `ov5640_camera_int.ko`、`v4l2_app`、`v4l2_camera`、`pxp_camera_test` |
| 辅助脚本 | `preload_drivers.sh` |
| 其他 | `webrtc-streamer/` 目录已存在 |

其中 `preload_drivers.sh` 当前内容是：
- 依次 `insmod led_drv/ap3216c_drv/dht11_drv/sr501_drv`
- 随后直接启动 `./My_Project`

说明板上已经具备“传感器驱动 + Qt 面板应用”的部署痕迹，但是否已经开机自启，仍应以当前进程和设备节点为准。

## 4. 当前运行状态

### 4.1 已确认正在生效的部分

- `dropbear` 正在运行，SSH 可免密登录。
- 摄像头链路相关模块当前已加载：
  - `mx6s_capture`
  - `ov5640_camera`
- 视频设备节点已经存在：
  - `/dev/video0`
  - `/dev/video1`
- 图片目录 `/media/figure` 已存在。

### 4.2 当前未看到或未激活的部分

本次采样时未看到以下字符设备节点：

- `/dev/led`
- `/dev/ap3216c`
- `/dev/dht11`
- `/dev/sr501`

本次采样时也未看到以下用户态进程正在运行：

- `Camera_Project`
- `My_Project`

这说明截至 **2026-03-22 19:22:05 CST** 这次采样：
- 板上文件已经部署过；
- 但传感器驱动和 Qt 应用并不处于“当前正在运行”的状态；
- 至少从这次快照看，当前活跃的是摄像头基础链路，不是完整的传感器 UI 运行态。

## 5. 复核命令

如果后续要重新核对当前状态，可以直接执行下面这些命令：

```bash
ssh root@10.20.20.36 'hostname; date; uptime'
ssh root@10.20.20.36 'uname -a; cat /etc/issue; busybox | head -n 1'
ssh root@10.20.20.36 'cat /proc/cmdline; df -h; cat /proc/partitions'
ssh root@10.20.20.36 'ifconfig -a; route -n'
ssh root@10.20.20.36 'ls -l /dev/video* /dev/fb*'
ssh root@10.20.20.36 'for f in /sys/class/video4linux/video*/name; do echo \"$(basename $(dirname \"$f\")): $(cat \"$f\")\"; done'
ssh root@10.20.20.36 'for d in /sys/bus/i2c/devices/[0-9]-*; do [ -f \"$d/name\" ] && echo \"$(basename \"$d\"): $(cat \"$d/name\")\"; done'
ssh root@10.20.20.36 'lsmod; ls -l /dev/led /dev/ap3216c /dev/dht11 /dev/sr501'
ssh root@10.20.20.36 'ps | grep -E \"[d]ropbear|[C]amera_Project|[M]y_Project\"'
```

## 6. 结论

一句话总结当前板况：

> 这块板现在已经是一套可通过 `root@10.20.20.36` 免密接入的 i.MX6ULL eMMC Linux 开发板，摄像头基础链路和 Qt 运行库都在，`Camera_Project`/`My_Project` 与多份 `.ko` 已部署到板上；但截至本次采样，传感器字符设备节点和 Qt 应用进程并没有处于运行态，板端系统时钟也明显未校准。
