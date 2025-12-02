/**
 * @file block-ramdisk-basic.c
 * @brief Basic RAM disk block device driver (kernel 5.0+ compatible)
 *
 * Demonstrates:
 * - Block device registration with blk-mq
 * - Modern multi-queue block layer
 * - Block I/O operations
 * - Sector-based access
 * - Integration with filesystem layer
 */

 #include <linux/init.h>
 #include <linux/module.h>
 #include <linux/kernel.h>
 #include <linux/fs.h>
 #include <linux/blkdev.h>
 #include <linux/blk-mq.h>
 #include <linux/bio.h>
 #include <linux/vmalloc.h>

 #define DEVICE_NAME "myramdisk"
 #define NSECTORS (1024 * 32)  // 32K sectors = 16MB
 #define KERNEL_SECTOR_SIZE 512

 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Federico Roux");
 MODULE_DESCRIPTION("Basic RAM disk block device driver (blk-mq)");
 MODULE_VERSION("1.0");

 // Global device structure
 struct myramdisk_dev {
     int major;                       // Major number
     u8 *data;                        // Storage space
     struct gendisk *gd;              // The gendisk structure
     struct request_queue *queue;     // Request queue
     struct blk_mq_tag_set tag_set;   // Tag set for blk-mq
 };

 static struct myramdisk_dev *Device = NULL;

 // Transfer a single bio
 static int myramdisk_transfer_bio(struct myramdisk_dev *dev, struct bio *bio) {
     struct bio_vec bvec;
     struct bvec_iter iter;
     sector_t sector = bio->bi_iter.bi_sector;
     void *buffer;
     unsigned int len;
     unsigned long offset;

     // Iterate over all segments in the bio
     bio_for_each_segment(bvec, bio, iter) {
         buffer = kmap_atomic(bvec.bv_page);
         len = bvec.bv_len;
         offset = sector * KERNEL_SECTOR_SIZE;

         // Check bounds
         if (offset + len > NSECTORS * KERNEL_SECTOR_SIZE) {
             kunmap_atomic(buffer);
             return -EIO;
         }

         if (bio_data_dir(bio) == WRITE) {
             // Write to ramdisk
             memcpy(dev->data + offset,
                    buffer + bvec.bv_offset,
                    len);
         } else {
             // Read from ramdisk
             memcpy(buffer + bvec.bv_offset,
                    dev->data + offset,
                    len);
         }

         kunmap_atomic(buffer);
         sector += len / KERNEL_SECTOR_SIZE;
     }

     return 0;
 }

 // Handle a single request
 static int myramdisk_transfer_request(struct myramdisk_dev *dev,
                                       struct request *req) {
     struct bio *bio;
     int ret = 0;

     // Process each bio in the request
     __rq_for_each_bio(bio, req) {
         ret = myramdisk_transfer_bio(dev, bio);
         if (ret)
             break;
     }

     return ret;
 }

 // blk-mq queue operation
 static blk_status_t myramdisk_queue_rq(struct blk_mq_hw_ctx *hctx,
                                        const struct blk_mq_queue_data *bd) {
     struct request *req = bd->rq;
     struct myramdisk_dev *dev = hctx->queue->queuedata;
     blk_status_t status = BLK_STS_OK;

     // Start the request
     blk_mq_start_request(req);

     // Handle the request
     if (myramdisk_transfer_request(dev, req) != 0)
         status = BLK_STS_IOERR;

     // Complete the request
     blk_mq_end_request(req, status);

     return BLK_STS_OK;
 }

 // blk-mq operations
 static struct blk_mq_ops myramdisk_mq_ops = {
     .queue_rq = myramdisk_queue_rq,
 };

 // Block device operations
 static struct block_device_operations myramdisk_fops = {
     .owner = THIS_MODULE,
 };

 // Module initialization
 static int __init myramdisk_init(void) {
     int ret;

     printk(KERN_INFO "myramdisk: Initializing...\n");

     // Allocate device structure
     Device = kzalloc(sizeof(struct myramdisk_dev), GFP_KERNEL);
     if (!Device) {
         printk(KERN_ERR "myramdisk: Failed to allocate device structure\n");
         return -ENOMEM;
     }

     // Allocate storage space
     Device->data = vmalloc(NSECTORS * KERNEL_SECTOR_SIZE);
     if (!Device->data) {
         printk(KERN_ERR "myramdisk: Failed to allocate storage\n");
         kfree(Device);
         return -ENOMEM;
     }
     memset(Device->data, 0, NSECTORS * KERNEL_SECTOR_SIZE);

     printk(KERN_INFO "myramdisk: Allocated %d KB of storage\n",
            (NSECTORS * KERNEL_SECTOR_SIZE) / 1024);

     // Register block device
     Device->major = register_blkdev(0, DEVICE_NAME);
     if (Device->major < 0) {
         printk(KERN_ERR "myramdisk: Failed to register block device\n");
         vfree(Device->data);
         kfree(Device);
         return -EBUSY;
     }

     printk(KERN_INFO "myramdisk: Registered with major number %d\n",
            Device->major);

     // Initialize blk-mq tag set
     Device->tag_set.ops = &myramdisk_mq_ops;
     Device->tag_set.nr_hw_queues = 1;
     Device->tag_set.queue_depth = 128;
     Device->tag_set.numa_node = NUMA_NO_NODE;
     Device->tag_set.cmd_size = 0;
     Device->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
     Device->tag_set.driver_data = Device;

     ret = blk_mq_alloc_tag_set(&Device->tag_set);
     if (ret) {
         printk(KERN_ERR "myramdisk: Failed to allocate tag set\n");
         unregister_blkdev(Device->major, DEVICE_NAME);
         vfree(Device->data);
         kfree(Device);
         return ret;
     }

     // Allocate disk with blk-mq
     Device->gd = blk_mq_alloc_disk(&Device->tag_set, Device);
     if (IS_ERR(Device->gd)) {
         printk(KERN_ERR "myramdisk: Failed to allocate disk\n");
         blk_mq_free_tag_set(&Device->tag_set);
         unregister_blkdev(Device->major, DEVICE_NAME);
         vfree(Device->data);
         kfree(Device);
         return PTR_ERR(Device->gd);
     }

     Device->queue = Device->gd->queue;
     Device->queue->queuedata = Device;

     // Set up gendisk
     Device->gd->major = Device->major;
     Device->gd->first_minor = 0;
     Device->gd->minors = 1;
     Device->gd->fops = &myramdisk_fops;
     Device->gd->private_data = Device;
     snprintf(Device->gd->disk_name, 32, DEVICE_NAME);
     set_capacity(Device->gd, NSECTORS);

     // Set queue properties
     blk_queue_logical_block_size(Device->queue, KERNEL_SECTOR_SIZE);
     blk_queue_physical_block_size(Device->queue, KERNEL_SECTOR_SIZE);

     // Add disk to system
     ret = add_disk(Device->gd);
     if (ret) {
         printk(KERN_ERR "myramdisk: Failed to add disk\n");
         blk_cleanup_disk(Device->gd);
         blk_mq_free_tag_set(&Device->tag_set);
         unregister_blkdev(Device->major, DEVICE_NAME);
         vfree(Device->data);
         kfree(Device);
         return ret;
     }

     printk(KERN_INFO "myramdisk: Device /dev/%s created (%d MB)\n",
            DEVICE_NAME, (NSECTORS * KERNEL_SECTOR_SIZE) / (1024 * 1024));

     return 0;
 }

 // Module cleanup
 static void __exit myramdisk_exit(void) {
     printk(KERN_INFO "myramdisk: Cleaning up...\n");

     if (Device->gd) {
         del_gendisk(Device->gd);
         blk_cleanup_disk(Device->gd);
     }

     blk_mq_free_tag_set(&Device->tag_set);

     if (Device->major > 0)
         unregister_blkdev(Device->major, DEVICE_NAME);

     if (Device->data)
         vfree(Device->data);

     kfree(Device);

     printk(KERN_INFO "myramdisk: Module unloaded\n");
 }

 module_init(myramdisk_init);
 module_exit(myramdisk_exit);
