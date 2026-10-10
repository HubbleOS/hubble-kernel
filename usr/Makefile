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

# Busybox applets, linked as /<dir>/<name> -> /busybox. Busybox picks the
# applet from argv[0], so a symlink is all an applet needs. The list comes
# from the binary itself (--list-full gives bin/ls, sbin/init, ...), so it
# always matches what busybox was built with. Older builds without
# --list-full fall back to --list (all in bin/). This runs busybox on the
# build host, which only works when it is built for the host's CPU.
BUSYBOX := $(KERNEL_PATH)/busy/busybox
BUSYBOX_APPLETS := $(shell $(BUSYBOX) --list-full 2>/dev/null || \
	$(BUSYBOX) --list 2>/dev/null | sed 's|^|bin/|')

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

# Everything that ends up in the image. Depending on init.elf alone meant
# rebuilt modules (or a new busybox) were never copied in once the image
# existed - the kernel then booted with stale .ko files.
INITRAMFS_INPUTS := $(BUILD_DIR)/init.elf \
	$(wildcard $(KERNEL_PATH)/out/modules/*.ko) \
	$(wildcard $(KERNEL_PATH)/busy/*) \
	$(KERNEL_PATH)/scripts/initramfs.py \
	$(lastword $(MAKEFILE_LIST))

# Create rootfs layout and generate initramfs image. The rootfs is rebuilt
# from scratch so files removed from the inputs do not linger.
$(INITRAMFS_IMG): $(INITRAMFS_INPUTS)
	@rm -rf $(ROOT_DIR)
	@mkdir -p $(ROOT_DIR)
	@cp $(BUILD_DIR)/init.elf $(ROOT_DIR)/init
	@mkdir -p $(ROOT_DIR)/modules
	@cp -r $(KERNEL_PATH)/out/modules/* $(ROOT_DIR)/modules/
	@cp -r $(KERNEL_PATH)/busy/* $(ROOT_DIR)/
	@# Mountpoints: kernel/init/fs.c and modules mount over these.
	@mkdir -p $(ROOT_DIR)/bin $(ROOT_DIR)/proc $(ROOT_DIR)/dev \
		$(ROOT_DIR)/ext2 $(ROOT_DIR)/pipe
	@[ -n "$(BUSYBOX_APPLETS)" ] || echo "initramfs: warning: could not list busybox applets (no --list-full/--list, or not runnable on this host); no applet links created"
	@for a in $(BUSYBOX_APPLETS); do \
		mkdir -p "$(ROOT_DIR)/$$(dirname "$$a")"; \
		ln -sfn /busybox "$(ROOT_DIR)/$$a"; \
	done
	@chmod +x $(ROOT_DIR)/init
	@python3 $(KERNEL_PATH)/scripts/initramfs.py \
		--root $(ROOT_DIR) \
		--output $@
	@echo "initramfs: generated $@"

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(INITRAMFS_IMG)
	@echo "initramfs: clean complete"
