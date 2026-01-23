# ATK-IMX6U Qt 应用程序套件

本项目包含两个针对 ATK-IMX6U 开发板的 Qt 应用程序，分别实现摄像头功能和传感器控制面板。

## 项目概览

| 项目 | 描述 |
|------|------|
| [Camera_Project](Camera_Project/) | 基于 V4L2 的摄像头应用，支持实时预览、拍照保存和图片浏览 |
| [My_Project](My_Project/) | 传感器控制面板，集成 LED 控制、AP3216C 光感/距离传感器、DHT11 温湿度传感器，并可启动摄像头应用 |

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

### 1. 开发环境搭建

```bash
# 安装交叉编译工具链 (Ubuntu)
sudo apt-get install gcc-arm-linux-gnueabihf

# 设置 Qt 交叉编译环境
export PATH=/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin:$PATH
```

### 2. 编译项目

#### Camera_Project
```bash
cd Camera_Project
/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/qmake
make
```

#### My_Project
```bash
cd My_Project
/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/qmake
make
```

### 3. 自动部署

每个项目都提供了部署脚本：

```bash
# Camera_Project
cd Camera_Project
./deploy.sh

# My_Project
cd My_Project
./deploy.sh
```

### 4. 手动部署

```bash
# 上传到开发板
sshpass -p "2918" scp -o HostKeyAlgorithms=+ssh-rsa Camera_Project/Camera_Project root@10.20.20.36:/lib/modules/4.1.15-g3dc0a4b/
sshpass -p "2918" scp -o HostKeyAlgorithms=+ssh-rsa My_Project/My_Project root@10.20.20.36:/lib/modules/4.1.15-g3dc0a4b/
```

### 5. 运行应用

```bash
# 连接到开发板
sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36

# 运行传感器控制面板
/lib/modules/4.1.15-g3dc0a4b/My_Project

# 运行摄像头应用 (或从 My_Project 界面启动)
/lib/modules/4.1.15-g3dc0a4b/Camera_Project
```

## 连接信息

| 参数 | 值 |
|------|-----|
| IP 地址 | 10.20.20.36 |
| 用户名 | root |
| SSH 密码 | 2918 |
| 连接命令 | `sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36` |

**注意**: 由于开发板 SSH 版本较旧，必须携带 `-o HostKeyAlgorithms=+ssh-rsa` 参数。

## 驱动准备

在运行应用前，确保以下驱动已加载：

```bash
# 检查摄像头驱动
lsmod | grep -E "ov5640|mx6s_capture"

# 检查传感器驱动
lsmod | grep -E "ap3216c|dht11"

# 检查 LED 驱动
lsmod | grep led
```

如果驱动未加载，请先安装对应的内核模块：

```bash
# 上传驱动模块到开发板
sshpass -p "2918" scp -o HostKeyAlgorithms=+ssh-rsa *.ko root@10.20.20.36:/lib/modules/4.1.15-g3dc0a4b/

# 加载驱动
insmod /lib/modules/4.1.15-g3dc0a4b/ap3216c.ko
insmod /lib/modules/4.1.15-g3dc0a4b/led_drv.ko
insmod /lib/modules/4.1.15-g3dc0a4b/dht11.ko
```

## 故障排查

### 常见问题

1. **摄像头黑屏**
   ```bash
   # 检查设备权限
   ls -la /dev/video1

   # 检查像素格式支持
   v4l2-ctl --device=/dev/video1 --list-formats

   # 查看内核日志
   dmesg | grep -i camera
   ```

2. **传感器读数失败**
   ```bash
   # 检查设备文件是否存在
   ls -la /dev/ap3216c /dev/dht11 /dev/led

   # 检查驱动是否加载
   lsmod | grep -E "ap3216c|dht11|led"

   # 测试设备访问
   cat /dev/ap3216c
   ```

3. **Qt 应用启动失败**
   ```bash
   # 检查 Qt 库路径
   echo $QT_QPA_PLATFORM

   # 设置显示环境
   export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
   export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/event1
   ```

### 调试命令

```bash
# 查看系统信息
uname -a
cat /proc/cpuinfo

# 检查内存和存储
free -h
df -h

# 监控系统日志
dmesg -w

# 测试网络连接
ping -c 3 10.20.20.36
```

## 开发指南

### 代码结构规范

1. **硬件抽象层**: 每个硬件设备对应独立的类封装
2. **业务逻辑层**: 在主窗口中处理用户交互
3. **资源管理**: 使用 RAII 原则管理文件描述符和内存
4. **错误处理**: 所有系统调用都检查返回值

### 交叉编译注意事项

1. **工具链选择**: 使用 `arm-linux-gnueabihf-gcc` 系列工具
2. **库依赖**: 静态链接或部署共享库到开发板
3. **调试符号**: 发布版本去除调试信息以减小体积
4. **版本兼容**: 确保库版本与开发板系统匹配

### 性能优化

1. **缓冲区复用**: 避免频繁的内存分配和释放
2. **事件驱动**: 使用定时器代替轮询
3. **图像处理**: 使用硬件加速格式 (RGB565)
4. **界面渲染**: 减少不必要的重绘

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2024 | 初始版本，基础功能实现 |
| v1.1 | 2024-12 | 添加 FPS 显示和性能优化 |

## 许可证

本项目仅供学习和研究使用。未经许可不得用于商业用途。

## 致谢

- 正点原子 ATK-IMX6U 开发板
- Qt 开源框架
- Linux V4L2 子系统
- 所有开源社区贡献者

---

*文档最后更新: 2026-01-23*