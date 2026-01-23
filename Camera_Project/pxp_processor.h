#ifndef PXP_PROCESSOR_H
#define PXP_PROCESSOR_H

#include <stdint.h>
#include <cstddef>  // for size_t
#include <linux/fb.h>
#include "pxp_types.h"  // 使用自定义 PXP 类型，避免 bool 冲突

// PXP处理器 - 使用硬件加速进行图像缩放和显示
class PXPProcessor
{
public:
    PXPProcessor();
    ~PXPProcessor();

    // 初始化PXP设备
    bool init(const char *fbDevice = "/dev/fb0");

    // 关闭PXP设备
    void close();

    // 是否已初始化
    bool isReady() const { return m_fd >= 0 && m_initialized; }

    // 获取LCD尺寸
    int getLCDWidth() const { return m_lcdWidth; }
    int getLCDHeight() const { return m_lcdHeight; }

    // 处理一帧图像（RGB565 → ARGB32 → 缩放 → Framebuffer）
    bool processFrame(const uint16_t *rgb565Data, int srcWidth, int srcHeight,
                      int dstX, int dstY, int dstWidth, int dstHeight);

private:
    bool allocateBuffer(size_t size);
    void freeBuffer();
    bool convertRGB565ToARGB32(const uint16_t *src, uint32_t *dst, int width, int height);
    unsigned long getFramebufferPhysicalAddr(const char *device);

    int m_fd;                           // PXP设备文件描述符
    struct pxp_chan_handle m_handle;    // PXP通道句柄
    struct pxp_mem_desc m_buffer;       // PXP物理内存描述符
    uint32_t *m_virtAddr;               // 虚拟地址映射
    int m_lcdWidth;                    // LCD宽度
    int m_lcdHeight;                   // LCD高度
    unsigned long m_lcdPhyAddr;       // LCD物理地址
    bool m_initialized;                 // 是否已初始化
};

#endif // PXP_PROCESSOR_H
