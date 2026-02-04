#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>  // 新增：为了使用 kzalloc 和 kfree
#include <linux/mutex.h> // 新增：为了使用互斥锁
#include <linux/gpio/consumer.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define MAX_DEVICES 10 // 该驱动支持的最大设备数
#define LEDDEV_NAME "led_dev"
#define LEDDEV_CNT MAX_DEVICES
/*
 * 步骤一：定义设备私有数据结构
 */
struct my_led_dev
{
    struct gpio_desc *gpiod; // 该设备对应的GPIO描述符
    int minor;               // 该设备对应的次设备号
    struct cdev cdev;			
    char name[32];           // 从设备树读取的设备名，用于创建设备节点
    struct device *device;   // 该设备对应的device结构体指针
};
/*
 * 步骤二：定义全局变量
 */
static int major;           // 该驱动的主设备号
static dev_t my_device_num; // 存设备号
static struct class *my_dev_class;

static struct my_led_dev *g_led_dev[MAX_DEVICES]; // 每一个设备都有专属的私有数据结构
static DEFINE_MUTEX(g_led_dev_lock);              // 保护g_led_dev数组的互斥锁

/*
 * 步骤三：实现文件操作函数file_operations
 */


 /**
 * my_drv_read - 当用户空间从设备文件读取数据时被调用
 *
 * @filp:   指向文件的file结构
 * @buf:    用户空间缓冲区指针，用于把数据拷贝到用户空间
 * @size:   要读取的数据长度
 * @offset: 文件偏移量，通常只允许从 0 位置读取一次。
 *
 * 读取GPIO的当前电平，并将其拷贝到用户空间（把设备的数据拷贝到buf）。
 */
static ssize_t my_drv_read(struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    char status;

    struct my_led_dev *p_led = filp->private_data;
    if (p_led == NULL || p_led->gpiod == NULL) {
        return -EFAULT;
    };
    if(*offset > 0) {
        return 0; // 只允许从文件开头读取
    }
    
    status = gpiod_get_value(p_led->gpiod); // 读取GPIO电平
    // 将电平状态status拷贝到buf中
    if (copy_to_user(buf, &status, 1)) {
        pr_err("Failed to copy data to user\n");
        return -EFAULT;
    }

    *offset += 1; // 更新文件偏移量

    return 1; // 返回成功读取的字节数
}

/**
 * my_drv_write - 当用户空间向设备文件写入数据时被调用
 *
 * @filp:  指向文件的file结构
 * @buf:   指向用户空间数据的指针
 * @size:  要写入的数据长度
 * @offset: 文件偏移量
 *
 * 从用户空间拷贝数据（把buf中的数据写入设备），并根据数据内容控制GPIO电平。
 */
static ssize_t my_drv_write(struct file *filp, const char __user *buf, size_t size, loff_t *offset)
{
    char status;
    
    /* 直接从 filp->private_data 中获取设备上下文，非常高效 */
    struct my_led_dev *p_led = filp->private_data;
    /* 检查上下文是否有效 */
    if (p_led == NULL || p_led->gpiod == NULL) {
        return -EFAULT;
    }

    /* 从用户空间安全地拷贝1个字节的数据 */
    if (copy_from_user(&status, buf, 1)) {
        pr_err("Failed to copy data from user\n");
        return -EFAULT;
    }

    /* 根据拷贝来的数据设置GPIO的值 (非0为高电平，0为低电平) */
    gpiod_set_value(p_led->gpiod, status ? 1 : 0);

    return 1; // 返回成功写入的字节数
}

/**
 * my_drv_open - 当用户空间打开设备文件时被调用
 *
 * @node: 指向文件的inode结构
 * @filp: 指向内核为此次打开操作创建的file结构
 *
 * 这个函数的核心任务是：
 * 1. 根据被打开文件的次设备号，找到我们驱动中对应的设备实例(struct my_led_dev)。
 * 2. 将找到的设备实例指针存放到 filp->private_data 中。
 * 这样做的好处是，后续的 read/write 等函数可以直接从 filp->private_data
 * 获取设备信息，无需重复查找，大大提高了效率。
 * 
 * 每次 open 都会创建一个新的 file 结构体，因此 filp->private_data
 */
