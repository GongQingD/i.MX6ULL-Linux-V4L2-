KERNELDIR := /home/ladykaka/Linux/linux-imx-4.1.15-2.1.0-e48931b1-v2.8
CURRENT_PATH := $(shell pwd)

obj-m := ap3216c_drv.o

build: kernel_modules

# 禁用位置无关代码 (PIC)，解决 "does not support PIC mode" 错误
ccflags-y += -fno-pie

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean