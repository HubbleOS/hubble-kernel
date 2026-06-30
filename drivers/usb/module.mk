# drivers/usb/module.mk
mod-y := drivers/usb
mod-output-y := $(OUT_DIR)/modules/usb.ko
mod-ldflags-y := -nostdlib
