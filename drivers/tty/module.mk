# drivers/tty/module.mk
mod-y := drivers/tty
mod-output-y := $(OUT_DIR)/modules/tty.ko
mod-ldflags-y := -nostdlib
