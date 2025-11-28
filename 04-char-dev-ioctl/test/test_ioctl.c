#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define IOCTL_MAGIC 'k'
#define IOCTL_SET_SPEED     _IOW(IOCTL_MAGIC, 1, int)
#define IOCTL_GET_SPEED     _IOR(IOCTL_MAGIC, 2, int)
#define IOCTL_SET_MODE      _IOW(IOCTL_MAGIC, 3, int)
#define IOCTL_GET_MODE      _IOR(IOCTL_MAGIC, 4, int)
#define IOCTL_RESET         _IO(IOCTL_MAGIC, 5)
#define IOCTL_GET_VERSION   _IOR(IOCTL_MAGIC, 6, int)

int main() {
    int fd, value;

    fd = open("/dev/myioctl", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    // Get version
    if (ioctl(fd, IOCTL_GET_VERSION, &value) == 0) {
        printf("Driver version: %d.%02d\n", value / 100, value % 100);
    }

    // Set speed
    value = 500;
    printf("Setting speed to %d\n", value);
    ioctl(fd, IOCTL_SET_SPEED, &value);

    // Get speed
    ioctl(fd, IOCTL_GET_SPEED, &value);
    printf("Current speed: %d\n", value);

    // Set mode
    value = 2;
    printf("Setting mode to %d\n", value);
    ioctl(fd, IOCTL_SET_MODE, &value);

    // Get mode
    ioctl(fd, IOCTL_GET_MODE, &value);
    printf("Current mode: %d\n", value);

    // Reset device
    printf("Resetting device\n");
    ioctl(fd, IOCTL_RESET, NULL);

    // Verify reset
    ioctl(fd, IOCTL_GET_SPEED, &value);
    printf("Speed after reset: %d\n", value);

    close(fd);
    return 0;
}
