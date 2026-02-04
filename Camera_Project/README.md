# Camera_Project - ATK-IMX6U V4L2 摄像头应用

基于 Qt5 和 V4L2 的摄像头应用程序，专为 ATK-IMX6U 开发板设计。

## 项目简介

本项目是一个面向嵌入式 Linux (ATK-IMX6U) 的摄像头应用，实现了实时视频预览、拍照保存、图片查看等功能。界面采用全屏设计，优化了触摸操作体验。

### 主要特性

- **实时视频预览**：通过 V4L2 接口获取摄像头数据，以约 33FPS 刷新率显示
- **拍照功能**：支持 JPEG 格式保存，文件自动命名（时间戳格式）
- **图片查看器**：全屏图片浏览，支持左右滑动切换
- **触摸优化**：全屏无边框界面，大按钮设计，适合触摸屏操作
- **优雅退出**：支持 Ctrl+C 信号处理，确保资源正确释放

## 硬件环境

| 项目 | 参数 |
|------|------|
| 开发板 | ATK-IMX6U (正点原子 i.MX6U 系列) |
| 摄像头 | OV5640 (5MP CMOS) |
| 接口 | MIPI CSI2 |
| 视频设备 | `/dev/video1` |
| LCD 分辨率 | 1024×600 |
| 像素格式 | RGB565 |
| 内核版本 | 4.1.15-g3dc0a4b |

## 项目结构

```
Camera_Project/
├── main.cpp           # 程序入口，信号处理和字体设置
├── mainwindow.h       # 主窗口头文件
├── mainwindow.cpp     # 主窗口实现（UI 和业务逻辑）
├── v4l2_device.h      # V4L2 设备封装类头文件
├── v4l2_device.cpp    # V4L2 设备封装类实现
└── Camera_Project.pro # Qt 项目配置文件
```

## 模块说明

### V4L2Device 类

V4L2 设备的 C++ 封装，提供简洁的摄像头操作接口。

| 方法 | 说明 |
|------|------|
| `openDevice(name)` | 打开视频设备 |
| `initDevice(w, h)` | 初始化分辨率和像素格式 |
| `startCapturing()` | 启动视频捕获 |
| `getFrame(&data, &len)` | 获取一帧数据 |
| `releaseFrame()` | 释放帧缓冲区 |
| `stopCapturing()` | 停止捕获 |
| `closeDevice()` | 关闭设备 |

**像素格式支持**：
- 优先：`V4L2_PIX_FMT_RGB565` (RGB565)
- 回退：`V4L2_PIX_FMT_YUYV` (YUYV)

### MainWindow 类

主界面窗口，包含摄像头预览区域、拍照按钮和文件列表。

**布局结构**：
```
┌─────────────────────────┬─────────┐
│                         │         │
│    摄像头预览区域        │         │
│    (占左侧 3/4 高度)     │         │
│                         │  拍照   │
├─────────────────────────┤  按钮   │
│    照片列表 (最近5张)    │         │
├─────────────────────────┴─────────┘
```

### ImageViewerDialog 类

全屏图片查看器对话框，支持触摸/鼠标滑动切换图片。

**交互方式**：
- 点击 `<` 按钮：上一张
- 点击 `>` 按钮：下一张
- 点击 "退出" 按钮：关闭查看器
- 左右滑动：切换图片

## 编译与部署

### 编译环境

- Qt 5.x
- 交叉编译器：`arm-linux-gnueabihf-gcc`
- 适用于 ARMv7l 架构

### 本地编译（Ubuntu 开发机）

```bash
cd /home/ladykaka/QT_doc/Camera_Project
/opt/fsl-imx-x11/4.1.15-2.1.0/sysroots/x86_64-pokysdk-linux/usr/bin/qmake
make
```

### 自动部署脚本

```bash
./build_imx6u.sh
```

该脚本会自动：
1. 清理旧的构建文件
2. 运行 qmake 和 make
3. 将可执行文件上传到开发板

### 手动部署

```bash
# 交叉编译
arm-linux-gnueabihf-gcc main.cpp mainwindow.cpp v4l2_device.cpp -o Camera_Project

# 上传到开发板
sshpass -p "2918" scp -o HostKeyAlgorithms=+ssh-rsa Camera_Project root@10.20.20.36:/lib/modules/4.1.15-g3dc0a4b/

# 在开发板上运行
sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36 "/lib/modules/4.1.15-g3dc0a4b/Camera_Project"
```

