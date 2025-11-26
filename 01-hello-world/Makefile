obj-m := hello.o
hello-objs := src/hello.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f src/*.o src/.*.cmd src/*.mod.c src/*.mod

# Convenience targets for testing
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
