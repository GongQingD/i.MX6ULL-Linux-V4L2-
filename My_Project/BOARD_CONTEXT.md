# 开发板上下文档案 (BOARD_CONTEXT.md)

## 1. 连接信息
* **IP地址**: 10.20.20.36
* **用户**: root
* **连接协议**: SSH (Legacy RSA)
* **主机名**: ATK-IMX6U (推测为正点原子 i.MX6U系列)
* **SSH的密码**：2918

## 2. 系统概览
* **内核版本**: 4.1.15-g3dc0a4b
* **操作系统**: Freescale i.MX Release Distro 4.1.15-2.1.0
* **文件系统**: /dev/root (7.0G, 已用9%)
* **架构**: ARMv7l (imx6ull)

## 3. 硬件参数 (LCD)
* **Framebuffer设备**: /dev/fb0 (mxs-lcdif驱动)
* **分辨率**: 1024×600
* **像素深度**: 16 bpp
* **像素格式**: RGB565
  - R: 偏移11, 长度5位
  - G: 偏移5, 长度6位
  - B: 偏移0, 长度5位
* **行字节数**: 2048 字节

## 4. 驱动开发记录
* **驱动路径**: `/lib/modules`
