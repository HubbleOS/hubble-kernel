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

# Linux-ABI demo programs (usr/demos/*.c -> /bin/<name>), built static
# against musl. Skipped, with a note, when the musl cross compiler is not
# found: the image does not need them.
MUSL_CC ?= $(firstword $(shell command -v x86_64-linux-musl-gcc 2>/dev/null) \
	$(wildcard $(HOME)/opt/x86_64-linux-musl-cross/bin/x86_64-linux-musl-gcc))
DEMO_SRCS := $(wildcard demos/*.c)
DEMOS := $(if $(MUSL_CC),$(patsubst demos/%.c,$(BUILD_DIR)/demos/%,$(DEMO_SRCS)))

# /bin/desktop: libgui (lib/libgui) in its GUI_DEMO configuration, with
# the HubbleOS platform layer from apps/desktop/platform. Also musl.
LIBGUI_SRCS := $(shell find lib/libgui/src -name '*.c')
DESKTOP_SRCS := $(shell find apps/desktop -name '*.c')
DESKTOP_HDRS := $(shell find lib/libgui apps/desktop -name '*.h')
DESKTOP := $(if $(MUSL_CC),$(BUILD_DIR)/demos/desktop)

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

$(BUILD_DIR)/demos/%: demos/%.c
	@mkdir -p $(dir $@)
	$(Q)$(MUSL_CC) -static -O2 -Wall -Wextra -o $@ $<
	@echo "initramfs: built demo $* -> $@"

$(BUILD_DIR)/demos/desktop: $(LIBGUI_SRCS) $(DESKTOP_SRCS) $(DESKTOP_HDRS)
	@mkdir -p $(dir $@)
	$(Q)$(MUSL_CC) -static -O2 -DGUI_DEMO \
		-Iapps/desktop -Ilib/libgui/include -Ilib/libgui/src \
		-o $@ $(DESKTOP_SRCS) $(LIBGUI_SRCS)
	@echo "initramfs: built desktop -> $@"

# Everything that ends up in the image. Depending on init.elf alone meant
# rebuilt modules (or a new busybox) were never copied in once the image
# existed - the kernel then booted with stale .ko files.
INITRAMFS_INPUTS := $(BUILD_DIR)/init.elf $(DEMOS) $(DESKTOP) \
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
	@[ -n "$(MUSL_CC)" ] || [ -z "$(DEMO_SRCS)" ] || echo "initramfs: note: x86_64-linux-musl-gcc not found; demos/ not built"
	@for d in $(DEMOS) $(DESKTOP); do cp "$$d" $(ROOT_DIR)/bin/; done
	@chmod +x $(ROOT_DIR)/init
	@python3 $(KERNEL_PATH)/scripts/initramfs.py \
		--root $(ROOT_DIR) \
		--output $@
	@echo "initramfs: generated $@"

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(INITRAMFS_IMG)
	@echo "initramfs: clean complete"
