FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    linux-headers-generic \
    linux-image-generic \
    linux-modules-extra-$(uname -r) \
    qemu-system-x86 \
    busybox-static \
    wget \
    cpio \
    gzip \
    kmod \
    && rm -rf /var/lib/apt/lists/*

# Enable 9p modules if they exist
RUN depmod -a 2>/dev/null || true

WORKDIR /workspace/modules

CMD ["/bin/bash"]
