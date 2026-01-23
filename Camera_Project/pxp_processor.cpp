#include "pxp_processor.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cstdio>

PXPProcessor::PXPProcessor()
    : m_fd(-1), m_virtAddr(nullptr), m_lcdWidth(0), m_lcdHeight(0),
      m_lcdPhyAddr(0), m_initialized(false)
{
    memset(&m_handle, 0, sizeof(m_handle));
    memset(&m_buffer, 0, sizeof(m_buffer));
}

PXPProcessor::~PXPProcessor()
{
    close();
}

unsigned long PXPProcessor::getFramebufferPhysicalAddr(const char *device)
{
    int fb_fd = open(device, O_RDWR);
    if (fb_fd < 0) {
        perror("Failed to open framebuffer");
        return 0;
    }

    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("Failed to get framebuffer fixed info");
        ::close(fb_fd);
        return 0;
    }

    ::close(fb_fd);
    return finfo.smem_start;
}

bool PXPProcessor::init(const char *fbDevice)
{
    // 1. 获取LCD物理地址
    m_lcdPhyAddr = getFramebufferPhysicalAddr(fbDevice);
    if (m_lcdPhyAddr == 0) {
        fprintf(stderr, "Failed to get framebuffer physical address\n");
        return false;
    }

    // 2. 打开framebuffer获取尺寸
    int fb_fd = open(fbDevice, O_RDWR);
    if (fb_fd < 0) {
        perror("Failed to open framebuffer for size");
        return false;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Failed to get framebuffer variable info");
        ::close(fb_fd);
        return false;
    }

    m_lcdWidth = vinfo.xres;
    m_lcdHeight = vinfo.yres;
    ::close(fb_fd);

    fprintf(stderr, "PXP: LCD %dx%d, phyAddr=0x%lx\n", m_lcdWidth, m_lcdHeight, m_lcdPhyAddr);

    // 3. 打开PXP设备
    m_fd = open("/dev/pxp_device", O_RDWR);
    if (m_fd < 0) {
        perror("Failed to open /dev/pxp_device");
        return false;
    }

    // 4. 获取PXP通道
    if (ioctl(m_fd, PXP_IOC_GET_CHAN, &m_handle.handle) < 0) {
        perror("Failed to get PXP channel");
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // 5. 分配PXP物理内存（使用源图像尺寸，640x480 RGB565）
    // PXP缓冲区只需要容纳输入图像，输出直接写入framebuffer
    size_t bufferSize = 640 * 480 * 2;  // RGB565 = 2字节/像素
    if (!allocateBuffer(bufferSize)) {
        ioctl(m_fd, PXP_IOC_PUT_CHAN, &m_handle.handle);
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    m_initialized = true;
    fprintf(stderr, "PXP: initialized successfully\n");
    return true;
}

void PXPProcessor::close()
{
    if (!m_initialized) return;

    if (m_virtAddr && m_virtAddr != MAP_FAILED) {
        munmap(m_virtAddr, m_buffer.size);
        m_virtAddr = nullptr;
    }

    if (m_buffer.size > 0) {
        ioctl(m_fd, PXP_IOC_PUT_PHYMEM, &m_buffer);
        memset(&m_buffer, 0, sizeof(m_buffer));
    }

    if (m_handle.handle) {
        ioctl(m_fd, PXP_IOC_PUT_CHAN, &m_handle.handle);
        memset(&m_handle, 0, sizeof(m_handle));
    }

    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }

    m_initialized = false;
}

bool PXPProcessor::allocateBuffer(size_t size)
{
    m_buffer.size = size;
    m_buffer.mtype = MEMORY_TYPE_UNCACHED;

    if (ioctl(m_fd, PXP_IOC_GET_PHYMEM, &m_buffer) < 0) {
        perror("Failed to get PXP physical memory");
        return false;
    }

    // 内存映射
    m_virtAddr = (uint32_t *)mmap(nullptr, m_buffer.size,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, m_fd, m_buffer.phys_addr);

    if (m_virtAddr == MAP_FAILED) {
        perror("mmap failed for PXP buffer");
        m_virtAddr = nullptr;
        // 释放已分配的物理内存
        ioctl(m_fd, PXP_IOC_PUT_PHYMEM, &m_buffer);
        memset(&m_buffer, 0, sizeof(m_buffer));
        return false;
    }

    return true;
}

void PXPProcessor::freeBuffer()
{
    if (m_virtAddr && m_virtAddr != MAP_FAILED) {
        munmap(m_virtAddr, m_buffer.size);
        m_virtAddr = nullptr;
    }

    if (m_buffer.size > 0) {
        ioctl(m_fd, PXP_IOC_PUT_PHYMEM, &m_buffer);
        memset(&m_buffer, 0, sizeof(m_buffer));
    }
}

// RGB565转ARGB32 (快速软件转换)
bool PXPProcessor::convertRGB565ToARGB32(const uint16_t *src, uint32_t *dst,
                                              int width, int height)
{
    if (!src || !dst) return false;

    const uint16_t *srcPtr = src;
    uint32_t *dstPtr = dst;
    int totalPixels = width * height;

    for (int i = 0; i < totalPixels; i++) {
        uint16_t pixel = *srcPtr++;

        // RGB565: RRRRRGGG GGGBBBBB
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;  // 5位扩展到8位
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;   // 6位扩展到8位
        uint8_t b = (pixel & 0x1F) << 3;          // 5位扩展到8位

        // ARGB32: AAAAAAAA RRRRRRRR GGGGGGGG BBBBBBBB
        *dstPtr++ = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }

    return true;
}

bool PXPProcessor::processFrame(const uint16_t *rgb565Data, int srcWidth, int srcHeight,
                                  int dstX, int dstY, int dstWidth, int dstHeight)
{
    static int frameCount = 0;
    static int lastPrintFrame = 0;
    frameCount++;

    if (!m_initialized || !rgb565Data) {
        fprintf(stderr, "PXP: not initialized or null data at frame %d\n", frameCount);
        return false;
    }

    // 直接使用 RGB565 格式，不经过 ARGB32 转换
    // 配置PXP处理
    struct pxp_config_data config;
    memset(&config, 0, sizeof(config));

    // 输入参数（源图像 - RGB565，直接使用摄像头数据的物理地址需要额外处理）
    // 由于摄像头数据是映射的虚拟地址，我们需要先复制到PXP缓冲区
    memcpy(m_virtAddr, rgb565Data, srcWidth * srcHeight * 2);  // RGB565 = 2字节/像素

    // 输入参数（源图像 - RGB565）
    config.s0_param.paddr = m_buffer.phys_addr;
    config.s0_param.width = srcWidth;
    config.s0_param.height = srcHeight;
    config.s0_param.pixel_fmt = PXP_PIX_FMT_RGB565;
    config.s0_param.stride = srcWidth * 2;

    // 源矩形（整个源图像）
    config.proc_data.srect.left = 0;
    config.proc_data.srect.top = 0;
    config.proc_data.srect.width = srcWidth;
    config.proc_data.srect.height = srcHeight;

    // 输出参数（Framebuffer - RGB565）
    config.out_param.paddr = m_lcdPhyAddr;
    config.out_param.width = m_lcdWidth;
    config.out_param.height = m_lcdHeight;
    config.out_param.pixel_fmt = PXP_PIX_FMT_RGB565;
    config.out_param.stride = m_lcdWidth * 2;

    // 目标矩形（显示位置和大小）
    config.proc_data.drect.left = dstX;
    config.proc_data.drect.top = dstY;
    config.proc_data.drect.width = dstWidth;
    config.proc_data.drect.height = dstHeight;

    // 启用缩放和插值
    config.proc_data.scaling = 1;
    config.layer_nr = 2;

    // 前10帧每帧输出，之后每60帧输出一次
    if (frameCount <= 10 || (frameCount - lastPrintFrame >= 60)) {
        fprintf(stderr, "PXP: frame %d, src=%dx%d, dst=%d,%d %dx%d, fb_phy=0x%lx, buf_phy=0x%lx\n",
                frameCount, srcWidth, srcHeight, dstX, dstY, dstWidth, dstHeight,
                m_lcdPhyAddr, m_buffer.phys_addr);
        lastPrintFrame = frameCount;
    }

    // 3. 配置PXP通道
    if (ioctl(m_fd, PXP_IOC_CONFIG_CHAN, &config) < 0) {
        perror("PXP_IOC_CONFIG_CHAN failed");
        return false;
    }

    // 4. 启动PXP处理
    if (ioctl(m_fd, PXP_IOC_START_CHAN, &m_handle.handle) < 0) {
        perror("PXP_IOC_START_CHAN failed");
        return false;
    }

    // 5. 等待PXP完成（硬件加速，约2-3ms）
    if (ioctl(m_fd, PXP_IOC_WAIT4CMPLT, &m_handle) < 0) {
        perror("PXP_IOC_WAIT4CMPLT failed");
        return false;
    }

    if (frameCount <= 10 || frameCount == lastPrintFrame) {
        fprintf(stderr, "PXP: frame %d processed successfully\n", frameCount);
    }

    return true;
}
