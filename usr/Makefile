# initramfs/Makefile
#
# Builds the minimal test initramfs for standalone kernel testing.
#
# Usage:
#   make                    Build initramfs (default)
#   make clean              Remove build artifacts
#
# This Makefile compiles a minimal /init and packages it into a CPIO
# initramfs image using the kernel's initramfs generator.

Q = @

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

KERNEL_PATH ?= $(abspath ..)
OUT_DIR     ?= $(abspath $(KERNEL_PATH)/out)
BUILD_DIR   := $(OUT_DIR)/initramfs-build
ROOT_DIR    := $(BUILD_DIR)/rootfs
INITRAMFS_IMG := $(OUT_DIR)/initramfs.img

# Toolchain — use the kernel's cross-compiler
CC      ?= x86_64-elf-gcc
LD      ?= x86_64-elf-ld

CFLAGS  := -ffreestanding -O2 -Wall -Wextra -m64 -g -fno-stack-protector
LDFLAGS := -nostdlib -static -nostartfiles

# Source files
SRC      := init.c
LDSCRIPT := link.ld

# # Busybox applets exposed as /bin/<name> -> /busybox symlinks. busybox
# # picks the applet from argv[0], so a symlink is all an applet needs.
# BUSYBOX_APPLETS := ls cat echo pwd uname mkdir rm cp mv sleep

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

.PHONY: all clean

all: $(INITRAMFS_IMG)

# Compile /init ELF
$(BUILD_DIR)/init.elf: $(SRC) $(LDSCRIPT)
	@mkdir -p $(BUILD_DIR)
	$(Q)$(CC) -ffreestanding -O2 -nostdlib -static -nostartfiles \
		-T $(LDSCRIPT) $(CFLAGS) \
		-o $@ $(SRC)
	@echo "initramfs: built /init -> $@"

# Create rootfs layout and generate initramfs image
$(INITRAMFS_IMG): $(BUILD_DIR)/init.elf
	@mkdir -p $(ROOT_DIR)
	@cp $(BUILD_DIR)/init.elf $(ROOT_DIR)/init
	@mkdir -p $(ROOT_DIR)/modules
	@cp -r $(KERNEL_PATH)/out/modules/* $(ROOT_DIR)/modules/
	@cp -r $(KERNEL_PATH)/busy/* $(ROOT_DIR)/
# 	@mkdir -p $(ROOT_DIR)/bin
# 	@for a in $(BUSYBOX_APPLETS); do ln -sfn /busybox $(ROOT_DIR)/bin/$$a; done
	@chmod +x $(ROOT_DIR)/init
	@python3 $(KERNEL_PATH)/scripts/initramfs.py \
		--root $(ROOT_DIR) \
		--output $@
	@echo "initramfs: generated $@"

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(INITRAMFS_IMG)
	@echo "initramfs: clean complete"
