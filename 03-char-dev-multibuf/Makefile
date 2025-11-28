MODULE_NAME := char-dev-multibuf

# Kbuild variables
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := src/$(MODULE_NAME).o

# Only include common.mk when NOT being called by kbuild
ifndef KERNELRELEASE
include ../common.mk
endif
