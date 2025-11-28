#!/bin/bash

KERNEL=$(ls /boot/vmlinuz-* 2>/dev/null | head -1)
KERNEL_VERSION=$(basename "$KERNEL" | sed 's/vmlinuz-//')

if [ -z "$KERNEL" ] || [ ! -f "$KERNEL" ]; then
    echo "ERROR: No kernel found in /boot"
    exit 1
fi

echo "Using kernel: $KERNEL (version: $KERNEL_VERSION)"

echo "Creating rootfs with modules..."
rm -rf /tmp/initramfs
mkdir -p /tmp/initramfs/{bin,dev,proc,sys,modules,usr/bin}

cp /bin/busybox /tmp/initramfs/bin/

echo "Including built modules..."
find /workspace/modules -name "*.ko" -exec cp {} /tmp/initramfs/modules/ \; 2>/dev/null
MODULE_COUNT=$(ls /tmp/initramfs/modules/*.ko 2>/dev/null | wc -l)
echo "Found $MODULE_COUNT module(s)"

echo "Including test programs..."
find /workspace/modules -name "test_*" -type f -executable -exec cp {} /tmp/initramfs/usr/bin/ \; 2>/dev/null
TEST_COUNT=$(ls /tmp/initramfs/usr/bin/test_* 2>/dev/null | wc -l)
echo "Found $TEST_COUNT test program(s)"

cat > /tmp/initramfs/init << 'EOF'
#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

echo ""
echo "========================================"
echo "  Linux Device Driver Testing VM"
echo "========================================"
echo ""
echo "Available modules:"
ls -lh /modules/*.ko 2>/dev/null || echo "  (none)"
echo ""
echo "Available test programs:"
ls -lh /usr/bin/test_* 2>/dev/null || echo "  (none)"
echo ""
echo "To test a module:"
echo "  insmod /modules/hello.ko"
echo "  dmesg | tail"
echo "  rmmod hello"
echo ""
echo "To exit: Ctrl+A then X"
echo ""

exec /bin/sh
EOF
chmod +x /tmp/initramfs/init

cd /tmp/initramfs
find . -print0 | cpio --null -o --format=newc | gzip > /workspace/initramfs.cpio.gz
cd /workspace/modules
rm -rf /tmp/initramfs
echo "Initramfs created!"

echo ""
echo "Starting QEMU..."
echo ""

qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -initrd /workspace/initramfs.cpio.gz \
    -m 512M \
    -nographic \
    -append "console=ttyS0" \
    -enable-kvm 2>/dev/null || \
qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -initrd /workspace/initramfs.cpio.gz \
    -m 512M \
    -nographic \
    -append "console=ttyS0"
