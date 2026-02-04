#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/gpio/consumer.h>

#include <linux/cdev.h> // 修复cdev类型不完全

#define DEVICE_NAME "sr501"

/*
 * 步骤一：定义设备私有数据结构
 */
struct my_sr501_dev
{
    struct gpio_desc *gpiod; // 该设备对应的GPIO描述符
    int minor;               // 该设备对应的次设备号
    struct cdev cdev;
    char name[32];           // 从设备树读取的设备名，用于创建设备节点
    struct device *device;   // 该设备对应的device结构体指针
    struct timer_list sr501_timer; // 定时器
    int irq;                     // GPIO对应的中断号
    int gpio_num;                // GPIO编号
    struct fasync_struct *sr501_fasync; // 异步通知队列
};

/*
 * 步骤二：定义全局变量
 */
static int major;           // 该驱动的主设备号
static dev_t my_device_num; // 存设备号
static struct class *my_dev_class; // 设备类
struct my_sr501_dev *p_sr501;         // 设备私有数据结构指针

/* 中断处理函数*/
static irqreturn_t sr501_irq_handler(int irq, void *dev_id)
{
    mod_timer(&p_sr501->sr501_timer, jiffies+HZ/50);//开始1/50s的计时
	return IRQ_HANDLED;             //中断处理完成，返回IRQ_HANDLED
}
/* 定时器回调函数*/
void sr501_timer_callback(unsigned long data)
{
    //定时器回调函数，定时器到期后会执行这里的
    kill_fasync(&p_sr501->sr501_fasync, SIGIO, POLL_IN);   //异步通知
    // sr_fasync是设备私有数据结构中的fasync_struct指针
    // SIGIO是发送的信号
    // POLL_IN表示有数据可读
}


/*
 * 步骤三：实现文件操作函数file_operations
 */
static int sr501_open (struct inode *node, struct file *filp)
{
    filp->private_data = p_sr501;
    return 0;

}

static ssize_t sr501_read (struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    char status;

    struct my_sr501_dev *p_sr501 = filp->private_data;
    if (p_sr501 == NULL || p_sr501->gpiod == NULL) {
        return -EFAULT;
    };
    if(*offset > 0) {
        return 0; // 只允许从文件开头读取
    }

    status = gpiod_get_value(p_sr501->gpiod); // 读取GPIO电平
    // 将电平状态status拷贝到buf中
    if (copy_to_user(buf, &status, 1)) {
        pr_err("Failed to copy data to user\n");
        return -EFAULT;
    }

    return 1; // 返回成功读取的字节数
}

static int sr501_release (struct inode *node, struct file *filp)
{
    struct my_sr501_dev *p_sr501 = filp->private_data;

    /* 清理在 open 中设置的私有数据 */
    filp->private_data = NULL;
    
    printk("Device '%s' released.\n", p_sr501->name);
    return 0;
}

static int sr501_fasync (int fd, struct file *filp, int on)
{
    int retval;
    struct my_sr501_dev *p_sr501 = filp->private_data;
	retval = fasync_helper(fd, filp, on, &p_sr501->sr501_fasync);   // 注册异步通知
	if (retval < 0)
		return retval;

	return 0;
}

/* operations结构体：为应用层提供驱动接口 */
static struct file_operations my_dev_ops = {
	.owner		=	THIS_MODULE,
	.open 		= 	sr501_open,
    .read       =   sr501_read,
	.release 	=	sr501_release,
	.fasync     =   sr501_fasync,
};

