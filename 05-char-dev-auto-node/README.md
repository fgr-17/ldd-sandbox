# 05-char-dev-auto-node

Character device driver with automatic device node creation, demonstrating:

- **Device classes**: Using `class_create()` to create device classes
- **Automatic `/dev` node creation**: Using `device_create()` to auto-create device nodes
- **udev integration**: Automatic integration with the Linux device manager
- **sysfs hierarchy**: Creates `/sys/class/` entries for device discovery
- **Multiple devices**: Creates 3 independent devices automatically
- **Modern best practices**: No manual `mknod` required!

## What's New: Device Classes and Automatic Creation

### The Traditional Way (Previous Examples)
~~~bash
insmod /modules/char-device-driver.ko
mknod /dev/mychardev c 241 0  # Manual device node creation!
~~~

### The Modern Way (This Example)
~~~bash
insmod /modules/char-dev-auto-node.ko
# /dev/myautodev0, /dev/myautodev1, /dev/myautodev2 appear automatically!
~~~

### How It Works

When you load this module, it:

1. **Creates a device class** with `class_create()`
   - Creates `/sys/class/myautoclass/` directory
   - Registers the class with the kernel device model

2. **Creates devices** with `device_create()`
   - Creates sysfs entries: `/sys/class/myautoclass/myautodev0/`
   - Sends events to udev
   - udev automatically creates `/dev/myautodev0` node

3. **Integrates with udev**
   - Device appears/disappears automatically on module load/unload
   - Permissions can be controlled via udev rules
   - Device events can trigger scripts

## Loading Module in QEMU

### 1. Compile the module

In the docker container `ldd-sandbox`:

~~~bash
cd 05-char-dev-auto-node
make
~~~

### 2. Launch QEMU and load module

~~~bash
./run-qemu.sh
insmod /modules/char-dev-auto-node.ko
~~~

### 3. Check the kernel messages

~~~bash
dmesg | tail
~~~

You should see:
~~~
[ 123.456789] myautodev: Initializing module...
[ 123.456790] myautodev: Allocated major number: 240
[ 123.456791] myautodev: Device class created
[ 123.456792] myautodev: Created /dev/myautodev0
[ 123.456793] myautodev: Created /dev/myautodev1
[ 123.456794] myautodev: Created /dev/myautodev2
[ 123.456795] myautodev: Module loaded successfully!
[ 123.456796] myautodev: Try: echo 'hello' > /dev/myautodev0
~~~

### 4. Verify devices were created automatically

~~~bash
ls -l /dev/myautodev*
~~~

Output:
~~~
crw------- 1 root root 240, 0 Nov 29 10:30 /dev/myautodev0
crw------- 1 root root 240, 1 Nov 29 10:30 /dev/myautodev1
crw------- 1 root root 240, 2 Nov 29 10:30 /dev/myautodev2
~~~

**No `mknod` needed!**

## Understanding Device Classes

### What is a Device Class?

A device class is a high-level view of devices in Linux. It groups similar devices together regardless of their physical bus or location.

### The Class Hierarchy

~~~bash
# Check the class was created
ls /sys/class/ | grep myautoclass
myautoclass

# List devices in the class
ls /sys/class/myautoclass/
myautodev0  myautodev1  myautodev2

# Check device information
cat /sys/class/myautoclass/myautodev0/dev
240:0  # major:minor number
~~~

### Real-World Class Examples

Linux uses classes to organize all devices:

~~~bash
/sys/class/input/    # Input devices (keyboards, mice)
/sys/class/block/    # Block devices (disks)
/sys/class/net/      # Network interfaces
/sys/class/tty/      # Terminal devices
/sys/class/sound/    # Sound cards
~~~

Your driver creates:
~~~bash
/sys/class/myautoclass/  # Your custom device class!
~~~

## Reading and Writing to Devices

Each device has its own independent buffer. You can interact with them like regular files:

### Writing to devices

**Device 0:**
~~~bash
echo "Message for device 0" > /dev/myautodev0
~~~

**Device 1:**
~~~bash
echo "Message for device 1" > /dev/myautodev1
~~~

**Device 2:**
~~~bash
echo "Message for device 2" > /dev/myautodev2
~~~

### Reading from devices

**Device 0:**
~~~bash
cat /dev/myautodev0
~~~
Output:
~~~
Message for device 0
~~~

**Device 1:**
~~~bash
cat /dev/myautodev1
~~~
Output:
~~~
Message for device 1
~~~

Each device maintains its own separate buffer!

### Kernel messages

While interacting with devices, watch the logs:

~~~bash
dmesg -w
~~~

You'll see:
~~~
[ 234.567890] myautodev0: Device opened
[ 234.567891] myautodev0: Received 20 bytes from user
[ 234.567892] myautodev0: Device closed
[ 234.567893] myautodev1: Device opened
[ 234.567894] myautodev1: Received 20 bytes from user
[ 234.567895] myautodev1: Device closed
~~~

## Understanding udev Integration

### What is udev?

