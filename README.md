# ATK-IMX6U Qt 应用程序套件

本项目包含两个针对 ATK-IMX6U 开发板的 Qt 应用程序，分别实现摄像头功能和传感器控制面板。

## 项目概览

| 项目 | 描述 |
|------|------|
| [Camera_Project](Camera_Project/) | 基于 V4L2 的摄像头应用，支持实时预览、拍照保存和图片浏览 |
| [My_Project](My_Project/) | 传感器控制面板，集成 LED 控制、AP3216C 光感/距离传感器、DHT11 温湿度传感器，并可启动摄像头应用 |
| [driver_file](driver_file/) | Linux 驱动模块源码，包括 AP3216C、DHT11、SR501 等 |含所需的 Linux 驱动模块源码，用于支持各硬件设备 |

## 硬件环境

- **开发板**: ATK-IMX6U (正点原子 i.MX6U 系列)
- **处理器**: ARM Cortex-A7 (imx6ull)
- **内核版本**: 4.1.15-g3dc0a4b
- **显示屏**: 1024×600 LCD，RGB565 格式
- **摄像头**: OV5640 (5MP CMOS)，MIPI CSI2 接口
- **传感器**:
  - AP3216C: 环境光强度/接近传感器
  - DHT11: 温湿度传感器
  - LED: 用户可控制 LED

## 软件环境

- **Qt 版本**: 5.x
- **交叉编译器**: `arm-linux-gnueabihf-gcc`
- **目标架构**: ARMv7l
- **部署目录**: `/lib/modules/4.1.15-g3dc0a4b/` (开发板)
- **图片存储**: `/media/figure/` (开发板)

## 项目结构

```
.
├── Camera_Project/          # 摄像头应用
│   ├── main.cpp            # 程序入口，信号处理
│   ├── mainwindow.h        # 主窗口头文件
│   ├── mainwindow.cpp      # 主窗口实现
│   ├── v4l2_device.h       # V4L2 设备封装类
│   ├── v4l2_device.cpp     # V4L2 设备实现
│   ├── Camera_Project.pro  # Qt 项目配置
│   ├── deploy.sh           # 自动部署脚本
│   └── README.md           # 详细文档
├── My_Project/             # 传感器控制面板
│   ├── main.cpp            # 程序入口
│   ├── mainwindow.h        # 主窗口头文件
│   ├── mainwindow.cpp      # 主窗口实现
│   ├── myslide.h           # 自定义滑块控件
│   ├── myslide.cpp         # 滑块实现
│   ├── My_Project.pro      # Qt 项目配置
│   ├── deploy.sh           # 自动部署脚本
│   ├── connect_board.sh    # 快速连接脚本
│   ├── CLAUDE_CODE_GUIDELINES.md  # 开发指南
│   └── BOARD_CONTEXT.md    # 开发板上下文档案
├── driver_file/            # Linux 驱动模块源码
│   ├── ap3216c_光照传感器/ # AP3216C 三合一传感器驱动
│   ├── dht11_温湿度传感器/  # DHT11 温湿度传感器驱动
│   ├── led_drv/            # LED 控制驱动
│   ├── sr501_人体感应器/   # SR501 人体红外传感器驱动
│   └── Makefile            # 驱动编译配置
└── README.md               # 本文件
```

## 子项目详情

### 1. Camera_Project

基于 Qt5 和 V4L2 的摄像头应用程序，专为 ATK-IMX6U 开发板设计。

**主要功能**:
- 实时视频预览 (约 33 FPS)
- JPEG 格式拍照保存
- 全屏图片查看器，支持滑动切换
- 触摸屏优化界面
- 优雅的信号处理 (Ctrl+C 安全退出)

**技术特性**:
- V4L2 Mmap 缓冲区管理
- RGB565/YUYV 像素格式支持
- 自动帧率适配
- 多缓冲区循环队列

详细文档请参阅 [Camera_Project/README.md](Camera_Project/README.md)。

### 2. My_Project

传感器控制面板应用，提供硬件交互界面。

**主要功能**:
- **LED 控制**: 打开/关闭用户 LED
- **AP3216C 传感器**: 实时显示环境光强度和接近距离
- **DHT11 传感器**: 实时显示温度和湿度
- **摄像头启动**: 一键启动 Camera_Project 应用
- **全屏触摸界面**: 优化嵌入式触摸操作

**硬件接口**:
- `/dev/led` - LED 控制设备
- `/dev/ap3216c` - AP3216C 传感器设备
- `/dev/dht11` - DHT11 传感器设备
- `/dev/video1` - 摄像头设备 (通过 Camera_Project 访问)

**界面布局**:
```
┌─────────────────────────────────┐
│ 温度: 25.3°C  湿度: 45%         │
│ 光照: 320 lux  距离: 120 mm     │
│                                 │
│  [ LED 开关 ]                   │
│                                 │
│  [ 启动摄像头 ]                  │
└─────────────────────────────────┘
```

## 快速开始

> **⚠️ 重要说明**: 文档中所有 `xxx` 占位符需要替换为您的实际配置：
> - IP 地址：开发板实际 IP 地址
> - 密码：开发板 SSH 登录密码

### 1. 开发环境搭建

需要交叉编译链arm-linux-gnueabihf，安装方法请参考网上教程。

### 2. 编译项目
- QT项目的编译请在虚拟机的QT应用进行
- 驱动编译过程参考正点原子IMX6ULL教程


### 3. QT程序部署

```bash
# Camera_Project
cd Camera_Project
./deploy.sh

# My_Project
cd My_Project
./deploy.sh
```

### 4. 驱动文件部署

本项目包含以下 Linux 驱动模块，源码位于 `driver_file/` 目录：

| 驱动名称 | 功能 | 设备节点 |
|----------|------|----------|
| ap3216c | 环境光/红外/接近传感器 | /dev/ap3216c |
| dht11 | 温湿度传感器 | /dev/dht11 |
| ft5x06 | LCD 触摸屏 | /dev/input/event1 |
| led_drv | LED 控制 | /dev/led |
| sr501 | 人体红外传感器 | /dev/sr501 |

- ==如何将驱动文件下载到开发板，请参考正点原子IMX6ULL教程==

将驱动文件在开发板部署，可以写一个脚本在开发板，一键部署所有驱动并启动QT界面
```bash
#!/bin/sh
set -e

mod_list="led_drv ap3216c_drv dht11_drv sr501_drv"
# The modules to be loaded.
for name in $mod_list; do
    ko="./${name}.ko"
    if [ -f "$ko" ]; then
        echo "[INFO] loading $ko"
        insmod "$ko" || echo "[WARN] $ko already inserted?"
    else
        echo "[ERROR] missing $ko"
    fi
done

./My_Project
```


## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2025-12 | 初始版本，基础功能实现 |
| v1.1 | 2025-12 | 添加 FPS 显示和性能优化 |
| v1.2 | 2026-02-04 | **RELEASE 版本** - 集成 SR501 传感器，UI 布局优化，移除 PXP 改用纯 Qt 渲染 |


## 项目效果

https://www.bilibili.com/video/BV1hHfZB8EDS/

---

**v1.2 RELEASE** - 文档最后更新: 2026-02-04