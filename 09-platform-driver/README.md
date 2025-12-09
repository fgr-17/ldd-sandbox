# 09-platform-driver - Platform Bus Device Driver

This example demonstrates a **platform device driver** - a driver for integrated devices that use the Linux platform bus framework. Unlike PCI or USB devices that are discoverable, platform devices are typically integrated into the SoC (System on Chip) and described via platform data or device tree.

**Note:** This example creates both a platform driver AND a platform device programmatically for testing purposes, since the QEMU environment doesn't have device tree support.

## What's New: Platform Drivers

### Character Devices (Previous Examples)
- Use `register_chrdev()` directly
- Manual device node creation with `mknod`
- Work with any bus (or no bus)

### Platform Drivers (This Example)
- Use `platform_driver_register()` and `platform_device_register_simple()`
- Automatic device lifecycle management
- Resource management (memory, IRQs, DMA)
- Device tree or platform data integration
- Better integration with device model

## Key Concepts

### Platform Bus

The platform bus (`platform_bus_type`) is for devices that:
- Are integrated into the SoC
- Don't have their own bus infrastructure
- Are described by platform data or device tree

~~~
Platform Bus
├── platform_device (represents hardware)
├── platform_driver (handles hardware)
└── Resources (memory, IRQs, etc.)
~~~

### Platform Device Structure

~~~C
struct platform_device {
    const char *name;           // Device name
    int id;                     // Instance ID
    struct resource *resource;  // Memory, IRQs, etc.
    // ... other fields
};
~~~

### Platform Driver Structure

~~~C
struct platform_driver {
    int (*probe)(struct platform_device *);     // Called when device found
    int (*remove)(struct platform_device *);    // Called when device removed
    struct device_driver driver;                // Core driver info
    // ... other fields
};
~~~

## Requirements

- **Kernel Version**: Any modern kernel (2.6+)
- **Headers**: `linux/platform_device.h`, `linux/cdev.h`
- **Build System**: Works with the sandbox's Makefile system

## Building and Testing

### 1. Build the module

~~~bash
cd 09-platform-driver
make
~~~

### 2. Load in QEMU

~~~bash
./run-qemu.sh
insmod /modules/platform-driver.ko
dmesg | tail
~~~

Expected output:
~~~
[ 123.456] myplatform: Loading platform driver
[ 123.457] myplatform: Probing platform device
[ 123.458] myplatform: Mapped memory at 0x30000000
[ 123.459] myplatform: Registered with major 243, minor 0
[ 123.460] myplatform: Create device: mknod /dev/myplatform c 243 0
~~~

### 3. Test the device

~~~bash
# Create device node (use the major number from dmesg)
mknod /dev/myplatform c 243 0

# Test writing
echo "Hello Platform Driver!" > /dev/myplatform

# Test reading
cat /dev/myplatform
# Should output: Hello Platform Driver!
~~~

### 4. Check device info

~~~bash
# Verify device created
ls -l /dev/myplatform
# Should show: crw-rw---- 1 root root 243, 0 ...

# Check kernel logs
dmesg | grep myplatform

# List loaded modules
lsmod | grep platform
~~~

## Understanding the Code

### Module Initialization

The platform driver creates both driver and device:

~~~C
static int __init myplatform_init(void)
{
    int ret;

    // 1. Register the driver
    ret = platform_driver_register(&myplatform_driver);
    if (ret < 0)
        return ret;

    // 2. Create a test device (normally done by device tree/platform)
    test_pdev = platform_device_register_simple("myplatform", -1, NULL, 0);
    if (IS_ERR(test_pdev)) {
        platform_driver_unregister(&myplatform_driver);
        return PTR_ERR(test_pdev);
    }

    return 0;
}
~~~

### Platform Driver Registration

~~~C
static struct platform_driver myplatform_driver = {
    .driver = {
        .name = "myplatform",
        .of_match_table = myplatform_of_match,  // Device tree matching
    },
    .probe = myplatform_probe,
    .remove = myplatform_remove,
};
~~~

### Probe Function

Called when a matching device is found:

~~~C
static int myplatform_probe(struct platform_device *pdev)
{
    // 1. Allocate private data
    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);

    // 2. Get resources (memory, IRQs)
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res) {
        dev->regs = devm_ioremap_resource(&pdev->dev, res);
    }

    // 3. Register character device
    alloc_chrdev_region(&dev->dev_num, 0, 1, DEVICE_NAME);
    cdev_init(&dev->cdev, &platform_fops);
    cdev_add(&dev->cdev, dev->dev_num, 1);

    return 0;
}
~~~

### Resource Management

Platform devices can provide resources:

~~~C
// Memory resource
struct resource *res;
res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
if (res) {
    void __iomem *regs = devm_ioremap_resource(&pdev->dev, res);
    // Use regs for MMIO access
}

// IRQ resource
int irq = platform_get_irq(pdev, 0);
if (irq > 0) {
    // Request IRQ
    devm_request_irq(&pdev->dev, irq, handler, ...);
}
~~~

Driver matches using `of_match_table`:

~~~C
static const struct of_device_id myplatform_of_match[] = {
    { .compatible = "mycompany,myplatform-device" },
    { }
};
MODULE_DEVICE_TABLE(of, myplatform_of_match);
~~~

## Platform vs Character Drivers

| Aspect | Character Driver | Platform Driver |
|--------|------------------|-----------------|
| **Registration** | `register_chrdev()` | `platform_driver_register()` |
| **Device Creation** | Manual `mknod` | Automatic or device tree |
| **Resource Mgmt** | Manual | `devm_*` functions |
| **Hotplug** | No | Yes |
| **Device Model** | Basic | Full integration |
| **Real Hardware** | Any | SoC integrated |

## Key Differences from Character Drivers

### 1. Registration Process

**Character Driver:**
~~~C
// Simple registration
major = register_chrdev(0, "mychar", &fops);
~~~

**Platform Driver:**
~~~C
// 1. Register driver
platform_driver_register(&myplatform_driver);

// 2. Device created separately (device tree/platform code)
//    or programmatically for testing
platform_device_register_simple("myplatform", -1, NULL, 0);
~~~

### 2. Device Lifecycle

**Character Driver:**
- Load module → device available
- Unload module → device gone

**Platform Driver:**
- Load module → driver registered
- Device appears → `probe()` called
- Device removed → `remove()` called
- Unload module → driver unregistered
