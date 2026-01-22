#ifndef V4L2_DEVICE_H
#define V4L2_DEVICE_H

#include <linux/videodev2.h>
#include <string>
#include <vector>

struct Buffer {
    void   *start;
    size_t length;
};

class V4L2Device {
public:
    V4L2Device();
    ~V4L2Device();

    bool openDevice(const std::string &deviceName);
    bool initDevice(int width, int height);
    bool startCapturing();
    bool stopCapturing();
    bool closeDevice();
    
    // 获取一帧数据，返回数据指针和长度
    int getFrame(unsigned char **data, size_t *length);
    // 处理完帧后放回队列
    bool releaseFrame();

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getFileDescriptor() const { return fd; }  // 暴露文件描述符

private:
    std::string deviceName;
    int fd;
    int width;
    int height;
    std::vector<Buffer> buffers;
    struct v4l2_buffer currentBuffer; // 记录当前取出的 buffer 信息以便释放
    bool isCapturing;

    bool initMmap();
};

#endif // V4L2_DEVICE_H
