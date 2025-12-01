/**
 * @file char-dev-auto-node.c
 * @brief Character device with automatic /dev node creation
 *
 * Demonstrates:
 * - Device class creation (class_create)
 * - Automatic device node creation (device_create)
 * - Proper cleanup order WITHOUT goto statements
 * - Multiple device support
 */

 #include <linux/init.h>
 #include <linux/module.h>
 #include <linux/kernel.h>
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <linux/device.h>
 #include <linux/uaccess.h>
 #include <linux/slab.h>

 #define DEVICE_NAME "myautodev"
 #define CLASS_NAME "myautoclass"
 #define NUM_DEVICES 3
 #define BUF_LEN 256

 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Federico Roux");
 MODULE_DESCRIPTION("Character device with automatic node creation");
 MODULE_VERSION("1.0");

 // Device state structure
 struct myauto_dev {
     struct cdev cdev;
     struct device *device;
     int minor;
     char buffer[BUF_LEN];
     size_t buffer_size;
     int open_count;
 };

 static dev_t dev_num;                      // First device number
 static struct class *myauto_class = NULL;  // Device class
 static struct myauto_dev *devices[NUM_DEVICES];

 // Called when device file is opened
 static int dev_open(struct inode *inodep, struct file *filep) {
     struct myauto_dev *dev = container_of(inodep->i_cdev, struct myauto_dev, cdev);
     filep->private_data = dev;

     dev->open_count++;

     printk(KERN_INFO "myautodev%d: Device opened (count: %d)\n",
            dev->minor, dev->open_count);
     return 0;
 }

 // Called when device file is read
 static ssize_t dev_read(struct file *filep, char __user *buffer,
                         size_t len, loff_t *offset) {
     struct myauto_dev *dev = filep->private_data;
     int bytes_read;

     if (*offset >= dev->buffer_size)
         return 0;

     if (*offset + len > dev->buffer_size)
         len = dev->buffer_size - *offset;

     bytes_read = len - copy_to_user(buffer, dev->buffer + *offset, len);
     *offset += bytes_read;

     printk(KERN_INFO "myautodev%d: Sent %d bytes to user\n",
            dev->minor, bytes_read);
     return bytes_read;
 }

 // Called when device file is written to
 static ssize_t dev_write(struct file *filep, const char __user *buffer,
                          size_t len, loff_t *offset) {
     struct myauto_dev *dev = filep->private_data;

     if (len > BUF_LEN - 1)
         len = BUF_LEN - 1;

     dev->buffer_size = len - copy_from_user(dev->buffer, buffer, len);
     dev->buffer[dev->buffer_size] = '\0';

     printk(KERN_INFO "myautodev%d: Received %zu bytes from user\n",
            dev->minor, dev->buffer_size);
     return dev->buffer_size;
 }

 // Called when device file is closed
 static int dev_release(struct inode *inodep, struct file *filep) {
     struct myauto_dev *dev = filep->private_data;

     printk(KERN_INFO "myautodev%d: Device closed\n", dev->minor);
     return 0;
 }

 // File operations structure
 static struct file_operations fops = {
     .owner = THIS_MODULE,
     .open = dev_open,
     .read = dev_read,
     .write = dev_write,
     .release = dev_release,
 };

 // Helper: Cleanup a single device
 static void cleanup_device(int index) {
     if (devices[index]) {
         if (devices[index]->device) {
             device_destroy(myauto_class,
                          MKDEV(MAJOR(dev_num), MINOR(dev_num) + index));
         }
         cdev_del(&devices[index]->cdev);
         kfree(devices[index]);
         devices[index] = NULL;
     }
 }

 // Helper: Cleanup all devices up to a certain index
 static void cleanup_devices(int up_to_index) {
     int i;
     for (i = 0; i < up_to_index && i < NUM_DEVICES; i++) {
         cleanup_device(i);
     }
 }

 // Helper: Cleanup class and device numbers
 static void cleanup_class_and_devnum(void) {
     if (myauto_class) {
         class_destroy(myauto_class);
         myauto_class = NULL;
     }
     unregister_chrdev_region(dev_num, NUM_DEVICES);
 }

 // Helper: Initialize a single device
 static int init_device(int index) {
     dev_t devno = MKDEV(MAJOR(dev_num), MINOR(dev_num) + index);
     int ret;

     // Allocate device structure
     devices[index] = kmalloc(sizeof(struct myauto_dev), GFP_KERNEL);
     if (!devices[index]) {
         printk(KERN_ALERT "myautodev: Failed to allocate memory for device %d\n", index);
         return -ENOMEM;
     }

     // Initialize device state
     memset(devices[index], 0, sizeof(struct myauto_dev));
     devices[index]->minor = index;
     snprintf(devices[index]->buffer, BUF_LEN, "Device %d default message\n", index);
     devices[index]->buffer_size = strlen(devices[index]->buffer);

     // Initialize cdev
     cdev_init(&devices[index]->cdev, &fops);
     devices[index]->cdev.owner = THIS_MODULE;

     // Add cdev to system
     ret = cdev_add(&devices[index]->cdev, devno, 1);
     if (ret < 0) {
         printk(KERN_ALERT "myautodev: Failed to add cdev for device %d\n", index);
         kfree(devices[index]);
         devices[index] = NULL;
         return ret;
     }

     // Create device node automatically
     devices[index]->device = device_create(myauto_class, NULL, devno, NULL,
                                           DEVICE_NAME "%d", index);
     if (IS_ERR(devices[index]->device)) {
         ret = PTR_ERR(devices[index]->device);
         printk(KERN_ALERT "myautodev: Failed to create device %d\n", index);
         devices[index]->device = NULL;
         cdev_del(&devices[index]->cdev);
         kfree(devices[index]);
         devices[index] = NULL;
         return ret;
     }

     printk(KERN_INFO "myautodev: Created /dev/%s%d\n", DEVICE_NAME, index);
     return 0;
 }

 // Module initialization
 static int __init chardev_init(void) {
     int ret;
     int i;

     printk(KERN_INFO "myautodev: Initializing module...\n");

     // Initialize devices array
     for (i = 0; i < NUM_DEVICES; i++) {
         devices[i] = NULL;
     }

     // 1. Allocate device numbers
     ret = alloc_chrdev_region(&dev_num, 0, NUM_DEVICES, DEVICE_NAME);
     if (ret < 0) {
         printk(KERN_ALERT "myautodev: Failed to allocate device numbers\n");
         return ret;
     }

     printk(KERN_INFO "myautodev: Allocated major number: %d\n", MAJOR(dev_num));

     // 2. Create device class
     myauto_class = class_create(THIS_MODULE, CLASS_NAME);
     if (IS_ERR(myauto_class)) {
         printk(KERN_ALERT "myautodev: Failed to create device class\n");
         unregister_chrdev_region(dev_num, NUM_DEVICES);
         return PTR_ERR(myauto_class);
     }

     printk(KERN_INFO "myautodev: Device class created\n");

     // 3. Create multiple devices
     for (i = 0; i < NUM_DEVICES; i++) {
         ret = init_device(i);
         if (ret < 0) {
             printk(KERN_ALERT "myautodev: Failed to initialize device %d\n", i);
             cleanup_devices(i);  // Cleanup successfully created devices
             cleanup_class_and_devnum();
             return ret;
         }
     }

     printk(KERN_INFO "myautodev: Module loaded successfully!\n");
     printk(KERN_INFO "myautodev: Try: echo 'hello' > /dev/myautodev0\n");
     return 0;
 }

 // Module cleanup
 static void __exit chardev_exit(void) {
     printk(KERN_INFO "myautodev: Cleaning up...\n");

     // Cleanup must be done in reverse order of initialization
     cleanup_devices(NUM_DEVICES);
     cleanup_class_and_devnum();

     printk(KERN_INFO "myautodev: Module unloaded\n");
 }

 module_init(chardev_init);
 module_exit(chardev_exit);
