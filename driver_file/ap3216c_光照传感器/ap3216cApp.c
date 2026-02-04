#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int fd;
    char *filename;
    unsigned char databuf[6];
    unsigned short ir, als, ps;
    int ret;

    if (argc != 2) {
        printf("Usage: %s /dev/yourdevicename\n", argv[0]);
        return -1;
    }

    filename = argv[1];
    fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    while (1) {
        ret = read(fd, databuf, sizeof(databuf));
        if (ret == 6) {
            ir  = (databuf[0] << 8) | databuf[1];
            als = (databuf[2] << 8) | databuf[3];
            ps  = (databuf[4] << 8) | databuf[5];
            printf("IR: %u, ALS: %u, PS: %u\n", ir, als, ps);
        } else {
            printf("Read error or unexpected data size: %d\n", ret);
        }
        usleep(200000); // 200ms
    }

    close(fd);
    return 0;
}