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
#include <linux/delay.h>
#include <linux/timekeeping.h>
#include <linux/wait.h>
#include <linux/irqflags.h>
#include <linux/cdev.h>

#define DEVICE_NAME	"dht11"

struct dht11_dev {
	struct cdev cdev;		/* cdev 	*/
	struct device *device;	/* 设备 	 */
    char name[32];           // 从设备树读取的设备名，用于创建设备节点
    int minor;				/* 次设备号 */
    struct gpio_desc *dht11_gpio; // DHT11使用的GPIO
};
static struct dht11_dev *dht11dev;

static int major;           // 该驱动的主设备号
static dev_t my_device_num; // 存设备号
static struct class *my_dev_class; // 设备类

unsigned char data[5] = {0};

static int dht11_open (struct inode *node, struct file *filp)
{
    return 0;
}

static int dht11_read_data(void)
{
    int i, j;
    unsigned char checksum;
    memset(data, 0, sizeof(data)); // 读取前清零
    /* 主机发送复位信号 */
    gpiod_direction_output(dht11dev->dht11_gpio, 0); // 拉低总线，至少18ms
    mdelay(18);
    gpiod_set_value(dht11dev->dht11_gpio, 1); // 拉高总线，20~40us
    udelay(40);

    /* 检测DHT11的响应信号 */
    gpiod_direction_input(dht11dev->dht11_gpio); // 设置为输入
    if(gpiod_get_value(dht11dev->dht11_gpio)) // DHT11拉低总线，80us
    {
        printk(KERN_ERR "DHT11 sensor not responding\n");
        return 1;   
    }
    udelay(80);
    if(!gpiod_get_value(dht11dev->dht11_gpio)) //
    {
        printk(KERN_ERR "DHT11 sensor not responding\n");
        return 1;   
    }
    udelay(80);
    /* DHT11已经准备好，读取40位数据 */

    for(i = 0; i < 5; i++){
        for(j=7; j>=0; j--){
            while(!gpiod_get_value(dht11dev->dht11_gpio)); // 等待数据位起始，50us低电平
            udelay(30); // 延时30us
            if(gpiod_get_value(dht11dev->dht11_gpio)) // 高电平持续时间决定数据位0或1,高电平持续 26~28us 表示数据“0”；持续 70us 表示数据“1”。
                data[i] |= (1 << j); // 读到1
            while(gpiod_get_value(dht11dev->dht11_gpio)); // 等待数据位结束
        }
    }

    /* 数据位校验 */
    checksum = data[0] + data[1] + data[2] + data[3];
    if(checksum != data[4]){
        printk(KERN_ERR "DHT11 data checksum error\n");
        return 1;
    }

    return 0;
}

static ssize_t dht11_read (struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    /* 关闭中断形成临界区 */
    int res;

    local_irq_disable();
    res = dht11_read_data();
    local_irq_enable(); // 恢复中断

    if(res) return -EIO; // 读取数据失败，返回I/O错误

    res = copy_to_user(buf, data, sizeof(data));
    if(res) return -EFAULT; // 访问用户空间失败，返回错误
    return sizeof(data);
}

static int dht11_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations my_dev_ops = {
    .owner	= 	THIS_MODULE,
    .open 	= 	dht11_open,
    .read 	= 	dht11_read,
    .release =	dht11_release,
};

static const struct of_device_id dht11_dt_match[] = {
    { .compatible = "hc-dht11" },
    { }, 
};

static int dht11_platform_probe(struct platform_device *pdev)
{
    int ret;
    const char *dev_name;

    /* 步骤 1: 为设备私有数据结构分配内存 */
    dht11dev = kzalloc(sizeof(*dht11dev), GFP_KERNEL); // 为每一个物理设备分配私有数据结构
    if (!dht11dev)
    {
        printk("kzalloc for device failed!\r\n");
        return -ENOMEM;
    }

    printk("dht11 driver and device was matched!\r\n");

    /* 步骤 2: 从设备树节点中解析硬件信息 */
    ret = of_property_read_string(pdev->dev.of_node, "my_name", &dev_name); //将np节点中的my_name属性值读出，存入device_name
    if (ret < 0) {
        printk("Property 'my_name' not found in device tree\n");
        goto err_free_mem; // 跳转到错误处理标签
    }
    strncpy(dht11dev->name, dev_name, sizeof(dht11dev->name));

    /* 步骤 3：在全局数组中分配次设备号，并完成字符设备注册 */
    cdev_init(&dht11dev->cdev, &my_dev_ops); // 初始化字符设备
    dht11dev->cdev.owner = THIS_MODULE;      // 标识该字符设备属于哪个内核模块，不是必须的

    dht11dev->minor = 0;   //分配次设备号
    my_device_num = MKDEV(major, dht11dev->minor); // 重新使用my_device_num，合成完整的设备号

    ret = cdev_add(&dht11dev->cdev, my_device_num, 1); // 注册字符设备
    if (ret < 0) {
        printk("Failed to add cdev for %s\n", dht11dev->name);
        goto err_free_mem;
    }

    /* 步骤 4： 创建设备节点 */
    dht11dev->device = device_create(my_dev_class, NULL, my_device_num, NULL, dht11dev->name); // 创建设备节点，设备名从设备
    if (IS_ERR(dht11dev->device)) {
        printk("device_create for %s failed!\r\n", dht11dev->name);
        cdev_del(&dht11dev->cdev);
        goto err_free_mem;
    }
    
    dht11dev->dht11_gpio = devm_gpiod_get(&pdev->dev, "dht11", GPIOD_OUT_HIGH);
    if (IS_ERR(dht11dev->dht11_gpio)) {
        ret = PTR_ERR(dht11dev->dht11_gpio);
        printk("Failed to get dht11 GPIO\n");
        device_destroy(my_dev_class, MKDEV(major, dht11dev->minor));
        cdev_del(&dht11dev->cdev);
        goto err_free_mem;
    }
    return ret;
err_free_mem:
    kfree(dht11dev);
    return ret;
}

static int dht11_platform_remove(struct platform_device *pdev)
{
    // 销毁设备节点，从内核中删除字符设备实例
    device_destroy(my_dev_class, MKDEV(major, dht11dev->minor));
    cdev_del(&dht11dev->cdev);

    kfree(dht11dev);
    return 0;
}

static struct platform_driver dht11_platform_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = "dht11",
        .of_match_table	= dht11_dt_match,
    },
    .probe = dht11_platform_probe,
    .remove = dht11_platform_remove,
};

static int __init dht11_init(void)
{
    int ret;
    /* 申请主设备号 */
    ret = alloc_chrdev_region(&my_device_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk("Failed to allocate char device region\n");
        return ret;
    }
    major = MAJOR(my_device_num);         // 获取主设备号

    /* 创建设备类 */
    my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(my_dev_class))
    {
        printk("class_create failed\n");
        unregister_chrdev_region(my_device_num, 1); // 补充释放
        return PTR_ERR(my_dev_class); // 返回标准错误码
    }

    /* 注册platform驱动 */
    return platform_driver_register(&dht11_platform_driver);

}

static void __exit dht11_exit(void)
{
    // 注销设备号
    unregister_chrdev_region(my_device_num, 1);
    // 删除设备类
    class_destroy(my_dev_class);
    /* 注销platform驱动 */
    platform_driver_unregister(&dht11_platform_driver);
}

module_init(dht11_init);
module_exit(dht11_exit);
MODULE_LICENSE("GPL");

