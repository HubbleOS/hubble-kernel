# drivers/video/module.mk
mod-y := drivers/video
mod-output-y := $(OUT_DIR)/modules/video.ko
mod-ldflags-y := -nostdlib
