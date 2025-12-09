#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>

#include "platform.h"

#define MAX_DEVICES 4

/**
 * @brief platform device private data, allocated dynamically when registering devices
 */
struct platform_dev_private_data {
    struct platform_data pdata;
    char*buf;
    dev_t dev_num;
    struct cdev cdev;
};

/**
 * @brief platform driver private data, allocated globally
 */
 struct platform_drv_private_data {
    int total_devices;
    dev_t dev_num_base;
    struct class* class_pcd;
    struct device* device_pcd;
};


struct platform_drv_private_data pdrv_data;


int check_permission(int dev_perm, int acc_mode) {
    if(dev_perm == O_RDWR) {
        return 0;
    }

    if((dev_perm == O_RDONLY) && (acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)) {
        return 0;
    }

    if((dev_perm == O_WRONLY) && (acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)) {
        return 0;
    }

    return -EPERM;
}

loff_t platform_dev_lseek(struct file *filep, loff_t offset, int whence) {
    return 0;
}

ssize_t platform_dev_read(struct file *filep, char __user *buf, size_t count, loff_t *offset) {
    return 0;
}

ssize_t platform_dev_write(struct file *filep, const char __user *buf, size_t count, loff_t *offset) {
    return -ENOMEM;
}

int platform_dev_open(struct inode *inodep, struct file *filep) {
    pr_info("platform_dev_open: Device opened\n");
    return 0;
}

int platform_dev_release(struct inode *inodep, struct file *filep) {
    pr_info("platform_dev_release: Device released\n");
    return 0;
}

static struct file_operations fops = {
    .open = platform_dev_open,
    .release = platform_dev_release,
    .read = platform_dev_read,
    .write = platform_dev_write,
    .llseek = platform_dev_lseek,
    .owner = THIS_MODULE
};

/**
 * @brief called when matched platform dev is found
 */
int platform_drv_probe(struct platform_device*pdev) {
    struct platform_dev_private_data* dev_data;
    struct platform_data* pdev_data;
    int ret = 0;
    int config_entry = 0;

    // pdev_data = pdev->dev.platform_data;
    pdev_data = (struct platform_data*) dev_get_platdata(&pdev->dev);
    if(!pdev_data) {
        pr_err("No platform data available");
        return -EINVAL;
    }

    // dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
    dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
    if(!dev_data) {
        pr_err("Cannot allocate memory for dev data");
        return -ENOMEM;
    }

    dev_data->pdata.size = pdev_data->size;
    dev_data->pdata.perm = pdev_data->perm;
    dev_data->pdata.serial_number = pdev_data->serial_number;

    pr_info("probe cb: Device detected");
    pr_info("sn: %s", dev_data->pdata.serial_number);
    pr_info("perm: %x", dev_data->pdata.perm);
    pr_info("size: %d", dev_data->pdata.size);

    config_entry = pdev->id_entry->driver_data;
    pr_info("Using config index: %d", config_entry);
    pr_info("config 1: %d", pdev_config_table[config_entry].config_1);
    pr_info("config 2: %d", pdev_config_table[config_entry].config_2);


    // dev_data->buf = kzalloc(dev_data->pdata.size, GFP_KERNEL);
    dev_data->buf = devm_kzalloc(&pdev->dev, dev_data->pdata.size, GFP_KERNEL);

    if(!dev_data->buf) {
        pr_err("Cannot allocate memory for dev buffer");
        ret = -ENOMEM;
        goto dev_data_free;
    }

    dev_data->dev_num = pdrv_data.dev_num_base + pdev->id;

    cdev_init(&dev_data->cdev, &fops);
    dev_data->cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
    if(ret < 0) {
        pr_err("`cdev_add failed`");
        goto buffer_free;
    }

    pdrv_data.device_pcd = device_create(pdrv_data.class_pcd, NULL, dev_data->dev_num, NULL, "pdev-%d", pdev->id);
    if(IS_ERR(pdrv_data.device_pcd)) {
        pr_err("Cannot create device");
        ret = PTR_ERR(pdrv_data.device_pcd);
        goto cdev_del;
    }

    pr_info("successful probe()!");
    dev_set_drvdata(&pdev->dev, dev_data);
    pdrv_data.total_devices++;
    return 0;

cdev_del:
    cdev_del(&dev_data->cdev);

buffer_free:
    // kfree(dev_data->buf);    // no need to do this if using resource managed kernel APIs
    devm_kfree(&pdev->dev, dev_data->buf);

dev_data_free:
    // kfree(dev_data);          // no need to do this if using resource managed kernel APIs
    devm_kfree(&pdev->dev, dev_data);
    return ret;
}

/**
 * @brief called when device is removed
 */
int platform_drv_remove(struct platform_device*pdev) {
    struct platform_dev_private_data*dev_data = dev_get_drvdata(&pdev->dev);
    device_destroy(pdrv_data.class_pcd, dev_data->dev_num);
    cdev_del(&dev_data->cdev);
    // no need to do this if using resource managed kernel APIs
    // kfree(dev_data->buf);
    // kfree(dev_data);
    pdrv_data.total_devices--;
    pr_info("remove cb: device removed");
    return 0;
}

struct platform_device_id pdevs_id[] = {
    [0] = {.name = "pdev-A1x", .driver_data = PDEV_A1X}, 
    [1] = {.name = "pdev-B1x", .driver_data = PDEV_B1X},
    {}          // array must be null terminated!
};


/**
 * @brief needs to define probe and remove cb
 * @brief also device_driver struct
 */
struct platform_driver platform_drv = {
    .probe = platform_drv_probe,
    .remove = platform_drv_remove,
    .id_table = pdevs_id,
    .driver = {
        .name = "pseudo-char-dev"
    }
};


static int __init platform_drv_init(void) {

    int ret;
    ret = alloc_chrdev_region(&pdrv_data.dev_num_base, 0, MAX_DEVICES, "pdrv");
    if(ret < 0) {
        pr_err("Alloc driver failed\n");
        return ret;
    }

    pdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
    if(IS_ERR(pdrv_data.class_pcd)) {
        pr_err("Platform driver class creation failed");
        ret = PTR_ERR(pdrv_data.class_pcd);
        unregister_chrdev_region(pdrv_data.dev_num_base, MAX_DEVICES);
        return ret;
    }

    platform_driver_register(&platform_drv);
    pr_info("platform driver loaded");
    return 0;
}

static void __exit platform_drv_cleanup(void) {
    platform_driver_unregister(&platform_drv);

    class_destroy(pdrv_data.class_pcd);
    unregister_chrdev_region(pdrv_data.dev_num_base, MAX_DEVICES);

    pr_info("platform driver unloaded");
}

module_init(platform_drv_init);
module_exit(platform_drv_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fgr-17");