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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");

static int major_num;
static char message[BUF_LEN] = {0};
static int message_size = 0;

// Called when device file is opened
static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "mychardev: Device opened\n");
    return 0;
}

// Called when device file is read
static ssize_t dev_read(struct file *filep, char *buffer,
                        size_t len, loff_t *offset) {
    int bytes_read = 0;

    if (*offset >= message_size)
        return 0;

    if (*offset + len > message_size)
        len = message_size - *offset;

    bytes_read = len - copy_to_user(buffer, message + *offset, len);
    *offset += bytes_read;

    printk(KERN_INFO "mychardev: Sent %d bytes to user\n", bytes_read);
    return bytes_read;
}

// Called when device file is written to
static ssize_t dev_write(struct file *filep, const char *buffer,
                         size_t len, loff_t *offset) {
    if (len > BUF_LEN - 1)
        len = BUF_LEN - 1;

    message_size = len - copy_from_user(message, buffer, len);
    message[message_size] = '\0';

    printk(KERN_INFO "mychardev: Received %d bytes from user\n", message_size);
    return message_size;
}

// Called when device file is closed
static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "mychardev: Device closed\n");
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
