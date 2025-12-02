# 08-block-ramdisk-basic - Basic RAM Disk Block Device

This example demonstrates a basic block device driver using a RAM disk with the modern **blk-mq** (multi-queue block layer) API. Unlike character devices that handle byte streams, block devices handle fixed-size blocks and integrate with the filesystem layer.

**Note:** This example uses the modern blk-mq API compatible with kernel 5.0+. The old `blk_init_queue()` API was removed in kernel 5.0.

## What's New: Block Devices

### Character Device (Previous Examples)
- Sequential byte-stream access
- Direct read/write operations
- No caching by kernel
- Examples: serial port, keyboard

### Block Device (This Example)
- Random access in fixed-size blocks (sectors)
- Request-based I/O
- Kernel page cache integration
- Examples: hard disk, SSD, USB drive

## Key Concepts

### Sectors and Blocks

**Sector**: Minimum addressable unit (usually 512 bytes)

~~~C
#define SECTOR_SIZE 512
#define NSECTORS (1024 * 32)  // 32K sectors = 16MB
~~~

### Request Queue (`blk-mq`)

Block devices use the multi-queue block layer (`blk-mq`) for efficient request processing:

~~~
User I/O → VFS → Page Cache → Block Layer → blk-mq → Hardware Queue → Driver
~~~

The `blk-mq` layer:
- Supports multiple hardware queues for parallel processing
- Merges adjacent requests
- Distributes requests across CPU cores
- Caches frequently accessed blocks

### gendisk Structure

Represents a generic disk:

~~~C
struct gendisk *gd;
gd->major = major_number;
gd->first_minor = 0;
gd->fops = &block_device_operations;
set_capacity(gd, num_sectors);
~~~

## Requirements

- **Kernel Version**: 5.0 or later (uses blk-mq API)
- For older kernels (pre-5.0), the legacy `blk_init_queue()` API would be needed

## Building and Testing

### 1. Build the module

~~~bash
cd 08-block-ramdisk-basic
make
~~~

### 2. Load in QEMU

~~~bash
./run-qemu.sh
insmod /modules/block-ramdisk-basic.ko
dmesg | tail
~~~

Output:
~~~
[ 123.456] myramdisk: Initializing...
[ 123.457] myramdisk: Allocated 16384 KB of storage
[ 123.458] myramdisk: Registered with major number 254
[ 123.459] myramdisk: Device /dev/myramdisk created (16 MB)
~~~

### 3. Verify device created

~~~bash
ls -l /dev/myramdisk
~~~

Output:
~~~
brw-rw---- 1 root root 254, 0 Nov 29 10:30 /dev/myramdisk
~~~

Notice: **`b`** = block device (not `c` for character)

### 4. Check device info

~~~bash
ls -l /sys/class/block/myramdisk/

# Device details
fdisk -l /dev/myramdisk

# Block device info
cat /sys/block/myramdisk/size
cat /sys/class/block/myramdisk/dev    # Shows: 254:0
cat /sys/class/block/myramdisk/size   # Shows: 32768 (sectors)
~~~

## Using the RAM Disk

### Experiment 1: test without formatting

~~~bash
# 1. Write some data
echo "Hello Block Device!" > /tmp/test.txt
dd if=/tmp/test.txt of=/dev/myramdisk bs=512 count=1

# 2. Read it back
dd if=/dev/myramdisk bs=512 count=1
# Should show: Hello Block Device!

# 3. Test larger write
dd if=/dev/zero of=/dev/myramdisk bs=1M count=5

# 4. Check for errors in dmesg
dmesg | tail -20
~~~

### Experiment 2: Fill the Disk

~~~bash
# Try to fill the disk
dd if=/dev/zero of=/mnt/ramdisk/bigfile bs=1M count=20
~~~

Should fail with "No space left on device" after ~15MB.

## Understanding the Code

### Device Registration (blk-mq)

~~~C
// 1. Register block device
major = register_blkdev(0, "myramdisk");

// 2. Initialize blk-mq tag set
tag_set.ops = &myramdisk_mq_ops;
tag_set.nr_hw_queues = 1;
tag_set.queue_depth = 128;
blk_mq_alloc_tag_set(&tag_set);

// 3. Allocate disk with blk-mq
gd = blk_mq_alloc_disk(&tag_set, device);

