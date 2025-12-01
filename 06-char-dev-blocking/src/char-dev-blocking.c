/**
 * @file char-dev-blocking.c
 * @brief Character device with blocking I/O using wait queues
 *
 * Demonstrates:
 * - Wait queues for blocking operations
 * - Producer-consumer pattern
 * - Interruptible sleep
 * - Wake-up mechanisms
 * - Signal handling during blocking
 */

 #include <linux/init.h>
 #include <linux/module.h>
 #include <linux/kernel.h>
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <linux/device.h>
 #include <linux/uaccess.h>
 #include <linux/slab.h>
 #include <linux/wait.h>
 #include <linux/sched.h>
 #include <linux/mutex.h>

 #define DEVICE_NAME "myblocking"
 #define CLASS_NAME "myblockclass"
 #define BUF_LEN 1024

 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Federico Roux");
 MODULE_DESCRIPTION("Character device with blocking I/O");
 MODULE_VERSION("1.0");

 // Device state structure
 struct myblock_dev {
     struct cdev cdev;
     struct device *device;
     struct mutex lock;              // Protects buffer access
     wait_queue_head_t read_queue;   // Queue for blocked readers
     wait_queue_head_t write_queue;  // Queue for blocked writers
     char *buffer;
     size_t buffer_size;
     size_t read_pos;
     size_t write_pos;
     int readers;                    // Number of current readers
     int writers;                    // Number of current writers
 };

 static dev_t dev_num;
 static struct class *myblock_class = NULL;
 static struct myblock_dev *my_device;

 // Helper: Check if buffer has data to read
 static int has_data(struct myblock_dev *dev) {
     return dev->write_pos > dev->read_pos;
 }

 // Helper: Check if buffer has space to write
 static int has_space(struct myblock_dev *dev) {
     return dev->write_pos < dev->buffer_size;
 }

 // Called when device file is opened
 static int dev_open(struct inode *inodep, struct file *filep) {
     struct myblock_dev *dev = container_of(inodep->i_cdev,
                                            struct myblock_dev, cdev);
     filep->private_data = dev;

     mutex_lock(&dev->lock);

     // Track reader/writer counts
     if (filep->f_mode & FMODE_READ)
         dev->readers++;
     if (filep->f_mode & FMODE_WRITE)
         dev->writers++;

     mutex_unlock(&dev->lock);

     printk(KERN_INFO "myblocking: Device opened (readers: %d, writers: %d)\n",
            dev->readers, dev->writers);
     return 0;
 }

 // Called when device file is read - BLOCKS if no data available
 static ssize_t dev_read(struct file *filep, char __user *buffer,
                         size_t len, loff_t *offset) {
     struct myblock_dev *dev = filep->private_data;
     size_t bytes_to_read;
     ssize_t bytes_read = 0;
     int ret;

     if (mutex_lock_interruptible(&dev->lock))
         return -ERESTARTSYS;

     // Wait for data to become available
     while (!has_data(dev)) {
         mutex_unlock(&dev->lock);

         printk(KERN_INFO "myblocking: Reader going to sleep (no data)\n");

         // Sleep until data is available or signal received
         ret = wait_event_interruptible(dev->read_queue, has_data(dev));

         if (ret < 0) {
             printk(KERN_INFO "myblocking: Reader interrupted by signal\n");
             return -ERESTARTSYS;  // Signal received
         }

         printk(KERN_INFO "myblocking: Reader woke up!\n");

         if (mutex_lock_interruptible(&dev->lock))
             return -ERESTARTSYS;
     }

     // Calculate how much to read
     bytes_to_read = dev->write_pos - dev->read_pos;
     if (bytes_to_read > len)
         bytes_to_read = len;

     // Copy to user space
     if (copy_to_user(buffer, dev->buffer + dev->read_pos, bytes_to_read)) {
         mutex_unlock(&dev->lock);
         return -EFAULT;
     }

     dev->read_pos += bytes_to_read;
     bytes_read = bytes_to_read;

     printk(KERN_INFO "myblocking: Read %zd bytes (read_pos: %zu, write_pos: %zu)\n",
            bytes_read, dev->read_pos, dev->write_pos);

     // If buffer is empty, reset positions to start
     if (dev->read_pos >= dev->write_pos) {
         dev->read_pos = 0;
         dev->write_pos = 0;
         printk(KERN_INFO "myblocking: Buffer empty, reset positions\n");
     }

     mutex_unlock(&dev->lock);

     // Wake up any waiting writers
     wake_up_interruptible(&dev->write_queue);

     return bytes_read;
 }

 // Called when device file is written to - wakes up waiting readers
 static ssize_t dev_write(struct file *filep, const char __user *buffer,
                          size_t len, loff_t *offset) {
     struct myblock_dev *dev = filep->private_data;
     size_t bytes_to_write;
     ssize_t bytes_written = 0;

     if (mutex_lock_interruptible(&dev->lock))
         return -ERESTARTSYS;

     // If buffer is full, wait for space (simplified: just reset for now)
     if (!has_space(dev)) {
         dev->read_pos = 0;
         dev->write_pos = 0;
         printk(KERN_INFO "myblocking: Buffer full, reset positions\n");
     }

     // Calculate how much to write
     bytes_to_write = dev->buffer_size - dev->write_pos;
     if (bytes_to_write > len)
         bytes_to_write = len;

     // Copy from user space
     if (copy_from_user(dev->buffer + dev->write_pos, buffer, bytes_to_write)) {
         mutex_unlock(&dev->lock);
         return -EFAULT;
     }

     dev->write_pos += bytes_to_write;
     bytes_written = bytes_to_write;

     printk(KERN_INFO "myblocking: Wrote %zd bytes (read_pos: %zu, write_pos: %zu)\n",
            bytes_written, dev->read_pos, dev->write_pos);

     mutex_unlock(&dev->lock);

     // Wake up all waiting readers!
     printk(KERN_INFO "myblocking: Waking up readers\n");
     wake_up_interruptible(&dev->read_queue);

     return bytes_written;
 }

 // Called when device file is closed
 static int dev_release(struct inode *inodep, struct file *filep) {
     struct myblock_dev *dev = filep->private_data;

     mutex_lock(&dev->lock);

     if (filep->f_mode & FMODE_READ)
         dev->readers--;
     if (filep->f_mode & FMODE_WRITE)
         dev->writers--;

     mutex_unlock(&dev->lock);

     printk(KERN_INFO "myblocking: Device closed (readers: %d, writers: %d)\n",
            dev->readers, dev->writers);
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

 // Module initialization
 static int __init chardev_init(void) {
     int ret;

     printk(KERN_INFO "myblocking: Initializing module...\n");

     // Allocate device numbers
     ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
     if (ret < 0) {
         printk(KERN_ALERT "myblocking: Failed to allocate device numbers\n");
         return ret;
     }

     printk(KERN_INFO "myblocking: Allocated major number: %d\n", MAJOR(dev_num));

     // Create device class
     myblock_class = class_create(THIS_MODULE, CLASS_NAME);
     if (IS_ERR(myblock_class)) {
         unregister_chrdev_region(dev_num, 1);
         printk(KERN_ALERT "myblocking: Failed to create device class\n");
         return PTR_ERR(myblock_class);
     }

     // Allocate device structure
     my_device = kzalloc(sizeof(struct myblock_dev), GFP_KERNEL);
     if (!my_device) {
         class_destroy(myblock_class);
         unregister_chrdev_region(dev_num, 1);
         return -ENOMEM;
     }

     // Allocate buffer
     my_device->buffer = kzalloc(BUF_LEN, GFP_KERNEL);
     if (!my_device->buffer) {
         kfree(my_device);
         class_destroy(myblock_class);
         unregister_chrdev_region(dev_num, 1);
         return -ENOMEM;
     }

     // Initialize device state
     my_device->buffer_size = BUF_LEN;
     my_device->read_pos = 0;
     my_device->write_pos = 0;
     my_device->readers = 0;
     my_device->writers = 0;
     mutex_init(&my_device->lock);
     init_waitqueue_head(&my_device->read_queue);
     init_waitqueue_head(&my_device->write_queue);

     // Initialize and add cdev
     cdev_init(&my_device->cdev, &fops);
     my_device->cdev.owner = THIS_MODULE;

     ret = cdev_add(&my_device->cdev, dev_num, 1);
     if (ret < 0) {
         kfree(my_device->buffer);
         kfree(my_device);
         class_destroy(myblock_class);
         unregister_chrdev_region(dev_num, 1);
         return ret;
     }

     // Create device automatically
     my_device->device = device_create(myblock_class, NULL, dev_num, NULL,
                                       DEVICE_NAME);
     if (IS_ERR(my_device->device)) {
         cdev_del(&my_device->cdev);
         kfree(my_device->buffer);
         kfree(my_device);
         class_destroy(myblock_class);
         unregister_chrdev_region(dev_num, 1);
         return PTR_ERR(my_device->device);
     }

     printk(KERN_INFO "myblocking: Module loaded successfully!\n");
     printk(KERN_INFO "myblocking: Device /dev/%s created\n", DEVICE_NAME);
     printk(KERN_INFO "myblocking: Try: cat /dev/%s (will block until data written)\n",
            DEVICE_NAME);

     return 0;
 }

 // Module cleanup
 static void __exit chardev_exit(void) {
     printk(KERN_INFO "myblocking: Cleaning up...\n");

     device_destroy(myblock_class, dev_num);
     cdev_del(&my_device->cdev);
     kfree(my_device->buffer);
     kfree(my_device);
     class_destroy(myblock_class);
     unregister_chrdev_region(dev_num, 1);

     printk(KERN_INFO "myblocking: Module unloaded\n");
 }

 module_init(chardev_init);
 module_exit(chardev_exit);