**udev** is the Linux device manager running in userspace. It:
- Listens for kernel device events
- Creates/removes device nodes in `/dev/`
- Sets permissions and ownership
- Can run scripts when devices appear/disappear
- Creates symlinks and additional names

### How udev Receives Events

When you call `device_create()`:

~~~
1. Kernel → Sends uevent to udev
2. udev   → Creates /dev/myautodev0
3. udev   → Sets permissions (default: root only)
4. udev   → Can trigger rules (optional)
~~~

### Viewing udev Events

Check the event information:

~~~bash
cat /sys/class/myautoclass/myautodev0/uevent
~~~

Output:
~~~
MAJOR=240
MINOR=0
DEVNAME=myautodev0
~~~

### Custom udev Rules (Advanced)

You can create udev rules to customize device behavior. For example, create `/etc/udev/rules.d/99-myautodev.rules`:

~~~bash
# Give everyone read/write access
SUBSYSTEM=="myautoclass", MODE="0666"

# Create a friendly symlink
SUBSYSTEM=="myautoclass", KERNEL=="myautodev0", SYMLINK+="my-special-device"

# Run a script when device appears
SUBSYSTEM=="myautoclass", ACTION=="add", RUN+="/usr/local/bin/notify-device-add.sh"
~~~

## Module Cleanup

When you unload the module:

~~~bash
rmmod char_dev_auto_node
~~~

Check the kernel messages:
~~~
[ 345.678901] myautodev: Cleaning up...
[ 345.678902] myautodev: Removed /dev/myautodev0
[ 345.678903] myautodev: Removed /dev/myautodev1
[ 345.678904] myautodev: Removed /dev/myautodev2
[ 345.678905] myautodev: Module unloaded
~~~

Verify devices are gone:
~~~bash
ls /dev/myautodev*
# ls: cannot access '/dev/myautodev*': No such file or directory
~~~

The class is also removed:
~~~bash
ls /sys/class/myautoclass
# ls: cannot access '/sys/class/myautoclass': No such file or directory
~~~

Everything is cleaned up automatically!

## Code Highlights

### Creating the Device Class

~~~C
// Create /sys/class/myautoclass/
myauto_class = class_create(THIS_MODULE, CLASS_NAME);
~~~

### Creating Devices Automatically

~~~C
// Create device and /dev node automatically via udev
devices[i]->device = device_create(
    myauto_class,              // Class to add device to
    NULL,                      // No parent device
    devno,                     // Device number (major:minor)
    NULL,                      // No additional data
    DEVICE_NAME "%d",          // Device name format
    i                          // Device index
);
// Result: /dev/myautodev0 appears automatically!
~~~

### Proper Cleanup Order (Critical!)

Cleanup must be in **reverse order** of initialization:

~~~C
device_destroy()            // 1. Remove /dev node
cdev_del()                  // 2. Remove cdev
kfree()                     // 3. Free memory
class_destroy()             // 4. Remove class
unregister_chrdev_region()  // 5. Free device numbers
~~~

## Advantages Over Manual Device Creation

| Manual `mknod` | Automatic with `device_create()` |
|----------------|----------------------------------|
| Must know major/minor | Handled automatically |
| Must run after `insmod` | Done during `insmod` |
| Easy to forget | Never forgotten |
| No sysfs integration | Full `/sys/class/` hierarchy |
| No udev events | Complete udev integration |
| Static permissions | Dynamic via udev rules |
| Manual cleanup | Automatic cleanup on `rmmod` |

## Getting Module Info

To get detailed module information:

~~~bash
# Create directory structure
mkdir -p /lib/modules/$(uname -r)

# Copy module
cp /modules/char-dev-auto-node.ko /lib/modules/$(uname -r)/

# Generate module database
depmod -a

# View module info
modinfo char-dev-auto-node
~~~

Output:
~~~
filename:       /lib/modules/6.x.x/char-dev-auto-node.ko
description:    Character device with automatic node creation
author:         John Doe
license:        GPL
version:        1.0
~~~

## Troubleshooting

### Devices not appearing in `/dev/`?

1. Check if udev is running (should be automatic in most systems)
2. Verify class exists: `ls /sys/class/myautoclass/`
3. Check for errors: `dmesg | grep -i error`
4. Verify kernel messages show device creation

### "Device or resource busy" on `rmmod`?

Make sure no programs have the devices open:
~~~bash
# Close any programs using the device
# Or check what's using it:
lsof /dev/myautodev*
~~~

### Wrong permissions on `/dev/` nodes?

By default, devices are created with root-only access (`0600`). To change this:
- Create custom udev rules (see "Custom udev Rules" section above)
- Or manually change after creation: `chmod 666 /dev/myautodev*`

## Summary

This example demonstrates the **modern, production-ready** way to create character devices:

✅ **No manual `mknod`** - Devices appear automatically
✅ **Full sysfs integration** - Complete device hierarchy
✅ **udev integration** - Proper device management
✅ **Hotplug ready** - Devices appear/disappear cleanly
✅ **Permission control** - Can use udev rules
✅ **Device discovery** - Easy to find via `/sys/class/`

This is how **real Linux drivers** work in modern systems!