static int my_drv_open(struct inode *node, struct file *filp)
{
    int minor = iminor(node);       // 获取次设备号
    struct my_led_dev *p_led = NULL;

    /* 加锁，因为 g_led_devs 是可能被并发访问的全局数组 */
    mutex_lock(&g_led_dev_lock);

    p_led = g_led_dev[minor]; // 从全局数组中获取设备指针

    if (p_led == NULL) {
        /* 如果指针为空，说明对应的设备可能已被移除 */
        pr_err("No device for minor number %d\n", minor);
        mutex_unlock(&g_led_dev_lock);
        return -ENODEV; // 返回 "No such device" 错误
    }

    /* * 【核心】将设备私有数据指针保存到文件的私有数据中。
     * 这是建立驱动与文件句柄上下文关联的关键一步。
     */
    filp->private_data = p_led;

    mutex_unlock(&g_led_dev_lock);
    
    printk("Device '%s' opened.\n", p_led->name);
    return 0;
}

/**
 * my_drv_release - 当用户空间关闭设备文件时被调用
 *
 * @node: 指向文件的inode结构
 * @filp: 指向文件的file结构
 *
 * 它的主要任务是清理在 open 函数中所做的设置。
 */
static int my_drv_release(struct inode *node, struct file *filp)
{
    struct my_led_dev *p_led = filp->private_data;

    /* 清理在 open 中设置的私有数据 */
    filp->private_data = NULL;
    
    printk("Device '%s' released.\n", p_led->name);
    return 0;
}

/* operations结构体：为应用层提供驱动接口 */
static struct file_operations my_dev_ops = {
    .owner = THIS_MODULE,
    .read = my_drv_read,
    .write = my_drv_write,
    .open = my_drv_open,
    .release = my_drv_release,
};

/*
 * platform_driver的probe函数
 * 函数的输入参数*pdev是平台总线传递给probe的设备对象，代表一个物理信息，包含了设备树信息、资源、设备号等，是内核管理设备的标准结构体。
 * 但是probe函数使用不到这么多数据，而且还需要增加一些变量，所以使用了新的结构体p_led来保存pdev的部分信息和新增变量，这就是我们的私有数据结构。
 */
static int my_probe(struct platform_device *pdev)
{
    

    struct device *dev;       // 获取platform_device的device结构体
    struct device_node *np; // 获取设备树节点（硬件信息）
    struct my_led_dev *p_led;              // 设备私有数据结构指针
    const char *device_name;               // 保存设备名称
    int ret;                               // 函数调用返回值
    int minor_num = -1;                    // 记录分配到的次设备号
    int i;

    dev = &pdev->dev;
    np = dev->of_node;
    
    /*  步骤 1: 为设备私有数据结构分配内存 */
    p_led = kzalloc(sizeof(struct my_led_dev), GFP_KERNEL); // 为每一个物理设备分配私有数据结构
    if (!p_led)
    {
        printk("kzalloc for led device failed!\r\n");
        return -ENOMEM;
    }

    printk("led driver and device was matched!\r\n");
    
    /* 步骤 2: 从设备树节点中解析硬件信息 */
    ret = of_property_read_string(np, "my_name", &device_name); //将np节点中的my_name属性值读出，存入device_name
    if (ret < 0) {
        printk("Property 'my_name' not found in device tree\n");
        goto err_free_mem; // 跳转到错误处理标签
    }
    strncpy(p_led->name, device_name, sizeof(p_led->name) - 1);//将设备树中读取到的设备名称拷贝到私有数据结构name，sizeof(p_led->name) - 1是防止溢出

    /* 步骤 3：获取GPIO控制权*/
    p_led->gpiod = gpiod_get(dev, device_name, GPIOD_OUT_LOW); // 获取GPIO，设置为输出模式，初始输出低电平
    if (IS_ERR(p_led->gpiod)) {
        ret = PTR_ERR(p_led->gpiod);
        printk("Failed to get GPIO, error code: %d\n", ret);
        goto err_free_mem; // 跳转到错误处理标签
    }

    /* 步骤 4：在全局数组中分配次设备号，并完成字符设备注册 */
    cdev_init(&p_led->cdev, &my_dev_ops); // 初始化字符设备
    p_led->cdev.owner = THIS_MODULE;      // 标识该字符设备属于哪个内核模块，不是必须的
    
    //这是一个临界区，必须用互斥锁保护，防止多个设备同时抢占同一个号。

    mutex_lock(&g_led_dev_lock);            //进入临界区
    // 找到数组的空为，并分配次设备号
    for (i = 0; i < MAX_DEVICES; i++) {
        if (g_led_dev[i] == NULL) {
            minor_num = i;
            break;
        }
    }
    // 错误检查
    if (minor_num == -1) {
        ret = -ENOSPC; // No space left on device
        printk("No available minor number for %s\n", p_led->name);
        mutex_unlock(&g_led_dev_lock);
        return ret;
    }
    // 将分配的次设备号存入私有数据结构
    p_led->minor = minor_num;
    g_led_dev[p_led->minor] = p_led; // 把新设备的私有数据结构在全局数组中登记
    mutex_unlock(&g_led_dev_lock);      //离开临界区

    /* 步骤5：注册字符设备*/
    my_device_num = MKDEV(major, p_led->minor); // 重新使用my_device_num，合成完整的设备号
    ret = cdev_add(&p_led->cdev, my_device_num, 1);
    if (ret < 0) {
        printk("Failed to add cdev for %s\n", p_led->name);
        return ret;
    }

    /* 步骤6：创建设备节点 */
    p_led->device = device_create(my_dev_class, NULL, my_device_num, NULL, p_led->name); // 创建设备节点，设备名从设备树读取
    if (IS_ERR(p_led->device)) {
        ret = PTR_ERR(p_led->device);
        printk("Failed to create device for %s, error code: %d\n", p_led->name, ret);
        cdev_del(&p_led->cdev); // 记得释放资源
        return ret;
    }

    /* 步骤7：将私有数据指针 "贴" 到 platform_device 上。
     * 1. 这样，在 remove 函数中，我们就可以通过 platform_get_drvdata() 轻松取回它，
     * 而无需进行任何搜索。
     * 2. 每一次使用probe，都会构建一个新的p_led，并赋予不同的数据空间，因此不会冲突。
     * 3. platform_get_drvdata(pdev)返回的是void指针，指向的是之间绑定的数据空间地址，需要强制转换为struct my_led_dev指针。
     * */
    platform_set_drvdata(pdev, p_led); // 绑定pdev和私有数据结构指针
    return ret;

err_free_mem:
    kfree(p_led); // 在这里统一释放内存
    return ret;
};

