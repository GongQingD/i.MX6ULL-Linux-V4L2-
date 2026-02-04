#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int main(int argc, char *argv[])
{
    int fd, res;
    unsigned char buff[5];

    if(argc != 2) {
        printf("Usage: %s /dev/yourdevicename\n", argv[0]);
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if(fd < 0) {
        printf("open %s failed!\n", argv[1]);
        return -1;
    };

    sleep(2); // 等待驱动初始化

    while(1){
        res = read(fd, buff, 5);
        if(res < 0) {
            printf("read failed!\n");
            close(fd);
            return -1;
        }
        printf("Humidity: %d.%d %% Temperature: %d.%d C\n", buff[0], buff[1], buff[2], buff[3]);
        sleep(2); // DHT11 最小读取间隔为1秒
    }


}