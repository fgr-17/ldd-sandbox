/**
 * @file char-dev-poll.c
 * @brief Character device with poll/select support
 *
 * Demonstrates:
 * - poll() file operation
 * - Integration with wait queues
 * - POLLIN/POLLOUT event reporting
 * - Non-blocking I/O multiplexing
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
 #include <linux/poll.h>
 #include <linux/mutex.h>

 #define DEVICE_NAME "mypoll"
 #define CLASS_NAME "mypollclass"
 #define BUF_LEN 1024

 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("John Doe");
 MODULE_DESCRIPTION("Character device with poll/select support");
 MODULE_VERSION("1.0");

 // Device state structure
 struct mypoll_dev {
     struct cdev cdev;
     struct device *device;
     struct mutex lock;
     wait_queue_head_t read_queue;
     wait_queue_head_t write_queue;
     char *buffer;
     size_t buffer_size;
     size_t read_pos;
     size_t write_pos;
     int readers;
     int writers;
 };

 static dev_t dev_num;
 static struct class *mypoll_class = NULL;
 static struct mypoll_dev *my_device;

 // Helper: Check if buffer has data to read
 static int has_data(struct mypoll_dev *dev) {
     return dev->write_pos > dev->read_pos;
 }

 // Helper: Check if buffer has space to write
 static int has_space(struct mypoll_dev *dev) {
     return dev->write_pos < dev->buffer_size;
 }

 // Called when device file is opened
 static int dev_open(struct inode *inodep, struct file *filep) {
     struct mypoll_dev *dev = container_of(inodep->i_cdev,
                                           struct mypoll_dev, cdev);
     filep->private_data = dev;

     mutex_lock(&dev->lock);
     if (filep->f_mode & FMODE_READ)
         dev->readers++;
     if (filep->f_mode & FMODE_WRITE)
         dev->writers++;
     mutex_unlock(&dev->lock);

     printk(KERN_INFO "mypoll: Device opened (readers: %d, writers: %d)\n",
            dev->readers, dev->writers);
     return 0;
 }

 // Called when device file is read
 static ssize_t dev_read(struct file *filep, char __user *buffer,
                         size_t len, loff_t *offset) {
     struct mypoll_dev *dev = filep->private_data;
     size_t bytes_to_read;
     ssize_t bytes_read = 0;
     int ret;

     if (mutex_lock_interruptible(&dev->lock))
         return -ERESTARTSYS;

     // Non-blocking mode check
     if ((filep->f_flags & O_NONBLOCK) && !has_data(dev)) {
         mutex_unlock(&dev->lock);
         return -EAGAIN;
     }

     // Wait for data (blocking mode)
     while (!has_data(dev)) {
         mutex_unlock(&dev->lock);

         printk(KERN_INFO "mypoll: Reader going to sleep\n");
         ret = wait_event_interruptible(dev->read_queue, has_data(dev));

         if (ret < 0)
             return -ERESTARTSYS;

         if (mutex_lock_interruptible(&dev->lock))
             return -ERESTARTSYS;
     }

     // Read data
     bytes_to_read = dev->write_pos - dev->read_pos;
     if (bytes_to_read > len)
         bytes_to_read = len;

     if (copy_to_user(buffer, dev->buffer + dev->read_pos, bytes_to_read)) {
         mutex_unlock(&dev->lock);
         return -EFAULT;
     }

     dev->read_pos += bytes_to_read;
     bytes_read = bytes_to_read;

     printk(KERN_INFO "mypoll: Read %zd bytes (read_pos: %zu, write_pos: %zu)\n",
            bytes_read, dev->read_pos, dev->write_pos);

     // Reset if buffer empty
     if (dev->read_pos >= dev->write_pos) {
         dev->read_pos = 0;
         dev->write_pos = 0;
     }

     mutex_unlock(&dev->lock);

     // Wake up writers waiting for space
     wake_up_interruptible(&dev->write_queue);

     return bytes_read;
 }

 // Called when device file is written to
 static ssize_t dev_write(struct file *filep, const char __user *buffer,
                          size_t len, loff_t *offset) {
     struct mypoll_dev *dev = filep->private_data;
     size_t bytes_to_write;
     ssize_t bytes_written = 0;

     if (mutex_lock_interruptible(&dev->lock))
         return -ERESTARTSYS;

     // Reset if buffer full
     if (!has_space(dev)) {
         dev->read_pos = 0;
         dev->write_pos = 0;
     }

     bytes_to_write = dev->buffer_size - dev->write_pos;
     if (bytes_to_write > len)
         bytes_to_write = len;

     if (copy_from_user(dev->buffer + dev->write_pos, buffer, bytes_to_write)) {
         mutex_unlock(&dev->lock);
         return -EFAULT;
     }

     dev->write_pos += bytes_to_write;
     bytes_written = bytes_to_write;

     printk(KERN_INFO "mypoll: Wrote %zd bytes (read_pos: %zu, write_pos: %zu)\n",
            bytes_written, dev->read_pos, dev->write_pos);

     mutex_unlock(&dev->lock);

     // Wake up readers waiting for data
     wake_up_interruptible(&dev->read_queue);

     return bytes_written;
 }

 // Called when poll/select/epoll is used on this device
 static __poll_t dev_poll(struct file *filep, poll_table *wait) {
     struct mypoll_dev *dev = filep->private_data;
     __poll_t mask = 0;

     printk(KERN_INFO "mypoll: poll() called\n");

     // Add our wait queues to the poll table
     // This doesn't block - it just registers interest
     poll_wait(filep, &dev->read_queue, wait);
     poll_wait(filep, &dev->write_queue, wait);

     mutex_lock(&dev->lock);

     // Check if device is readable
     if (has_data(dev)) {
         mask |= POLLIN | POLLRDNORM;  // Data available for reading
         printk(KERN_INFO "mypoll: Device is READABLE\n");
     }

     // Check if device is writable
     if (has_space(dev)) {
         mask |= POLLOUT | POLLWRNORM; // Space available for writing
         printk(KERN_INFO "mypoll: Device is WRITABLE\n");
     }

     mutex_unlock(&dev->lock);

     return mask;
 }

 // Called when device file is closed
 static int dev_release(struct inode *inodep, struct file *filep) {
     struct mypoll_dev *dev = filep->private_data;

     mutex_lock(&dev->lock);
     if (filep->f_mode & FMODE_READ)
         dev->readers--;
     if (filep->f_mode & FMODE_WRITE)
         dev->writers--;
     mutex_unlock(&dev->lock);

     printk(KERN_INFO "mypoll: Device closed (readers: %d, writers: %d)\n",
            dev->readers, dev->writers);
     return 0;
 }

 // File operations structure
 static struct file_operations fops = {
     .owner = THIS_MODULE,
     .open = dev_open,
     .read = dev_read,
     .write = dev_write,
     .poll = dev_poll,  // The magic function!
     .release = dev_release,
 };

 // Module initialization
 static int __init chardev_init(void) {
     int ret;

     printk(KERN_INFO "mypoll: Initializing module...\n");

     ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
     if (ret < 0)
         return ret;

     printk(KERN_INFO "mypoll: Allocated major number: %d\n", MAJOR(dev_num));

     mypoll_class = class_create(THIS_MODULE, CLASS_NAME);
     if (IS_ERR(mypoll_class)) {
         unregister_chrdev_region(dev_num, 1);
         return PTR_ERR(mypoll_class);
     }

     my_device = kzalloc(sizeof(struct mypoll_dev), GFP_KERNEL);
     if (!my_device) {
         class_destroy(mypoll_class);
         unregister_chrdev_region(dev_num, 1);
         return -ENOMEM;
     }

     my_device->buffer = kzalloc(BUF_LEN, GFP_KERNEL);
     if (!my_device->buffer) {
         kfree(my_device);
         class_destroy(mypoll_class);
         unregister_chrdev_region(dev_num, 1);
         return -ENOMEM;
     }

     my_device->buffer_size = BUF_LEN;
     my_device->read_pos = 0;
     my_device->write_pos = 0;
     my_device->readers = 0;
     my_device->writers = 0;
     mutex_init(&my_device->lock);
     init_waitqueue_head(&my_device->read_queue);
     init_waitqueue_head(&my_device->write_queue);

     cdev_init(&my_device->cdev, &fops);
     my_device->cdev.owner = THIS_MODULE;

     ret = cdev_add(&my_device->cdev, dev_num, 1);
     if (ret < 0) {
         kfree(my_device->buffer);
         kfree(my_device);
         class_destroy(mypoll_class);
         unregister_chrdev_region(dev_num, 1);
         return ret;
     }

     my_device->device = device_create(mypoll_class, NULL, dev_num, NULL,
                                       DEVICE_NAME);
     if (IS_ERR(my_device->device)) {
         cdev_del(&my_device->cdev);
         kfree(my_device->buffer);
         kfree(my_device);
         class_destroy(mypoll_class);
         unregister_chrdev_region(dev_num, 1);
         return PTR_ERR(my_device->device);
     }

     printk(KERN_INFO "mypoll: Module loaded successfully!\n");
     printk(KERN_INFO "mypoll: Device /dev/%s created\n", DEVICE_NAME);

     return 0;
 }

 // Module cleanup
 static void __exit chardev_exit(void) {
     printk(KERN_INFO "mypoll: Cleaning up...\n");

     device_destroy(mypoll_class, dev_num);
     cdev_del(&my_device->cdev);
     kfree(my_device->buffer);
     kfree(my_device);
     class_destroy(mypoll_class);
     unregister_chrdev_region(dev_num, 1);

     printk(KERN_INFO "mypoll: Module unloaded\n");
 }

 module_init(chardev_init);
 module_exit(chardev_exit);
