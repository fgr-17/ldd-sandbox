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

## Device Tree Integration

For real hardware, devices are described in device tree:

~~~dts
myplatform@30000000 {
    compatible = "mycompany,myplatform-device";
    reg = <0x30000000 0x1000>;        // Memory region
    interrupts = <10>;                // IRQ number
    clocks = <&clock_controller 5>;   // Clock reference
};
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

### 3. Resource Management

**Character Driver:**
~~~C
// Manual cleanup required
kmalloc(...);
request_irq(...);
// Must free in exit function
~~~

**Platform Driver:**
~~~C
// Automatic cleanup with devm_*
devm_kzalloc(&pdev->dev, ...);    // Auto freed on remove
devm_request_irq(&pdev->dev, ...); // Auto freed on remove
devm_ioremap_resource(...);       // Auto unmapped on remove
~~~

## Monitoring and Debugging

### Watch Device Creation

~~~bash
# Monitor udev events
udevadm monitor

# Check device model
ls /sys/bus/platform/devices/
ls /sys/bus/platform/drivers/

# Device attributes
ls /sys/devices/platform/myplatform/
~~~

### Debug Output

Add debug prints to see the flow:

~~~C
static int myplatform_probe(struct platform_device *pdev)
{
    pr_info("myplatform: Probing device %s\n", pdev->name);

    // Check resources
    struct resource *res;
    int i = 0;
    while ((res = platform_get_resource(pdev, IORESOURCE_MEM, i))) {
        pr_info("myplatform: Memory resource %d: 0x%llx-0x%llx\n",
                i, res->start, res->end);
        i++;
    }

    // ... rest of probe
}
~~~

## Cleanup

~~~bash
# Remove device node
rm /dev/myplatform

# Remove module
rmmod platform-driver

# Check cleanup
dmesg | tail
lsmod | grep platform  # Should be empty
~~~

## Real-World Platform Devices

Platform drivers are used for:
- **GPIO controllers** (`drivers/gpio/`)
- **I2C controllers** (`drivers/i2c/`)
- **SPI controllers** (`drivers/spi/`)
- **Clock controllers** (`drivers/clk/`)
- **Interrupt controllers** (`drivers/irqchip/`)
- **SoC-specific devices** (watchdog, RTC, etc.)

## Advantages of Platform Drivers

1. **Resource Management**: Automatic cleanup with `devm_*` functions
2. **Device Model Integration**: Proper sysfs entries and device hierarchy
3. **Hotplug Support**: Devices can appear/disappear dynamically
4. **Power Management**: Automatic suspend/resume handling
5. **Device Tree Support**: Modern hardware description
6. **Modular**: Driver and device are separate entities

## Limitations of This Example

1. **No Real Hardware**: Creates fake platform device for testing
2. **Simple Resources**: Only demonstrates basic memory mapping
3. **No IRQ Handling**: Doesn't show interrupt request/handling
4. **Single Instance**: Only one device instance
5. **No Power Management**: No suspend/resume callbacks

Future examples will address these!

## Detailed Flow

### Driver Loading

~~~
1. Module loaded
2. platform_driver_register() called
3. Driver registered with platform bus
4. platform_device_register_simple() called
5. Device registered with platform bus
6. Bus matches driver to device
7. myplatform_probe() called
8. Character device registered
9. Device ready for use
~~~

### Device Access

~~~
User Space                    Kernel Space
    ↓                              ↓
echo "data" > /dev/myplatform    VFS
    ↓                              ↓
open() / write()               Character device fops
    ↓                              ↓
                                   platform driver
    ↓                              ↓
                                   RAM buffer
~~~

### Driver Unloading

~~~
1. rmmod platform-driver
2. platform_device_unregister() called
3. myplatform_remove() called
4. Character device unregistered
5. Resources freed automatically
6. platform_driver_unregister() called
7. Module unloaded
~~~

## Understanding Platform Bus

### What is the Platform Bus?

The platform bus is Linux's mechanism for:
- Devices without their own bus (PCI, USB, etc.)
- SoC-integrated peripherals
- Devices described by platform data or device tree

### Bus Matching

Drivers and devices are matched by:
1. **Name matching**: `driver.name == device.name`
2. **Device tree**: `driver.of_match_table` vs `device.compatible`
3. **ID table**: `driver.id_table` vs `device.id_entry`

### Resource Types

Platform devices can provide:
- `IORESOURCE_MEM`: Memory regions
- `IORESOURCE_IRQ`: Interrupt lines
- `IORESOURCE_DMA`: DMA channels
- `IORESOURCE_IO`: I/O ports

## Platform Driver Best Practices

1. **Use devm_* functions**: Automatic resource cleanup
2. **Check return values**: All resource requests can fail
3. **Handle probe deferral**: Use `dev_err_probe()` for better error messages
4. **Device tree first**: Prefer DT over platform data
5. **Proper error handling**: Clean up on any failure

This example provides a solid foundation for understanding platform drivers. The concepts here apply to most real-world platform device drivers in the Linux kernel!