// 4. Configure disk
gd->major = major;
gd->minors = 1;  // 1 minor = no partition support
set_capacity(gd, num_sectors);

// 5. Add to system
add_disk(gd);
~~~

### Request Processing (blk-mq)

~~~C
static blk_status_t myramdisk_queue_rq(struct blk_mq_hw_ctx *hctx,
                                       const struct blk_mq_queue_data *bd) {
    struct request *req = bd->rq;
    struct myramdisk_dev *dev = hctx->queue->queuedata;
    blk_status_t status = BLK_STS_OK;

    // Start the request
    blk_mq_start_request(req);

    // Process the request
    if (myramdisk_transfer_request(dev, req) != 0)
        status = BLK_STS_IOERR;

    // Complete the request
    blk_mq_end_request(req, status);

    return BLK_STS_OK;
}

// blk-mq operations structure
static struct blk_mq_ops myramdisk_mq_ops = {
    .queue_rq = myramdisk_queue_rq,
};
~~~

### Data Transfer

~~~C
// Each request contains one or more bios
// Each bio contains one or more segments (pages)
bio_for_each_segment(bvec, bio, iter) {
    if (WRITE)
        memcpy(ramdisk + offset, page_data, len);
    else
        memcpy(page_data, ramdisk + offset, len);
}
~~~

## Block Device vs Character Device

| Aspect | Character Device | Block Device |
|--------|------------------|--------------|
| **Access Unit** | Bytes | Sectors (512/4096 bytes) |
| **Access Pattern** | Sequential | Random access |
| **Buffering** | None | Page cache |
| **I/O Model** | `read()`/`write()` | Request queues |
| **Filesystems** | No | Yes |
| **Example Operations** | `cat /dev/mychar` | `mount /dev/myblock` |
| **Device Type** | `c` (character) | `b` (block) |

## Key Differences from Char Devices

### 1. Registration

**Character:**
~~~C
register_chrdev(major, "name", &fops);
~~~

**Block (modern blk-mq):**
~~~C
register_blkdev(major, "name");
blk_mq_alloc_tag_set(&tag_set);
blk_mq_alloc_disk(&tag_set, device);
add_disk(gd);
~~~

### 2. I/O Operations

**Character:**
~~~C
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
~~~

**Block (blk-mq):**
~~~C
blk_status_t queue_rq(struct blk_mq_hw_ctx *hctx,
                      const struct blk_mq_queue_data *bd);
~~~

### 3. User Interaction

**Character:**
~~~bash
cat /dev/mychar       # Direct read
echo "x" > /dev/mychar # Direct write
~~~

**Block:**
~~~bash
mkfs.ext4 /dev/myblock  # Format
mount /dev/myblock /mnt # Mount
ls /mnt                 # Access via filesystem
~~~

## Monitoring

### Watch Block I/O

~~~bash
# Kernel messages
dmesg -w

# I/O statistics (if available)
iostat -x 1

# Block device statistics
cat /sys/block/myramdisk/stat

# Queue information
cat /sys/block/myramdisk/queue/scheduler
~~~

### Track Requests

Add debug output to the request handler to see all I/O:

~~~C
printk(KERN_INFO "Request: sector=%llu, size=%u, dir=%s\n",
       (unsigned long long)blk_rq_pos(req),
       blk_rq_bytes(req),
       rq_data_dir(req) ? "WRITE" : "READ");
~~~

## Cleanup

~~~bash
# Unmount first!
umount /mnt/ramdisk

# Remove module
rmmod block_ramdisk_basic

# Verify device gone
ls /dev/myramdisk  # Should not exist
lsblk              # Should not show myramdisk
~~~

## Troubleshooting

### "Device busy" when removing module?

Make sure to unmount first:
~~~bash
umount /mnt/ramdisk
lsof | grep myramdisk  # Check for open files
rmmod block_ramdisk_basic
~~~

### Module fails to load?

Check kernel version and compatibility:
~~~bash
dmesg | tail
# Look for API compatibility issues
~~~

This example uses the blk-mq API which requires **kernel 5.0 or later**. For older kernels (pre-5.0), you would need the legacy `blk_init_queue()` API.

### Can't format or mount?

Check device exists and has correct permissions:
~~~bash
ls -l /dev/myramdisk
# Should show: brw-rw---- (block device)
~~~

### Data lost after reboot?

This is expected! It's a RAM disk - data only persists while:
1. The module is loaded
2. The system is running

