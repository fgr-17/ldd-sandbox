#!/bin/bash

KERNEL=$(ls /boot/vmlinuz-* 2>/dev/null | head -1)

if [ -z "$KERNEL" ] || [ ! -f "$KERNEL" ]; then
    echo "ERROR: No kernel found in /boot"
    exit 1
fi

echo "Using kernel: $KERNEL"

# Create minimal rootfs
if [ ! -f "/workspace/initramfs.cpio.gz" ]; then
    echo "Creating minimal rootfs..."
    mkdir -p /tmp/initramfs/{bin,dev,proc,sys}

    cp /bin/busybox /tmp/initramfs/bin/

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
echo "This is an isolated QEMU environment"
echo "for testing kernel modules safely."
echo ""
echo "To test modules:"
echo "  1. Build in container: make"
echo "  2. Copy .ko here (or rebuild initramfs)"
echo "  3. insmod your_module.ko"
echo "  4. dmesg | tail"
echo ""

exec /bin/sh
EOF
    chmod +x /tmp/initramfs/init

    cd /tmp/initramfs
    find . -print0 | cpio --null -o --format=newc | gzip > /workspace/initramfs.cpio.gz
    cd /workspace/modules
    rm -rf /tmp/initramfs
    echo "Rootfs created!"
fi

echo "Starting QEMU..."
echo "Press Ctrl+A then X to exit"
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
