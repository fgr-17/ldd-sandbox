/**
 * @file char-dev-ioctl.c
 * @brief Modern character device driver with ioctl support
 *
 * Demonstrates:
 * - Modern cdev interface (cdev_init/cdev_add)
 * - Device-specific ioctl commands
 * - Proper device number allocation
 * - Better error handling
 */

 #include <linux/init.h>
 #include <linux/module.h>
 #include <linux/kernel.h>
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <linux/uaccess.h>
 #include <linux/slab.h>


 #define DEVICE_NAME "myioctl"
 #define BUF_LEN 256

 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Juan Perez");
 MODULE_DESCRIPTION("Character device with ioctl support");

 // Define ioctl commands
 // _IOW = write (userspace -> kernel)
 // _IOR = read (kernel -> userspace)
 // _IOWR = read/write
 #define IOCTL_MAGIC 'k'  // Magic number for this driver
 #define IOCTL_SET_SPEED     _IOW(IOCTL_MAGIC, 1, int)
 #define IOCTL_GET_SPEED     _IOR(IOCTL_MAGIC, 2, int)
 #define IOCTL_SET_MODE      _IOW(IOCTL_MAGIC, 3, int)
 #define IOCTL_GET_MODE      _IOR(IOCTL_MAGIC, 4, int)
 #define IOCTL_RESET         _IO(IOCTL_MAGIC, 5)
 #define IOCTL_GET_VERSION   _IOR(IOCTL_MAGIC, 6, int)

 #define DRIVER_VERSION 100  // Version 1.00

 // Device state
 struct myioctl_dev {
     struct cdev cdev;
     int speed;        // Simulated hardware speed setting
     int mode;         // Simulated hardware mode
     char buffer[BUF_LEN];
     size_t buffer_size;
 };

 static dev_t dev_num;           // First device number
 static struct myioctl_dev *my_device;

 // Called when device file is opened
 static int dev_open(struct inode *inodep, struct file *filep) {
     struct myioctl_dev *dev = container_of(inodep->i_cdev, struct myioctl_dev, cdev);
     filep->private_data = dev;

     printk(KERN_INFO "myioctl: Device opened\n");
     return 0;
 }

 // Called when device file is read
 static ssize_t dev_read(struct file *filep, char __user *buffer,
                         size_t len, loff_t *offset) {
     struct myioctl_dev *dev = filep->private_data;
     int bytes_read;

     if (*offset >= dev->buffer_size)
         return 0;

     if (*offset + len > dev->buffer_size)
         len = dev->buffer_size - *offset;

     bytes_read = len - copy_to_user(buffer, dev->buffer + *offset, len);
     *offset += bytes_read;

     printk(KERN_INFO "myioctl: Sent %d bytes to user\n", bytes_read);
     return bytes_read;
 }

 // Called when device file is written to
 static ssize_t dev_write(struct file *filep, const char __user *buffer,
                          size_t len, loff_t *offset) {
     struct myioctl_dev *dev = filep->private_data;

     if (len > BUF_LEN - 1)
         len = BUF_LEN - 1;

     dev->buffer_size = len - copy_from_user(dev->buffer, buffer, len);
     dev->buffer[dev->buffer_size] = '\0';

     printk(KERN_INFO "myioctl: Received %zu bytes from user\n", dev->buffer_size);
     return dev->buffer_size;
 }

 // Called when ioctl is invoked
 static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
     struct myioctl_dev *dev = filep->private_data;
     int temp;
     int retval = 0;

     // Verify the magic number
     // ref: The magic number is a namespace mechanism
     // to prevent ioctl command collisions across different drivers in the system.
     if (_IOC_TYPE(cmd) != IOCTL_MAGIC) {
         printk(KERN_WARNING "myioctl: Invalid ioctl magic number\n");
         return -ENOTTY;
     }

     switch (cmd) {
         case IOCTL_SET_SPEED:
             if (copy_from_user(&temp, (int __user *)arg, sizeof(int))) {
                 return -EFAULT;
             }
             if (temp < 0 || temp > 1000) {
                 printk(KERN_WARNING "myioctl: Invalid speed value %d\n", temp);
                 return -EINVAL;
             }
             dev->speed = temp;
             printk(KERN_INFO "myioctl: Speed set to %d\n", dev->speed);
             break;

         case IOCTL_GET_SPEED:
             if (copy_to_user((int __user *)arg, &dev->speed, sizeof(int))) {
                 return -EFAULT;
             }
             printk(KERN_INFO "myioctl: Speed read: %d\n", dev->speed);
             break;

         case IOCTL_SET_MODE:
             if (copy_from_user(&temp, (int __user *)arg, sizeof(int))) {
                 return -EFAULT;
             }
             if (temp < 0 || temp > 3) {
                 printk(KERN_WARNING "myioctl: Invalid mode value %d\n", temp);
                 return -EINVAL;
             }
             dev->mode = temp;
             printk(KERN_INFO "myioctl: Mode set to %d\n", dev->mode);
             break;

         case IOCTL_GET_MODE:
             if (copy_to_user((int __user *)arg, &dev->mode, sizeof(int))) {
                 return -EFAULT;
             }
             printk(KERN_INFO "myioctl: Mode read: %d\n", dev->mode);
             break;

         case IOCTL_RESET:
             dev->speed = 0;
             dev->mode = 0;
             dev->buffer_size = 0;
             memset(dev->buffer, 0, BUF_LEN);
             printk(KERN_INFO "myioctl: Device reset\n");
             break;

         case IOCTL_GET_VERSION:
             temp = DRIVER_VERSION;
             if (copy_to_user((int __user *)arg, &temp, sizeof(int))) {
                 return -EFAULT;
             }
             printk(KERN_INFO "myioctl: Version read: %d\n", temp);
             break;

         default:
             printk(KERN_WARNING "myioctl: Unknown ioctl command: 0x%x\n", cmd);
             return -ENOTTY;
     }

     return retval;
 }

 // Called when device file is closed
 static int dev_release(struct inode *inodep, struct file *filep) {
     printk(KERN_INFO "myioctl: Device closed\n");
     return 0;
 }

 // File operations structure
 static struct file_operations fops = {
     .owner = THIS_MODULE,
     .open = dev_open,
     .read = dev_read,
     .write = dev_write,
     .unlocked_ioctl = dev_ioctl,
     .release = dev_release,
 };

 // Module initialization
 static int __init chardev_init(void) {
     int ret;

     // Allocate device numbers dynamically
     ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
     if (ret < 0) {
         printk(KERN_ALERT "myioctl: Failed to allocate device numbers\n");
         return ret;
     }

     printk(KERN_INFO "myioctl: Allocated device numbers - Major: %d, Minor: %d\n",
            MAJOR(dev_num), MINOR(dev_num));

     // Allocate device structure
     my_device = kmalloc(sizeof(struct myioctl_dev), GFP_KERNEL);
     if (!my_device) {
         unregister_chrdev_region(dev_num, 1);
         printk(KERN_ALERT "myioctl: Failed to allocate memory\n");
         return -ENOMEM;
     }

     // Initialize device state
     memset(my_device, 0, sizeof(struct myioctl_dev));
     my_device->speed = 100;  // Default speed
     my_device->mode = 0;     // Default mode

     // Initialize and add cdev
     cdev_init(&my_device->cdev, &fops);
     my_device->cdev.owner = THIS_MODULE;

     ret = cdev_add(&my_device->cdev, dev_num, 1);
     if (ret < 0) {
         kfree(my_device);
         unregister_chrdev_region(dev_num, 1);
         printk(KERN_ALERT "myioctl: Failed to add cdev\n");
         return ret;
     }

     printk(KERN_INFO "myioctl: Device registered successfully\n");
     printk(KERN_INFO "Create device: mknod /dev/%s c %d %d\n",
            DEVICE_NAME, MAJOR(dev_num), MINOR(dev_num));

     return 0;
 }

 // Module cleanup
 static void __exit chardev_exit(void) {
     cdev_del(&my_device->cdev);
     kfree(my_device);
     unregister_chrdev_region(dev_num, 1);
     printk(KERN_INFO "myioctl: Device unregistered\n");
 }

 module_init(chardev_init);
 module_exit(chardev_exit);
