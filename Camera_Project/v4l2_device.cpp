#include "v4l2_device.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <iostream>

V4L2Device::V4L2Device() : fd(-1), width(640), height(480), isCapturing(false) {
    memset(&currentBuffer, 0, sizeof(currentBuffer));
}

V4L2Device::~V4L2Device() {
    stopCapturing();
    closeDevice();
}

/* 打开设备 */
bool V4L2Device::openDevice(const std::string &name) {
    deviceName = name;
    fd = open(deviceName.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        perror("Opening video device");
        return false;
    }
    return true;
}

/* 初始化设备，设置屏幕的分辨率和像素格式 */
bool V4L2Device::initDevice(int w, int h) {
    width = w;
    height = h;

    // 1. 设置格式（已有代码）
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
             perror("Setting Pixel Format");
             return false;
        }
    }
    
    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;

    // ===== 新增：设置帧率 =====
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    // 先查询当前参数
    if (ioctl(fd, VIDIOC_G_PARM, &parm) == -1) {
        perror("Getting stream parameters");
        // 非致命错误，继续执行
    } else {
        // 设置帧率为 30 FPS
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = 30;
        
        if (ioctl(fd, VIDIOC_S_PARM, &parm) == -1) {
            perror("Setting stream parameters");
            // 非致命错误，有些驱动不支持
        } else {
            // 读取实际设置的帧率（驱动可能调整）
            // int actualFPS = parm.parm.capture.timeperframe.denominator /
            //                parm.parm.capture.timeperframe.numerator;
            // 调试输出已移除
        }
    }
    // ===== 新增结束 =====

    return initMmap();
}

/* 申请帧缓冲，初始化内存映射 */
bool V4L2Device::initMmap() {
    /* 申请帧缓冲 */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Requesting Buffer");
        return false;
    }

    buffers.resize(req.count);
    /* 查询帧缓冲的信息：首地址长度，偏移量 */
    for (size_t i = 0; i < buffers.size(); ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));//数据清零
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;//第i个缓冲区，此时已经申请到了帧缓冲空间，接下来是查询帧缓冲信息
        // 获取第i个缓冲区的信息
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Querying Buffer");
            return false;
        }
        // 帧缓冲的首地址和长度信息已经存到buf中
        buffers[i].length = buf.length;// 第i个缓冲区的长度
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);//返回指向每一个帧缓冲在用户空间的地址的指针

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            return false;
        }
    }
    return true;
}

bool V4L2Device::startCapturing() {
    /* 入队 */
    for (size_t i = 0; i < buffers.size(); ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Queue Buffer");
            return false;
        }
    }
    /* 开启视频采集 */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("Stream On");
        return false;
    }
    isCapturing = true;
    return true;
}

bool V4L2Device::stopCapturing() {
    if (!isCapturing) return true;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("Stream Off");
        return false;
    }
    isCapturing = false;
    return true;
}

/* 关闭设备，释放资源 */
bool V4L2Device::closeDevice() {
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i].start && buffers[i].start != MAP_FAILED) {
            munmap(buffers[i].start, buffers[i].length);
        }
    }
    if (fd != -1) {
        close(fd);
        fd = -1;
    }
    return true;
}

/* 获取一帧数据，将该帧数据的首地址和长度赋值给输入的指针变量 */
int V4L2Device::getFrame(unsigned char **data, size_t *length) {
    memset(&currentBuffer, 0, sizeof(currentBuffer));
    currentBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    currentBuffer.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &currentBuffer) == -1) {
        return -1; // 暂时没有数据或错误
    }

    *data = (unsigned char*)buffers[currentBuffer.index].start;
    *length = currentBuffer.bytesused;
    return currentBuffer.index;
}

/* 释放一帧数据，重新入队 */
bool V4L2Device::releaseFrame() {
    if (ioctl(fd, VIDIOC_QBUF, &currentBuffer) == -1) {
        perror("Queue Buffer (Release)");
        return false;
    }
    return true;
}