static int my_probe(struct platform_device *pdev)
{
    struct device *dev;       // 获取platform_device的device结构体
    struct device_node *np;                 // 获取设备树节点（硬件信息）
    // struct my_sr501_dev *p_sr501;          // 设备私有数据结构指针
    const char *device_name;               // 保存设备名称
    int ret;                               // 函数调用返回值
    // int minor_num = -1;                    // 未使用，删除
    // int i;                                // 未使用，删除

    dev = &pdev->dev;
    np = dev->of_node;

    /* 步骤 1: 为设备私有数据结构分配内存 */
    p_sr501 = kzalloc(sizeof(struct my_sr501_dev), GFP_KERNEL); // 为每一个物理设备分配私有数据结构
    if (!p_sr501)
    {
        printk("kzalloc for device failed!\r\n");
        return -ENOMEM;
    }

    printk("led driver and device was matched!\r\n");
    
    /* 步骤 2: 从设备树节点中解析硬件信息 */
    ret = of_property_read_string(np, "my_name", &device_name); //将np节点中的my_name属性值读出，存入device_name
    if (ret < 0) {
        printk("Property 'my_name' not found in device tree\n");
        goto err_free_mem; // 跳转到错误处理标签
    }
    strncpy(p_sr501->name, device_name, sizeof(p_sr501->name) - 1);//将设备树中读取到的设备名称拷贝到私有数据结构name，sizeof(p_sr501->name) - 1是防止溢出

    /* 步骤 3：获取GPIO控制权，获取GPIO中断号并注册中断*/
    p_sr501->gpiod = gpiod_get(dev, device_name, GPIOD_IN); // 获取GPIO，设置为输出模式，初始输出低电平
    if (IS_ERR(p_sr501->gpiod)) {
        ret = PTR_ERR(p_sr501->gpiod);
        printk("Failed to get GPIO, error code: %d\n", ret);
        goto err_free_mem; // 跳转到错误处理标签
    }


    p_sr501->gpio_num = desc_to_gpio(p_sr501->gpiod); // 获取GPIO编号
    p_sr501->irq = gpio_to_irq(p_sr501->gpio_num); // 获取GPIO对应的中断号
    
    ret = request_irq(p_sr501->irq, sr501_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, p_sr501->name, NULL);//中断注册函数

    /* 初始化定时器（兼容旧内核） */
    init_timer(&p_sr501->sr501_timer);
    p_sr501->sr501_timer.function = sr501_timer_callback;
    p_sr501->sr501_timer.data = (unsigned long)p_sr501;
    add_timer(&p_sr501->sr501_timer);

    /* 步骤 4：在全局数组中分配次设备号，并完成字符设备注册 */
    cdev_init(&p_sr501->cdev, &my_dev_ops); // 初始化字符设备
    p_sr501->cdev.owner = THIS_MODULE;      // 标识该字符设备属于哪个内核模块，不是必须的

    p_sr501->minor = 1;   //分配次设备号

    my_device_num = MKDEV(major, p_sr501->minor); // 重新使用my_device_num，合成完整的设备号
    ret = cdev_add(&p_sr501->cdev, my_device_num, 1);
    if (ret < 0) {
        printk("Failed to add cdev for %s\n", p_sr501->name);
        return ret;
    }

    /* 步骤5：创建设备节点 */
    p_sr501->device = device_create(my_dev_class, NULL, my_device_num, NULL, p_sr501->name); // 创建设备节点，设备名从设备树读取
    if (IS_ERR(p_sr501->device)) {
        ret = PTR_ERR(p_sr501->device);
        printk("Failed to create device for %s, error code: %d\n", p_sr501->name, ret);
        cdev_del(&p_sr501->cdev); // 记得释放资源
        return ret;
    }

    platform_set_drvdata(pdev, p_sr501); // 绑定pdev和私有数据结构指针
    return ret;

err_free_mem:
    // 释放分配的内存资源
    kfree(p_sr501); // 如果有分配内存，需释放
    return ret;
}

static int my_remove(struct platform_device *pdev)
{
    // 步骤1：获取设备私有数据结构指针
    struct my_sr501_dev *p_sr501 = platform_get_drvdata(pdev);
    if (!p_sr501)
        return -ENODEV;

    // 步骤2. 销毁设备节点，从内核中删除字符设备实例，注销类
    device_destroy(my_dev_class, MKDEV(major, p_sr501->minor));
    cdev_del(&p_sr501->cdev);

    // 步骤3. 释放 GPIO 描述符
    if (p_sr501->gpiod)
        gpiod_put(p_sr501->gpiod);

    // 4. 注销中断，删除定时器
    free_irq(p_sr501->irq, NULL);
    del_timer(&p_sr501->sr501_timer);    // 兼容旧内核API

    // 5. 释放设备结构体内存
    kfree(p_sr501);

    return 0;
}

/*
 * 步骤五：驱动的注册与注销
 */

// 定义驱动与设备树节点的匹配表
static struct of_device_id my_dev_match[] = {
    {.compatible = "hc-sr501"},
    {},
};

static struct platform_driver dev_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "sr501_dev",
        .of_match_table = my_dev_match,
    },
};

// 注册驱动
static int dev_init(void)
{
    int ret;

    /* 1.申请主设备号 */
    ret = alloc_chrdev_region(&my_device_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk("Failed to allocate char device region\n");
        return ret;
    }
    major = MAJOR(my_device_num);         // 获取主设备号

    /* 2. 创建设备类 */
    my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(my_dev_class))
    {
        printk("class_create failed\n");
        unregister_chrdev_region(my_device_num, 1); // 补充释放
        return PTR_ERR(my_dev_class); // 返回标准错误码
    }

    /* 3. 注册 platform 驱动 */
    return platform_driver_register(&dev_driver);
}

// 注销驱动
static void dev_exit(void)
{
    // 注销 platform 驱动
    platform_driver_unregister(&dev_driver);

    // 删除设备类
    class_destroy(my_dev_class);

    // 注销设备号
    unregister_chrdev_region(my_device_num, 1);
};

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
