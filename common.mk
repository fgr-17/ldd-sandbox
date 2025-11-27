# common.mk - Shared kernel module build logic for user targets
# Kbuild variables should be defined in each module's Makefile

ifndef MODULE_NAME
$(error MODULE_NAME must be defined before including common.mk)
endif

# Kernel directory
KDIR ?= $(shell \
    if [ -d "/lib/modules/$$(uname -r)/build" ]; then \
        echo "/lib/modules/$$(uname -r)/build"; \
    else \
        find /lib/modules -maxdepth 2 -name build -type l 2>/dev/null | head -1; \
    fi)

# Current directory
PWD := $(shell pwd)

# Build directory
BUILD_DIR := $(PWD)/build

all:
	@echo "Building $(MODULE_NAME) for kernel: $(KDIR)"
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	@echo "Moving build artifacts to $(BUILD_DIR)/"
	@# Move all build artifacts to build directory
	@find . -maxdepth 1 \( -name '*.ko' -o -name '*.o' -o -name '*.mod' -o -name '*.mod.c' -o -name '.*.cmd' -o -name 'modules.order' -o -name 'Module.symvers' \) -exec mv {} $(BUILD_DIR)/ \; 2>/dev/null || true
	@find src -type f \( -name '*.o' -o -name '.*.cmd' -o -name '*.mod.c' \) -exec mv {} $(BUILD_DIR)/ \; 2>/dev/null || true
	@echo "Build complete: $(BUILD_DIR)/$(MODULE_NAME).ko"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f src/*.o src/.*.cmd src/*.mod.c src/*.mod
	rm -rf $(BUILD_DIR)
	@echo "Cleaned $(MODULE_NAME)"

load:
	@if [ ! -f "$(BUILD_DIR)/$(MODULE_NAME).ko" ]; then \
		echo "Error: Module not built. Run 'make' first."; \
		exit 1; \
	fi
	sudo insmod $(BUILD_DIR)/$(MODULE_NAME).ko
	dmesg | tail -10

unload:
	sudo rmmod $(MODULE_NAME)

reload: unload load

test: reload

info:
	@if [ ! -f "$(BUILD_DIR)/$(MODULE_NAME).ko" ]; then \
		echo "Error: Module not built. Run 'make' first."; \
		exit 1; \
	fi
	modinfo $(BUILD_DIR)/$(MODULE_NAME).ko

.PHONY: all clean load unload reload test info
