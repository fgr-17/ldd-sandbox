MODULE_NAME := char-dev-auto-node

# Kbuild variables
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := src/$(MODULE_NAME).o

# Test program variables
TEST_SRC := test/test_ioctl.c
TEST_BIN := test_ioctl

# Only include common.mk when NOT being called by kbuild
ifndef KERNELRELEASE
include ../common.mk
endif