## 使用说明

### 运行应用

```bash
# SSH 连接到开发板
sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36

# 启动程序
/lib/modules/4.1.15-g3dc0a4b/Camera_Project
```

### 操作说明

1. **预览视频**：应用启动后自动显示摄像头实时画面
2. **拍照**：点击右侧黄色大按钮
3. **查看照片**：点击左下角列表中的文件名
4. **退出程序**：按 Ctrl+C

### 照片存储

- **存储路径**：`/media/figure/`
- **文件命名**：`capture_yyyyMMdd_HHmmss.jpg`
- **格式**：JPEG
- **列表显示**：最近 5 张照片

## 技术细节

### V4L2 Mmap 机制

项目使用 Mmap 方式进行高效的帧缓冲区管理：

```
┌─────────────────────────────────────────────┐
│              V4L2 驱动层                     │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐           │
│  │Buf 0│ │Buf 1│ │Buf 2│ │Buf 3│ (4个缓冲) │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘           │
└─────┼──────┼──────┼──────┼─────────────────┘
      │ mmap │ mmap │ mmap │ mmap
┌─────┼──────┼──────┼──────┼─────────────────┐
│     ▼      ▼      ▼      ▼                 │
│  ┌─────────────────────────────────┐       │
│  │   用户空间 V4L2Device 类         │       │
│  └─────────────────────────────────┘       │
└─────────────────────────────────────────────┘
```

### 帧处理流程

```
1. VIDIOC_REQBUFS  → 请求缓冲区
2. VIDIOC_QUERYBUF → 查询缓冲区信息
3. mmap            → 映射到用户空间
4. VIDIOC_QBUF     → 将缓冲区放入队列
5. VIDIOC_STREAMON → 开始流传输
6. VIDIOC_DQBUF    → 取出已填充的缓冲区
7. 图像处理        → QImage 转换和显示
8. VIDIOC_QBUF     → 将缓冲区放回队列
9. 重复步骤 6-8
```

### 信号处理机制

应用使用 socketpair + QSocketNotifier 实现安全的 Unix 信号处理：

```
SIGINT (Ctrl+C)
    │
    ▼
┌─────────────────┐
│ sigIntHandler() │ → write(sigIntFd[0])
│ (信号处理函数)   │
└─────────────────┘
         │
         ▼ (socket 通信)
┌────────────────────┐
│ QSocketNotifier    │ → activated 信号
│ (监听 sigIntFd[1]) │
└────────────────────┘
         │
         ▼
┌────────────────────┐
│ handleSigInt()     │ → QApplication::quit()
│ (槽函数)           │
└────────────────────┘
```

### Qt 依赖库

```pro
QT += core gui widgets
```

## 故障排查

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 无法打开 `/dev/video1` | 摄像头驱动未加载 | 检查 `lsmod | grep ov5640` |
| 画面黑屏 | 像素格式不匹配 | 检查摄像头支持格式 |
| 照片保存失败 | 目录权限不足 | 检查 `/media/figure/` 权限 |
| 触摸无响应 | Qt触摸事件被禁用 | 检查 `WA_AcceptTouchEvents` 设置 |

### 调试命令

```bash
# 检查视频设备
v4l2-ctl --device=/dev/video1 --list-formats

# 查看内核日志
dmesg | grep -i camera

# 检查文件权限
ls -la /media/figure/

# 测试摄像头
v4l2-ctl --device=/dev/video1 --stream-mmap --stream-count=10
```

## 开发环境连接

| 参数 | 值 |
|------|-----|
| IP 地址 | 10.20.20.36 |
| 用户名 | root |
| SSH 密码 | 2918 |
| 连接命令 | `sshpass -p "2918" ssh -o HostKeyAlgorithms=+ssh-rsa root@10.20.20.36` |

**注意**：由于开发板 SSH 版本较旧，必须携带 `-o HostKeyAlgorithms=+ssh-rsa` 参数。

## 版本信息

- **创建日期**：2024
- **Qt 版本**：5.x
- **目标平台**：ARMv7l (imx6ull)
- **内核版本**：4.1.15-g3dc0a4b

## 许可证

本项目仅供学习和研究使用。

---

*文档生成于 2025-12-23*
