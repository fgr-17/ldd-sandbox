/**
 * @file char-device-driver.c
 * @brief generates a basic example of a character device driver
 */

// chardev.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define BUF_LEN 1024
#define DEV_MAX 16

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marc Anthony");

static int major_num;
static char message[DEV_MAX][BUF_LEN] = {0};
static int message_size[DEV_MAX] = {0};

// Called when device file is opened
static int dev_open(struct inode *inodep, struct file *filep) {
    int minor = iminor(inodep);
    printk(KERN_INFO "mychardev: Device %d opened\n", minor);
    return 0;
}

// Called when device file is read
static ssize_t dev_read(struct file *filep, char *buffer,
                        size_t len, loff_t *offset) {
    int minor = iminor(filep->f_inode);
    int bytes_read = 0;

    if (minor >= DEV_MAX) {
        return -ENODEV;
    }

    if (*offset >= message_size[minor])
        return 0;

    if (*offset + len > message_size[minor])
        len = message_size[minor] - *offset;

    bytes_read = len - copy_to_user(buffer, message[minor] + *offset, len);
    *offset += bytes_read;

    printk(KERN_INFO "mychardev: Device %d sent %d bytes to user\n", minor, bytes_read);
    return bytes_read;
}

// Called when device file is written to
static ssize_t dev_write(struct file *filep, const char *buffer,
                         size_t len, loff_t *offset) {
    int minor = iminor(filep->f_inode);

    if (minor >= DEV_MAX) {
        return -ENODEV;
    }

    if (len > BUF_LEN - 1)
        len = BUF_LEN - 1;

    message_size[minor] = len - copy_from_user(message[minor], buffer, len);
    message[minor][message_size[minor]] = '\0';

    printk(KERN_INFO "mychardev: Device %d received %d bytes from user\n", minor, message_size[minor]);
    return message_size[minor];
}

// Called when device file is closed
static int dev_release(struct inode *inodep, struct file *filep) {
    int minor = iminor(inodep);
    printk(KERN_INFO "mychardev: Device %d closed\n", minor);
    return 0;
}

// File operations structure
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Module initialization
static int __init chardev_init(void) {
    major_num = register_chrdev(0, DEVICE_NAME, &fops);

    if (major_num < 0) {
        printk(KERN_ALERT "mychardev: Failed to register\n");
        return major_num;
    }

    printk(KERN_INFO "mychardev: Registered with major number %d\n", major_num);
    printk(KERN_INFO "Create device: mknod /dev/%s c %d 0\n",
           DEVICE_NAME, major_num);
    return 0;
}

// Module cleanup
static void __exit chardev_exit(void) {
    unregister_chrdev(major_num, DEVICE_NAME);
    printk(KERN_INFO "mychardev: Unregistered\n");
}

module_init(chardev_init);
module_exit(chardev_exit);
