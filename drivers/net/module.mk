# drivers/net/module.mk
mod-y := drivers/net
mod-output-y := $(OUT_DIR)/modules/net.ko
mod-ldflags-y := -nostdlib
