#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/cdev.h>

#define DEVICE_NAME "myplatform"
#define BUF_LEN 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");

// Platform driver private data
struct myplatform_dev {
    struct platform_device *pdev;
    void __iomem *regs;
    int irq;
    struct cdev cdev;
    dev_t dev_num;
    char message[BUF_LEN];
    int message_size;
};

// File operations
static int platform_open(struct inode *inode, struct file *filep)
{
    struct myplatform_dev *dev = container_of(inode->i_cdev, 
                                            struct myplatform_dev, cdev);
    filep->private_data = dev;
    
    pr_info("myplatform: Device opened\n");
    return 0;
}

static ssize_t platform_read(struct file *filep, char __user *buffer,
                            size_t len, loff_t *offset)
{
    struct myplatform_dev *dev = filep->private_data;
    int bytes_read = 0;

    if (*offset >= dev->message_size)
        return 0;

    if (*offset + len > dev->message_size)
        len = dev->message_size - *offset;

    bytes_read = len - copy_to_user(buffer, dev->message + *offset, len);
    *offset += bytes_read;

    pr_info("myplatform: Sent %d bytes to user\n", bytes_read);
    return bytes_read;
}

static ssize_t platform_write(struct file *filep, const char __user *buffer,
                             size_t len, loff_t *offset)
{
    struct myplatform_dev *dev = filep->private_data;
    
    if (len > BUF_LEN - 1)
        len = BUF_LEN - 1;

    dev->message_size = len - copy_from_user(dev->message, buffer, len);
    dev->message[dev->message_size] = '\0';

    pr_info("myplatform: Received %d bytes from user\n", dev->message_size);
    return dev->message_size;
}

static int platform_release(struct inode *inode, struct file *filep)
{
    pr_info("myplatform: Device closed\n");
    return 0;
}

static const struct file_operations platform_fops = {
    .owner = THIS_MODULE,
    .open = platform_open,
    .read = platform_read,
    .write = platform_write,
    .release = platform_release,
};

// Platform driver probe function
static int myplatform_probe(struct platform_device *pdev)
{
    struct myplatform_dev *dev;
    struct resource *res;
    int ret;

    pr_info("myplatform: Probing platform device\n");

    // Allocate device structure
    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->pdev = pdev;
    platform_set_drvdata(pdev, dev);

    // Get memory resource (if defined in device tree/platform data)
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res) {
        dev->regs = devm_ioremap_resource(&pdev->dev, res);
        if (IS_ERR(dev->regs))
            return PTR_ERR(dev->regs);
        pr_info("myplatform: Mapped memory at 0x%llx\n", 
                (unsigned long long)res->start);
    }

    // Get IRQ resource (if defined)
    dev->irq = platform_get_irq(pdev, 0);
    if (dev->irq > 0) {
        pr_info("myplatform: Got IRQ %d\n", dev->irq);
        // You could request the IRQ here with devm_request_irq()
    }

    // Allocate character device
    ret = alloc_chrdev_region(&dev->dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("myplatform: Failed to allocate chrdev region\n");
        return ret;
    }

    // Initialize cdev
    cdev_init(&dev->cdev, &platform_fops);
    dev->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&dev->cdev, dev->dev_num, 1);
    if (ret < 0) {
        pr_err("myplatform: Failed to add cdev\n");
        goto unregister_chrdev;
    }

    pr_info("myplatform: Registered with major %d, minor 0\n", 
            MAJOR(dev->dev_num));
    pr_info("myplatform: Create device: mknod /dev/%s c %d 0\n",
            DEVICE_NAME, MAJOR(dev->dev_num));

    return 0;

unregister_chrdev:
    unregister_chrdev_region(dev->dev_num, 1);
    return ret;
}

// Platform driver remove function
static int myplatform_remove(struct platform_device *pdev)
{
    struct myplatform_dev *dev = platform_get_drvdata(pdev);

    pr_info("myplatform: Removing platform device\n");

    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->dev_num, 1);

    return 0;
}

// Platform driver structure
static const struct of_device_id myplatform_of_match[] = {
    { .compatible = "mycompany,myplatform-device" },
    { }
};
MODULE_DEVICE_TABLE(of, myplatform_of_match);

static struct platform_driver myplatform_driver = {
    .driver = {
        .name = "myplatform",
        .of_match_table = myplatform_of_match,
    },
    .probe = myplatform_probe,
    .remove = myplatform_remove,
};

static struct platform_device *test_pdev;
// Module init/exit
static int __init myplatform_init(void)
{
    int ret;
    
    pr_info("myplatform: Loading platform driver\n");
    
    ret = platform_driver_register(&myplatform_driver);
    if (ret < 0) {
        pr_err("myplatform: Failed to register platform driver\n");
        return ret;
    }

    // Add this: Create test platform device
    test_pdev = platform_device_register_simple("myplatform", -1, NULL, 0);
    if (IS_ERR(test_pdev)) {
        platform_driver_unregister(&myplatform_driver);
        return PTR_ERR(test_pdev);
    }

    return 0;
}

static void __exit myplatform_exit(void)
{
    pr_info("myplatform: Unloading platform driver\n");
    
    // Add this: Unregister test device first
    if (test_pdev)
        platform_device_unregister(test_pdev);
    
    platform_driver_unregister(&myplatform_driver);
}

module_init(myplatform_init);
module_exit(myplatform_exit);