static int my_remove(struct platform_device *pdev)
{
    // 步骤1：获取设备私有数据结构指针
    struct my_led_dev *p_led = platform_get_drvdata(pdev);
    if (!p_led)
        return -ENODEV;

    // 步骤2. 销毁设备节点，从内核中删除字符设备实例
    device_destroy(my_dev_class, MKDEV(major, p_led->minor));
    cdev_del(&p_led->cdev);

    // 步骤3. 释放 GPIO 描述符
    if (p_led->gpiod)
        gpiod_put(p_led->gpiod);

    // 步骤4. 从全局数组移除该设备
    mutex_lock(&g_led_dev_lock);
    g_led_dev[p_led->minor] = NULL;
    mutex_unlock(&g_led_dev_lock);

    // 5. 释放设备结构体内存
    kfree(p_led);

    return 0;
};

/*
 * 步骤五：驱动的注册与注销
 */

// 定义驱动与设备树节点的匹配表
static struct of_device_id my_dev_match[] = {
    {.compatible = "hc-led"},
    {},
};

static struct platform_driver dev_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "led_dev",
        .of_match_table = my_dev_match,
    },
};

// 注册驱动
static int dev_init(void)
{
    int ret;

    /* 1.申请主设备号 */
    ret = alloc_chrdev_region(&my_device_num, 0, LEDDEV_CNT, LEDDEV_NAME);
    if (ret < 0) {
        printk("Failed to allocate char device region\n");
        return ret;
    }
    major = MAJOR(my_device_num);         // 获取主设备号

    /* 2. 创建设备类 */
    my_dev_class = class_create(THIS_MODULE, LEDDEV_NAME);
    if (IS_ERR(my_dev_class))
    {
        printk("class_create failed\n");
        unregister_chrdev_region(my_device_num, LEDDEV_CNT); // 补充释放
        return PTR_ERR(my_dev_class); // 返回标准错误码
    }

    /* 3. 注册 platform 驱动 */
    return platform_driver_register(&dev_driver);
};

// 注销驱动
static void dev_exit(void)
{
    // 注销 platform 驱动
    platform_driver_unregister(&dev_driver);

    // 删除设备类
    class_destroy(my_dev_class);

    // 注销设备号
    unregister_chrdev_region(my_device_num, MAX_DEVICES);
};

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
