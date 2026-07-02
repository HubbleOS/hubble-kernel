# drivers/input/module.mk
mod-y := drivers/input
mod-output-y := $(OUT_DIR)/modules/input.ko
mod-ldflags-y := -nostdlib
