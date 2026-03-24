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
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/gpio/consumer.h>

#include <linux/cdev.h>

#define DEVICE_NAME "sr501"

struct my_sr501_dev
{
    struct gpio_desc *gpiod;
    int minor;
    struct cdev cdev;
    char name[32];
    struct device *device;
    struct timer_list sr501_timer;
    int irq;
    int gpio_num;
    struct fasync_struct *sr501_fasync;
};

static int major;
static dev_t my_device_num;
static struct class *my_dev_class;
static int device_index = 0;

static irqreturn_t sr501_irq_handler(int irq, void *dev_id)
{
    struct my_sr501_dev *p_sr501 = (struct my_sr501_dev *)dev_id;
    int val = gpiod_get_value(p_sr501->gpiod);

    printk("SR501 IRQ triggered, GPIO value: %d\n", val);
    mod_timer(&p_sr501->sr501_timer, jiffies + HZ / 50);
    return IRQ_HANDLED;
}

static void sr501_timer_callback(unsigned long data)
{
    struct my_sr501_dev *p_sr501 = (struct my_sr501_dev *)data;

    kill_fasync(&p_sr501->sr501_fasync, SIGIO, POLL_IN);
}

static int sr501_open(struct inode *node, struct file *filp)
{
    struct my_sr501_dev *p_sr501 = container_of(node->i_cdev, struct my_sr501_dev, cdev);

    filp->private_data = p_sr501;
    return 0;
}

static ssize_t sr501_read(struct file *filp, char __user *buf, size_t size, loff_t *offset)
{
    char status;
    struct my_sr501_dev *p_sr501 = filp->private_data;

    if (p_sr501 == NULL || p_sr501->gpiod == NULL) {
        return -EFAULT;
    }

    if (*offset > 0) {
        return 0;
    }

    status = gpiod_get_value(p_sr501->gpiod);
    if (copy_to_user(buf, &status, 1)) {
        pr_err("Failed to copy data to user\n");
        return -EFAULT;
    }

    return 1;
}

static int sr501_release(struct inode *node, struct file *filp)
{
    struct my_sr501_dev *p_sr501 = filp->private_data;

    filp->private_data = NULL;
    if (p_sr501) {
        printk("Device '%s' released.\n", p_sr501->name);
    }
    return 0;
}

static int sr501_fasync(int fd, struct file *filp, int on)
{
    int retval;
    struct my_sr501_dev *p_sr501 = filp->private_data;

    if (!p_sr501) {
        return -ENODEV;
    }

    retval = fasync_helper(fd, filp, on, &p_sr501->sr501_fasync);
    if (retval < 0) {
        return retval;
    }

    return 0;
}

static struct file_operations my_dev_ops = {
    .owner = THIS_MODULE,
    .open = sr501_open,
    .read = sr501_read,
    .release = sr501_release,
    .fasync = sr501_fasync,
};

static int my_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    struct my_sr501_dev *p_sr501;
    const char *device_name;
    int ret;

    p_sr501 = kzalloc(sizeof(struct my_sr501_dev), GFP_KERNEL);
    if (!p_sr501) {
        printk("kzalloc for device failed!\r\n");
        return -ENOMEM;
    }

    printk("sr501 driver and device was matched!\r\n");

    ret = of_property_read_string(np, "my_name", &device_name);
    if (ret < 0) {
        printk("Property 'my_name' not found in device tree\n");
        goto err_free_mem;
    }
    strncpy(p_sr501->name, device_name, sizeof(p_sr501->name) - 1);

    p_sr501->gpiod = gpiod_get(dev, NULL, GPIOD_IN);
    if (IS_ERR(p_sr501->gpiod)) {
        ret = PTR_ERR(p_sr501->gpiod);
        printk("Failed to get GPIO, error code: %d\n", ret);
        goto err_free_mem;
    }

    p_sr501->gpio_num = desc_to_gpio(p_sr501->gpiod);
    p_sr501->irq = gpio_to_irq(p_sr501->gpio_num);

    ret = request_irq(p_sr501->irq,
                      sr501_irq_handler,
                      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                      p_sr501->name,
                      p_sr501);
    if (ret < 0) {
        printk("Failed to request IRQ for %s\n", p_sr501->name);
        goto err_put_gpio;
    }

    init_timer(&p_sr501->sr501_timer);
    p_sr501->sr501_timer.function = sr501_timer_callback;
    p_sr501->sr501_timer.data = (unsigned long)p_sr501;
    add_timer(&p_sr501->sr501_timer);

    cdev_init(&p_sr501->cdev, &my_dev_ops);
    p_sr501->cdev.owner = THIS_MODULE;

    p_sr501->minor = device_index++;
    my_device_num = MKDEV(major, p_sr501->minor);
    ret = cdev_add(&p_sr501->cdev, my_device_num, 1);
    if (ret < 0) {
        printk("Failed to add cdev for %s\n", p_sr501->name);
        goto err_free_irq;
    }

    p_sr501->device = device_create(my_dev_class, NULL, my_device_num, NULL, p_sr501->name);
    if (IS_ERR(p_sr501->device)) {
        ret = PTR_ERR(p_sr501->device);
        printk("Failed to create device for %s, error code: %d\n", p_sr501->name, ret);
        goto err_del_cdev;
    }

    platform_set_drvdata(pdev, p_sr501);
    printk("SR501 device '%s' (minor %d) registered successfully\n", p_sr501->name, p_sr501->minor);
    return 0;

err_del_cdev:
    cdev_del(&p_sr501->cdev);
err_free_irq:
    del_timer(&p_sr501->sr501_timer);
    free_irq(p_sr501->irq, p_sr501);
err_put_gpio:
    gpiod_put(p_sr501->gpiod);
err_free_mem:
    kfree(p_sr501);
    return ret;
}

static int my_remove(struct platform_device *pdev)
{
    struct my_sr501_dev *p_sr501 = platform_get_drvdata(pdev);

    if (!p_sr501) {
        return -ENODEV;
    }

    printk("Removing SR501 device '%s' (minor %d)\n", p_sr501->name, p_sr501->minor);

    device_destroy(my_dev_class, MKDEV(major, p_sr501->minor));
    cdev_del(&p_sr501->cdev);

    if (p_sr501->gpiod) {
        gpiod_put(p_sr501->gpiod);
    }

    free_irq(p_sr501->irq, p_sr501);
    del_timer_sync(&p_sr501->sr501_timer);
    kfree(p_sr501);

    return 0;
}

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

static int dev_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&my_device_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk("Failed to allocate char device region\n");
        return ret;
    }
    major = MAJOR(my_device_num);

    my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(my_dev_class)) {
        printk("class_create failed\n");
        unregister_chrdev_region(my_device_num, 1);
        return PTR_ERR(my_dev_class);
    }

    ret = platform_driver_register(&dev_driver);
    if (ret < 0) {
        class_destroy(my_dev_class);
        unregister_chrdev_region(my_device_num, 1);
    }

    return ret;
}

static void dev_exit(void)
{
    platform_driver_unregister(&dev_driver);
    class_destroy(my_dev_class);
    unregister_chrdev_region(MKDEV(major, 0), 1);
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
