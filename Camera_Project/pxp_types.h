#ifndef PXP_TYPES_H
#define PXP_TYPES_H

#include <linux/ioctl.h>
#include <stdint.h>

// PXP Ioctl 定义（避免包含内核头文件中的 bool 冲突）
#define PXP_IOC_MAGIC  'P'

#define PXP_IOC_GET_CHAN      _IOR(PXP_IOC_MAGIC, 0, struct pxp_mem_desc)
#define PXP_IOC_PUT_CHAN      _IOW(PXP_IOC_MAGIC, 1, struct pxp_mem_desc)
#define PXP_IOC_CONFIG_CHAN   _IOW(PXP_IOC_MAGIC, 2, struct pxp_config_data)
#define PXP_IOC_START_CHAN    _IOW(PXP_IOC_MAGIC, 3, struct pxp_mem_desc)
#define PXP_IOC_GET_PHYMEM    _IOWR(PXP_IOC_MAGIC, 4, struct pxp_mem_desc)
#define PXP_IOC_PUT_PHYMEM    _IOW(PXP_IOC_MAGIC, 5, struct pxp_mem_desc)
#define PXP_IOC_WAIT4CMPLT    _IOWR(PXP_IOC_MAGIC, 6, struct pxp_chan_handle)

// 内存类型
#define MEMORY_TYPE_UNCACHED 0x0

// 像素格式定义
#define fourcc(a, b, c, d) ((uint32_t)(a)<<0 | (uint32_t)(b)<<8 | (uint32_t)(c)<<16 | (uint32_t)(d)<<24)
#define PXP_PIX_FMT_ARGB32  fourcc('A', 'R', 'G', 'B')
#define PXP_PIX_FMT_RGB565  fourcc('R', 'G', 'B', 'P')  // Framebuffer 实际格式

// PXP 通道句柄
struct pxp_chan_handle {
    unsigned int handle;
    int hist_status;
};

// PXP 内存描述符
struct pxp_mem_desc {
    unsigned int handle;
    unsigned int size;
    unsigned long phys_addr;  // dma_addr_t
    unsigned int virt_uaddr;
    unsigned int mtype;
};

// 矩形区域
struct rect {
    int top;
    int left;
    int width;
    int height;
};

// PXP 图层参数
struct pxp_layer_param {
    unsigned short left;
    unsigned short top;
    unsigned short width;
    unsigned short height;
    unsigned short stride;
    unsigned int pixel_fmt;
    unsigned int flag;
    unsigned long paddr;  // dma_addr_t
};

// PXP 处理数据
struct pxp_proc_data {
    struct rect srect;
    struct rect drect;
    unsigned int scaling;
    unsigned int rotation;
    unsigned int hflip;
    unsigned int vflip;
    unsigned int bgcolor;
    unsigned int overlay_layer;
    unsigned int layer_nr;
};

// PXP 配置数据
struct pxp_config_data {
    struct pxp_layer_param s0_param;
    struct pxp_layer_param out_param;
    struct pxp_proc_data proc_data;
    unsigned int proc_data_invalid;
    unsigned int layer_nr;  // 图层数量
};

#endif // PXP_TYPES_H
