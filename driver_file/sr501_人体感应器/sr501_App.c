#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

int fd;

/* 处理Ctrl+C信号，退出程序 */
void handler(int signum)
{
    close(fd);
    exit(0);
}
/* 处理异步通知信号SIGIO */
void sr501_handler(int signum)
{
    char val;
    read(fd, &val, 1);
    printf("val is %d, %s\n", val, val == 1 ? "have people" : "no people");
}

int main(int argc, char *argv[])
{
    int flags;

    signal(SIGINT, handler);      // Ctrl+C 退出
    signal(SIGIO, sr501_handler); // 异步通知处理

    fd = open("/dev/sr501", O_RDWR);
    if (fd < 0) {
        printf("/dev/sr501 open failed\n");
        return 1;
    }

    fcntl(fd, F_SETOWN, getpid());           // 设置接收SIGIO的进程
    flags = fcntl(fd, F_GETFL);              // 获取文件状态
    fcntl(fd, F_SETFL, flags | O_ASYNC);     // 对该文件的操作启用异步通知，该代码会触发驱动中的fasync函数

    while (1) {
        sleep(1); // 主循环等待信号
    }

    return 0;
}