To persist data, copy it before unloading:
~~~bash
cp -r /mnt/ramdisk/* /tmp/backup/
~~~

## Real-World Block Devices

This RAM disk is similar to:

- **`/dev/ram0`** - Kernel RAM disks
- **`/dev/loop0`** - Loop devices (file-backed)
- **tmpfs/ramfs** - Filesystem in RAM
- **NVMe** - Fast SSD (but with real hardware)

## Performance Comparison

Test your RAM disk vs real disk:

~~~bash
# RAM disk
time dd if=/dev/zero of=/mnt/ramdisk/test bs=1M count=100

# Real disk (if available)
time dd if=/dev/zero of=/tmp/test bs=1M count=100
~~~

RAM disk should be MUCH faster!

## Limitations of This Example

1. **No partition support** - Only one device, no `/dev/myramdisk1`, etc.
2. **Single hardware queue** - Uses only 1 queue (blk-mq supports multiple)
3. **Simple error handling** - Minimal error checking
4. **Fixed size** - Can't resize after loading
5. **No advanced features** - No discard, flush, or other advanced operations

These will be addressed in future examples!

## Detailed I/O Flow

### How a Write Happens

~~~
1. User: echo "test" > /mnt/ramdisk/file.txt
        ↓
2. VFS: Handles file operations
        ↓
3. Filesystem (ext4): Translates to block numbers
        ↓
4. Page Cache: Buffers the data
        ↓
5. Block Layer: Creates struct request
        ↓
6. Request Queue: Queues the request
        ↓
7. Driver: myramdisk_request() called
        ↓
8. Transfer: memcpy to RAM buffer
        ↓
9. Complete: __blk_end_request_all()
        ↓
10. VFS: Returns to userspace
~~~

### How a Read Happens

~~~
1. User: cat /mnt/ramdisk/file.txt
        ↓
2. Page Cache: Check if data cached
        ↓ (cache miss)
3. Block Layer: Create read request
        ↓
4. Driver: myramdisk_request() called
        ↓
5. Transfer: memcpy from RAM buffer
        ↓
6. Page Cache: Cache the data
        ↓
7. VFS: Return to userspace
~~~

## Understanding blk-mq

### What is blk-mq?

**blk-mq** (multi-queue block layer) is the modern block I/O subsystem introduced in kernel 3.13 and became mandatory in kernel 5.0.

**Key advantages:**
- **Multiple queues**: One queue per CPU core for better scalability
- **Lower latency**: Direct submission without locking bottlenecks
- **Better performance**: Especially on multi-core systems and fast storage (NVMe, SSD)

### blk-mq Components

~~~
Application
    ↓
VFS/Filesystem
    ↓
Block Layer (blk-mq core)
    ↓
Software Queue (per CPU)
    ↓
Hardware Queue (per device)
    ↓
Driver (our code)
    ↓
Hardware
~~~

### Tag Set

The `blk_mq_tag_set` structure configures the queues:

~~~C
struct blk_mq_tag_set tag_set = {
    .ops = &myramdisk_mq_ops,      // Operations
    .nr_hw_queues = 1,              // Number of hardware queues
    .queue_depth = 128,             // Max requests in flight
    .numa_node = NUMA_NO_NODE,      // NUMA node
    .flags = BLK_MQ_F_SHOULD_MERGE, // Enable request merging
};
~~~

### Request Flow in blk-mq

~~~
1. Userspace I/O call
2. VFS processes request
3. Block layer allocates tag
4. Request placed in software queue
5. Scheduler moves to hardware queue
6. Driver's queue_rq() called
7. Driver processes request
8. Driver calls blk_mq_end_request()
9. Tag freed, statistics updated
~~~

## Understanding bio and request

### struct bio
Represents a single I/O operation:
- Start sector
- Size in bytes
- Array of memory pages (bio_vec)
- Direction (READ/WRITE)

### struct request
May contain multiple bios:
- Combines related I/O operations
- Optimized by block layer
- Can be split or merged

### Processing Flow

~~~C
request
├── bio 1
│   ├── segment 1 (page 1, offset, length)
│   ├── segment 2 (page 2, offset, length)
│   └── segment 3 (page 3, offset, length)
├── bio 2
│   ├── segment 1
│   └── segment 2
└── bio 3
    └── segment 1
~~~
