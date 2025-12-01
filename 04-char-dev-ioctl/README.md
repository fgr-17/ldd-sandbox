# 04-char-dev-ioctl

Modern character device driver demonstrating:

- **Modern cdev interface**: Uses `cdev_init()` and `cdev_add()` instead of old `register_chrdev()`
- **Dynamic device number allocation**: Uses `alloc_chrdev_region()` for automatic major/minor assignment
- **ioctl commands**: Device-specific control operations
- **Per-device state**: Uses `filep->private_data` to maintain device state
- **Proper error handling**: Better cleanup paths and error checking

## ioctl Commands

| Command | Type | Description |
|---------|------|-------------|
| `IOCTL_SET_SPEED` | Write | Set device speed (0-1000) |
| `IOCTL_GET_SPEED` | Read | Get current speed |
| `IOCTL_SET_MODE` | Write | Set device mode (0-3) |
| `IOCTL_GET_MODE` | Read | Get current mode |
| `IOCTL_RESET` | None | Reset device to defaults |
| `IOCTL_GET_VERSION` | Read | Get driver version |

## Building and Testing

### 1. Build the module

~~~bash
cd 04-char-dev-ioctl
make
~~~

### 2. Load in QEMU

~~~bash
./run-qemu.sh
insmod /modules/char-dev-ioctl.ko
dmesg | tail
~~~

### 3. Create device node

Check the major number from dmesg

~~~bash
mknod /dev/myioctl c 240 0  # Use the major number from dmesg
~~~

### 4. Test basic read/write

~~~bash
echo "Hello from userspace" > /dev/myioctl
cat /dev/myioctl
~~~

### 5. Test ioctl commands

Run `test_ioctl` and check the logs:

~~~
/usr/bin/test_ioctl
~~~

## Key Differences from Old API

### Old way (register_chrdev):

~~~C
major_num = register_chrdev(0, DEVICE_NAME, &fops);
~~~

### Modern way (cdev):

~~~C
// 1. Allocate device numbers
alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);

// 2. Initialize cdev structure
cdev_init(&my_device->cdev, &fops);

// 3. Add cdev to system
cdev_add(&my_device->cdev, dev_num, 1);
~~~

### Benefits :
- Better control over device numbers
- Supports multiple devices more naturally
- Required for automatic device node creation (next example!)
- Modern kernel code style

## Monitoring

Watch kernel messages:

~~~bash
dmesg -w
~~~

Then interact with the device in another terminal to see real-time logs.
