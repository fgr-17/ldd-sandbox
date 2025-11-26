obj-m := hello.o
hello-objs := src/hello.o

KDIR ?= $(shell \
    KERNEL=$$(ls /boot/vmlinuz-* 2>/dev/null | head -1 | sed 's|/boot/vmlinuz-||'); \
    if [ -d "/lib/modules/$$KERNEL/build" ]; then \
        echo "/lib/modules/$$KERNEL/build"; \
    else \
        echo "/lib/modules/$$(uname -r)/build"; \
    fi)

PWD := $(shell pwd)

all:
	@echo "Building for kernel: $(KDIR)"
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f src/*.o src/.*.cmd src/*.mod.c src/*.mod

load:
	sudo insmod $(obj-m:.o=.ko)
	dmesg | tail -10

unload:
	sudo rmmod $(obj-m:.o=)

reload: unload load

test: reload

info:
	modinfo $(obj-m:.o=.ko)

.PHONY: all clean load unload reload test info
