# 10-platform-instance - Platform Bus Matching example

This example demonstrates two different modules with platform devices and drivers:

* [`platform-setup`](./src/platform-setup.c): creates instances of platform devices and registers that on the platform bus
* [`platform-drv`](./src/platform-drv.c): contains the code of the platform driver 

## Device/driver matching on platform bus

When a new device or driver is loaded into the platform bus (on driver or device table), the bus tries to match according to some criteria. In this example, driver and device are matching by .name field. 

~~~mermaid
graph TD
    A[platform bus]
    
    subgraph device list
        B1[platform device 1 - .name=xyz]
    end
    
    subgraph driver list
        C1[platform driver 1 - .name=abc]
        C2[platform driver 2 - .name=ijk]
        C3[platform driver 3 - .name=xyz]
    end
    
    A --> B1
    A --> C1
    
    C1 --> C2
    C2 --> C3
    
    style B1 stroke:#FF0000,stroke-width:2px,fill:#FFF4E0
    style C3 stroke:#FF0000,stroke-width:2px,fill:#FFF4E0
~~~

## Platform Bus

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

## Building and Testing

### 1. Build the two modules

~~~bash
docker exec -it ldd-sandbox bash
cd 10-platform-driver
make
~~~

### 2. Load in QEMU

The order of loading the two modules shows a difference in the output:

1. If loading first the driver and then the devices, it doesn't show any matching when the driver is loaded, but shows logs when the devices are registered:

~~~bash
./run-qemu.sh
insmod /modules/platform-drv.ko
dmesg | tail
~~~

    Expected output:

~~~
platform-driver.ko  platform-drv.ko
[   16.902016] platform_drv: loading out-of-tree module taints kernel.
[   16.910718] platform_drv: module verification failed: signature and/or required key missing - tainting kernel~~~
~~~

    At this point, the code is completely silent, it's waiting for some device to match with.
    Then, when registering the platform devices:

~~~bash
insmod modules/platform-setup.ko 
[    8.627010] platform driver loaded
[   16.137928] probe cb: Device detected
[   16.142835] sn: PCDEV1111
[   16.148233] perm: 11
[   16.151917] size: 512
[   16.155234] successful probe()!
[   16.158747] probe cb: Device detected
[   16.163221] sn: PCDEV2222
[   16.168378] perm: 11
[   16.172136] size: 1024
[   16.175487] successful probe()!
[   16.178884] platform_driver_init: Devices registered
~~~

    In this case, the devices are matched by the name. If you change one of the device names in `platform-setup.c`, you should see one less device detected.

2. If loading the devices first, and the driver after that:

~~~bash
insmod modules/platform-setup.ko 
[    7.949137] platform_setup: loading out-of-tree module taints kernel.
[    7.956326] platform_setup: module verification failed: signature and/or required key missing - tainting kernel
[    7.960350] platform_driver_init: Devices registered

insmod modules/platform-drv.ko 
[   15.801348] probe cb: Device detected
~~~

3. Removing the modules

Removing the `platform-setup` module should remove the devices, showing the following log:

~~~bash
rmmod platform-setup
[   25.602333] remove cb: device removed
[   25.602388] platform_dev_release: Device released
[   25.614235] remove cb: device removed
[   25.614264] platform_dev_release: Device released
[   25.626136] platform_driver_exit: Devices unregistered
~~~

If the driver is removed before the devices, the `remove()` callback is called as well:

~~~bash
rmmod modules/platform-drv.ko 
[  724.908915] remove cb: device removed
[  724.908942] remove cb: device removed
~~~
