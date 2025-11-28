MODULE_NAME := char-dev-ioctl

# Kbuild variables
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := src/$(MODULE_NAME).o

# Test program variables
TEST_SRC := test/test_ioctl.c
TEST_BIN := test_ioctl

# Only include common.mk when NOT being called by kbuild
ifndef KERNELRELEASE
include ../common.mk

# Extend the all target to also build test program
all: test-program

test-program:
	@echo "Building test program: $(TEST_BIN)"
	@mkdir -p $(BUILD_DIR)
	gcc -static -o $(BUILD_DIR)/$(TEST_BIN) $(TEST_SRC) -Wall
	chmod +x $(BUILD_DIR)/$(TEST_BIN)
	@echo "Test program built: $(BUILD_DIR)/$(TEST_BIN)"

# Extend the clean target
clean: clean-test

clean-test:
	rm -f $(BUILD_DIR)/$(TEST_BIN)

.PHONY: test-program clean-test

endif
