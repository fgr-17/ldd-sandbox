FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-12 \
    g++-12 \
    linux-headers-generic \
    linux-image-generic \
    qemu-system-x86 \
    busybox-static \
    cpio \
    gzip \
    kmod \
    bc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/modules

CMD ["/bin/bash"]
