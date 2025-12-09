#include <linux/module.h>
#include <linux/platform_device.h>
#include "platform.h"

#define RDWR 0x11

void platform_dev_release(struct device *dev) {
    pr_info("platform_dev_release: Device released\n");
}

static struct platform_data pdata[2] = {
    [0] = {.size = 512, .perm = RDWR, .serial_number = "PCDEV1111"},
    [1] = {.size = 1024, .perm = RDWR, .serial_number = "PCDEV2222"}
};

struct platform_device platform_dev_1 = {
    .name = "pdev-B1x",
    .id = 0,
    .dev = {
        .platform_data = &pdata[0],
        .release = platform_dev_release
    }
};

struct platform_device platform_dev_2 = {
    .name = "pdev-A1x",
    .id = 1,
    .dev = {
        .platform_data = &pdata[1],
        .release = platform_dev_release
    } 
};

struct platform_device*platform_devs[] = {
    &platform_dev_1, 
    &platform_dev_2
};

static int __init platform_driver_init(void) {
    // Add devices to the platform bus
    // platform_device_register(&platform_dev_1);
    // platform_device_register(&platform_dev_2);

    // adding a chunk of devices:
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));
    pr_info("platform_driver_init: Devices registered\n");
    return 0;
}

static void __exit platform_driver_exit(void) {
    platform_device_unregister(&platform_dev_1);
    platform_device_unregister(&platform_dev_2);
    pr_info("platform_driver_exit: Devices unregistered\n");
    return;
}

module_init(platform_driver_init);
module_exit(platform_driver_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module that registers two platform devices");