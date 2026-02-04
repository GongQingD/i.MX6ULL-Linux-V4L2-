#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define DEVICE_CNT	1
#define DEVICE_NAME	"ap3216c"

struct ap3216c_dev {
	struct cdev cdev;		/* cdev 	*/
	struct device *device;	/* 设备 	 */
	struct device_node	*nd; /* 设备节点 */
    char name[32];           // 从设备树读取的设备名，用于创建设备节点
	void *private_data;	/* 私有数据 */
	unsigned short ir, als, ps;		/* 三个光传感器数据 */
    int minor;				/* 次设备号 */
};
struct ap3216c_dev *ap3216cdev;

static int major;           // 该驱动的主设备号
static dev_t my_device_num; // 存设备号
static struct class *my_dev_class; // 设备类

static int ap3216_open (struct inode *node, struct file *filp)
{   
    filp->private_data = ap3216cdev; // 设置私有数据
    return 0;
}

static ssize_t ap3216_read (struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    int var, err;
    char data[6];
    struct ap3216c_dev *dev = filp->private_data;

    /* 允许用户传入 >=6 的长度，若小于6认为参数非法 */
    if (size < 6)
        return -EINVAL;

    /* read IR */
    var = i2c_smbus_read_word_data(dev->private_data, 0x0a);
    if (var < 0)
        return -EIO;
    data[0] = (var >> 8) & 0xff;
    data[1] = var & 0xff;

    /* read ALS */
    var = i2c_smbus_read_word_data(dev->private_data, 0x0c);
    if (var < 0)
        return -EIO;
    data[2] = (var >> 8) & 0xff;
    data[3] = var & 0xff;

    /* read PS */
    var = i2c_smbus_read_word_data(dev->private_data, 0x0e);
    if (var < 0)
        return -EIO;
    data[4] = (var >> 8) & 0xff;
    data[5] = var & 0xff;

    err = copy_to_user(buf, data, 6);
    if (err)
        return -EFAULT;
    return 6;
}

static int ap3216_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations my_dev_ops = {
	.owner	= 	THIS_MODULE,
	.open 	= 	ap3216_open,
	.read 	= 	ap3216_read,
    .release =	ap3216_release,
};

static const struct of_device_id ap3216_dt_match[] = {
	{ .compatible = "alientek,ap3216c" },
	{ }, 
};

static int ap3216_i2c_probe(struct i2c_client *client, const struct i2c_device_id * i2c_id)
{
    struct device *dev;       // 获取platform_device的device结构体
    struct device_node *np;                 // 获取设备树节点（硬件信息）
    const char *device_name;               // 保存设备名称
    int ret;                               // 函数调用返回值

    dev = &client->dev;                    // 获取i2c_client的device结构体
    np = dev->of_node;                     // 获取设备节点

    /* 步骤 1: 为设备私有数据结构分配内存 */
    ap3216cdev = kzalloc(sizeof(struct ap3216c_dev), GFP_KERNEL); // 为每一个物理设备分配私有数据结构
    if (!ap3216cdev)
    {
        printk("kzalloc for device failed!\r\n");
        return -ENOMEM;
    }

    printk("ap3216c driver and device was matched!\r\n");

    /* 步骤 2: 从设备树节点中解析硬件信息 */
    ret = of_property_read_string(np, "my_name", &device_name); //将np节点中的my_name属性值读出，存入device_name
    if (ret < 0) {
        printk("Property 'my_name' not found in device tree\n");
        goto err_free_mem; // 跳转到错误处理标签
    }
    strncpy(ap3216cdev->name, device_name, sizeof(ap3216cdev->name) - 1);//将设备树中读取到的设备名称拷贝到私有数据结构name，sizeof(ap3216cdev->name) - 1是防止溢出

    /* 步骤 3：在全局数组中分配次设备号，并完成字符设备注册 */
    cdev_init(&ap3216cdev->cdev, &my_dev_ops); // 初始化字符设备
    ap3216cdev->cdev.owner = THIS_MODULE;      // 标识该字符设备属于哪个内核模块，不是必须的

    ap3216cdev->minor = 0;   //分配次设备号

    my_device_num = MKDEV(major, ap3216cdev->minor); // 重新使用my_device_num，合成完整的设备号
    ret = cdev_add(&ap3216cdev->cdev, my_device_num, 1);
    if (ret < 0) {
        printk("Failed to add cdev for %s\n", ap3216cdev->name);
        return ret;
    }

    /* 步骤5：创建设备节点 */
    ap3216cdev->device = device_create(my_dev_class, NULL, my_device_num, NULL, ap3216cdev->name); // 创建设备节点，设备名从设备树读取
    if (IS_ERR(ap3216cdev->device)) {
        ret = PTR_ERR(ap3216cdev->device);
        printk("Failed to create device for %s, error code: %d\n", ap3216cdev->name, ret);
        cdev_del(&ap3216cdev->cdev); // 记得释放资源
        return ret;
    }

    ap3216cdev->private_data = client; // 将i2c_client结构体指针存入私有数据，以便在其他函数中使用

    /* 初始化芯片：复位并配置工作模式（移自 open） */
    i2c_smbus_write_byte_data(ap3216cdev->private_data, 0x00, 0x04); // 复位
    mdelay(15);                                                      // 等待复位
    i2c_smbus_write_byte_data(ap3216cdev->private_data, 0x00, 0x03); // 使能 ALS/PS/IR

    return 0;
err_free_mem:
    kfree(ap3216cdev); // 释放内存
    return ret;
};

static int ap3216_i2c_remove(struct i2c_client *client)
{
    // 销毁设备节点，从内核中删除字符设备实例
    device_destroy(my_dev_class, MKDEV(major, ap3216cdev->minor));
    cdev_del(&ap3216cdev->cdev);

    kfree(ap3216cdev); // 释放内存

    return 0;
};

/* 传统匹配方式ID列表 */
static const struct i2c_device_id ap3216c_id[] = {
	{"alientek,ap3216c", 0},  
	{}
};

static struct i2c_driver ap3216_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ap3216c_drv",
		.of_match_table	= ap3216_dt_match,
	},
	.probe = ap3216_i2c_probe,
	.remove = ap3216_i2c_remove,
    .id_table = ap3216c_id,
};

static int __init ap3216c_init(void)
{
    int ret;

    /* 1.申请主设备号 */
    ret = alloc_chrdev_region(&my_device_num, 0, DEVICE_CNT, DEVICE_NAME);
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

    /* 3. 注册i2c驱动 */
    return i2c_add_driver(&ap3216_i2c_driver);
};

static void __exit ap3216c_exit(void)
{
    // 注销 i2c 驱动
    i2c_del_driver(&ap3216_i2c_driver);

    // 删除设备类
    class_destroy(my_dev_class);

    // 注销设备号
    unregister_chrdev_region(my_device_num, 1);
};

module_init(ap3216c_init);
module_exit(ap3216c_exit);
MODULE_LICENSE("GPL");


