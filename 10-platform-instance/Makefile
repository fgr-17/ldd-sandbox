MODULE_NAME := platform-instance

obj-m := platform-drv.o platform-setup.o
platform-drv-objs := src/platform-drv.o
platform-setup-objs := src/platform-setup.o

# Only include common.mk when NOT being called by kbuild
ifndef KERNELRELEASE
include ../common.mk
endif