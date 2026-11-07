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
# Configuration: defaults here, overridden by $(KERNEL_PATH)/config.mk (see
# config.mk.example) and by the command line.
# ---------------------------------------------------------------------------

KERNEL_PATH ?= $(abspath ..)
ROOT_DIR    := $(KERNEL_PATH)
-include $(KERNEL_PATH)/config.mk

OUT_DIR     ?= $(abspath $(KERNEL_PATH)/out)
BUILD_DIR   := $(OUT_DIR)/initramfs-build
ROOTFS_DIR  := $(BUILD_DIR)/rootfs
INITRAMFS_IMG := $(OUT_DIR)/initramfs.img

# Toolchain: the kernel's cross compiler (the top-level Makefile passes CC;
# make's built-in default "cc" means we were run on our own).
CROSS   ?= x86_64-elf-
ifeq ($(origin CC),default)
  CC    := $(CROSS)gcc
endif

CFLAGS  := -ffreestanding -O2 -Wall -Wextra -m64 -g -fno-stack-protector
LDFLAGS := -nostdlib -static -nostartfiles

# Source files
SRC      := init.c
LDSCRIPT := link.ld

# Optional parts, each y, n or auto (see config.mk.example).
CONFIG_BUSYBOX ?= auto
CONFIG_DEMOS   ?= auto
CONFIG_GUI     ?= auto
CONFIG_WIN     ?= auto
CONFIG_DISK    ?= auto
OPTIONS := CONFIG_BUSYBOX CONFIG_DEMOS CONFIG_GUI CONFIG_WIN CONFIG_DISK
$(foreach o,$(OPTIONS),$(if $(filter y n auto,$(strip $($(o)))),,\
  $(error $(o) must be y, n or auto, not '$($(o))')))

MUSL_CC ?= $(firstword $(shell command -v x86_64-linux-musl-gcc 2>/dev/null) \
	$(wildcard $(HOME)/opt/x86_64-linux-musl-cross/bin/x86_64-linux-musl-gcc))
MINGW_CC ?= $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
BUSYBOX_DIR ?= $(KERNEL_PATH)/busy
BUSYBOX := $(BUSYBOX_DIR)/busybox

# $(call use,OPTION,thing): the thing (a compiler, a file) an option needs
# when the option is on and the thing exists, else nothing. OPTION=y
# without the thing is an error, except for `make clean` and `make config`.
MISSING :=
use = $(if $(filter n,$($(1))),,$(or $(2),$(if $(filter y,$($(1))),$(eval MISSING += $(1)))))
need_CONFIG_BUSYBOX = $(BUSYBOX)
need_CONFIG_DEMOS   = a static musl compiler (set MUSL_CC)
need_CONFIG_GUI     = a static musl compiler (set MUSL_CC)
need_CONFIG_WIN     = a mingw-w64 compiler (set MINGW_CC)
need_CONFIG_DISK    = mke2fs, mkfs.fat and mcopy (e2fsprogs, dosfstools, mtools)

HAVE_BUSYBOX := $(call use,CONFIG_BUSYBOX,$(wildcard $(BUSYBOX)))
DEMO_CC := $(call use,CONFIG_DEMOS,$(MUSL_CC))
GUI_CC  := $(call use,CONFIG_GUI,$(MUSL_CC))
WIN_CC  := $(call use,CONFIG_WIN,$(MINGW_CC))
DISK_TOOLS := $(if $(and $(shell command -v mke2fs),$(shell command -v mkfs.fat),$(shell command -v mcopy)),yes)
MAKE_DISK := $(call use,CONFIG_DISK,$(DISK_TOOLS))

ifneq ($(MISSING),)
ifeq ($(filter clean config,$(MAKECMDGOALS)),)
  $(foreach o,$(MISSING),$(info $(o)=y needs $(need_$(o)), which was not found))
  $(error set them to auto or n, in config.mk or on the command line)
endif
endif

# Everything in BUSYBOX_DIR is copied to the image root. Without busybox
# the image gets no applets and boots the minimal /init instead.
BUSY_FILES := $(if $(HAVE_BUSYBOX),$(wildcard $(BUSYBOX_DIR)/*))

# Busybox applets, linked as /<dir>/<name> -> /busybox. Busybox picks the
# applet from argv[0], so a symlink is all an applet needs. The list comes
# from the binary itself (--list-full gives bin/ls, sbin/init, ...), so it
# always matches what busybox was built with. Older builds without
# --list-full fall back to --list (all in bin/). This runs busybox on the
# build host, which only works when it is built for the host's CPU.
BUSYBOX_APPLETS := $(if $(HAVE_BUSYBOX),$(shell $(BUSYBOX) --list-full 2>/dev/null || \
	$(BUSYBOX) --list 2>/dev/null | sed 's|^|bin/|'))

# Linux-ABI demo programs (usr/demos/*.c -> /bin/<name>), built static
# against musl.
DEMO_SRCS := $(wildcard demos/*.c)
DEMOS := $(if $(DEMO_CC),$(patsubst demos/%.c,$(BUILD_DIR)/demos/%,$(DEMO_SRCS)))

# libgui (lib/libgui): the shared core, plus a server half for
# /bin/desktop (the display server) and a client half for applications
# (GUI_APPS, each apps/<name>/*.c). Also musl.
LIBGUI_DIR := lib/libgui/src
LIBGUI_COMMON := $(shell find $(LIBGUI_DIR)/core $(LIBGUI_DIR)/ui $(LIBGUI_DIR)/utils -name '*.c')
LIBGUI_SERVER := $(shell find $(LIBGUI_DIR)/server -name '*.c')
LIBGUI_CLIENT := $(shell find $(LIBGUI_DIR)/client -name '*.c')
LIBGUI_HDRS := $(shell find lib/libgui -name '*.h')
LIBGUI_CFLAGS := -static -O2 -Ilib/libgui/include -I$(LIBGUI_DIR)
# libgui's TrueType fonts (ui/text/font.h), installed in /usr/share/fonts.
FONTS := $(wildcard fonts/*.ttf) fonts/LICENSE.DejaVu

GUI_APPS ?= panel term
DESKTOP := $(if $(GUI_CC),$(BUILD_DIR)/demos/desktop)
GUI_APP_BINS := $(if $(GUI_CC),$(patsubst %,$(BUILD_DIR)/demos/%,$(GUI_APPS)))

# /bin/peload runs Windows .exe files (the kernel hands them to it); it
# comes with the demos. usr/win/*.c are Windows test programs for it,
# built with mingw-w64 without a C runtime.
PELOAD := $(if $(DEMO_CC),$(BUILD_DIR)/demos/peload)
WIN_SRCS := $(wildcard win/*.c)
WIN_EXES := $(if $(WIN_CC),$(patsubst win/%.c,$(BUILD_DIR)/win/%.exe,$(WIN_SRCS)))

# The test disk (scripts/mkdisk.py), attached by `make run` as the IDE
# primary master: GPT, partition 1 FAT32 "BOOT", partition 2 ext2 "ROOT"
# (the kernel mounts it at /ext2, see kernel/init/fs.c). usr/disk/ goes on
# both; FAT32 also gets the modules, ext2 the built programs in /bin
# (demos, desktop, term..., not the busybox links, which point at /busybox).
DISK_IMG     ?= $(OUT_DIR)/disk.img
DISK_FAT_MB  ?= 64
DISK_EXT2_MB ?= 64
DISK_FILES   := $(shell find disk -type f 2>/dev/null)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

.PHONY: all clean config disk

all: $(INITRAMFS_IMG) $(if $(MAKE_DISK),$(DISK_IMG))

disk: $(if $(MAKE_DISK),$(DISK_IMG))
	@$(if $(MAKE_DISK),,echo "initramfs: note: no test disk ($(if $(filter n,$(CONFIG_DISK)),CONFIG_DISK=n,needs $(need_CONFIG_DISK)))")

# Rebuilt only when what goes on it changes, so what the kernel writes to
# it stays between runs until then. Delete it to start over.
$(DISK_IMG): $(INITRAMFS_IMG) $(DISK_FILES) $(KERNEL_PATH)/scripts/mkdisk.py
	$(Q)python3 $(KERNEL_PATH)/scripts/mkdisk.py --output $@ \
		--part fat32:BOOT:$(DISK_FAT_MB):disk:$(KERNEL_PATH)/out/modules=/modules \
		--part ext2:ROOT:$(DISK_EXT2_MB):disk:$(BUILD_DIR)/demos=/bin

# Compile /init ELF
$(BUILD_DIR)/init.elf: $(SRC) $(LDSCRIPT)
	@mkdir -p $(BUILD_DIR)
	$(Q)$(CC) -ffreestanding -O2 -nostdlib -static -nostartfiles \
		-T $(LDSCRIPT) $(CFLAGS) \
		-o $@ $(SRC)
	@echo "initramfs: built /init -> $@"

$(BUILD_DIR)/demos/%: demos/%.c
	@mkdir -p $(dir $@)
	$(Q)$(MUSL_CC) -static -O2 -Wall -Wextra -o $@ $(filter %.c,$^)
	@echo "initramfs: built demo $* -> $@"

$(BUILD_DIR)/demos/desktop: $(wildcard apps/desktop/*.c) $(LIBGUI_COMMON) $(LIBGUI_SERVER) $(LIBGUI_HDRS)
	@mkdir -p $(dir $@)
	$(Q)$(MUSL_CC) $(LIBGUI_CFLAGS) -o $@ $(filter %.c,$^)
	@echo "initramfs: built desktop -> $@"

# GUI applications: one per apps/<name>, with libgui's client half.
.SECONDEXPANSION:
$(GUI_APP_BINS): $(BUILD_DIR)/demos/%: $$(wildcard apps/%/*.c) $(LIBGUI_COMMON) $(LIBGUI_CLIENT) $(LIBGUI_HDRS)
	@mkdir -p $(dir $@)
	$(Q)$(MUSL_CC) $(LIBGUI_CFLAGS) -o $@ $(filter %.c,$^)
	@echo "initramfs: built GUI app $* -> $@"

$(BUILD_DIR)/demos/peload: $(wildcard apps/peload/*.c apps/peload/*.h)
	@mkdir -p $(dir $@)
	$(Q)$(MUSL_CC) -static -O2 -Wall -Wextra -o $@ $(filter %.c,$^)
	@echo "initramfs: built peload -> $@"

$(BUILD_DIR)/win/%.exe: win/%.c
	@mkdir -p $(dir $@)
	$(Q)$(MINGW_CC) -O2 -Wall -Wextra -nostdlib -e start -o $@ $< -lkernel32
	@echo "initramfs: built Windows program $*.exe -> $@"

# The settings that decide the image's contents, rewritten only when they
# change, so switching an option off rebuilds the image without it.
CONFIG_STAMP := $(BUILD_DIR)/config.stamp
CONFIG_STATE := $(HAVE_BUSYBOX)|$(BUSYBOX_DIR)|$(DEMO_CC)|$(GUI_CC)|$(GUI_APPS)|$(WIN_CC)|$(DISK_FAT_MB)|$(DISK_EXT2_MB)
$(shell mkdir -p $(BUILD_DIR); [ "$$(cat $(CONFIG_STAMP) 2>/dev/null)" = '$(CONFIG_STATE)' ] || \
	echo '$(CONFIG_STATE)' > $(CONFIG_STAMP))

# Everything that ends up in the image. Depending on init.elf alone meant
# rebuilt modules (or a new busybox) were never copied in once the image
# existed - the kernel then booted with stale .ko files.
INITRAMFS_INPUTS := $(BUILD_DIR)/init.elf $(DEMOS) $(DESKTOP) $(GUI_APP_BINS) $(PELOAD) $(WIN_EXES) \
	$(wildcard $(KERNEL_PATH)/out/modules/*.ko) \
	$(BUSY_FILES) $(FONTS) \
	$(KERNEL_PATH)/scripts/initramfs.py inittab $(shell find rootfs -type f) $(CONFIG_STAMP) \
	$(lastword $(MAKEFILE_LIST))

# Create rootfs layout and generate initramfs image. The rootfs is rebuilt
# from scratch so files removed from the inputs do not linger.
#
# The kernel runs init= from its command line, or else the first of /init,
# /sbin/init, /etc/init, /bin/init, /bin/sh. With busybox that is
# /sbin/init (busybox init) reading /etc/inittab; without it, the minimal
# /init built from init.c.
$(INITRAMFS_IMG): $(INITRAMFS_INPUTS)
	@rm -rf $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/modules
	@cp -r $(KERNEL_PATH)/out/modules/* $(ROOTFS_DIR)/modules/
	@$(if $(BUSY_FILES),cp -r $(BUSY_FILES) $(ROOTFS_DIR)/)
	@[ -e $(ROOTFS_DIR)/busybox ] || cp $(BUILD_DIR)/init.elf $(ROOTFS_DIR)/init
	@# Mountpoints: kernel/init/fs.c and modules mount over these.
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/proc $(ROOTFS_DIR)/dev \
		$(ROOTFS_DIR)/ext2 $(ROOTFS_DIR)/fat $(ROOTFS_DIR)/pipe
	@# /tmp: programs expect it (tmux puts its server socket there).
	@mkdir -p -m 1777 $(ROOTFS_DIR)/tmp
	@mkdir -p $(ROOTFS_DIR)/etc
	@cp inittab $(ROOTFS_DIR)/etc/inittab
	@# Files that go into the image as they are (/etc/init.d/rcS, ...).
	@cp -r rootfs/. $(ROOTFS_DIR)/
	@mkdir -p $(ROOTFS_DIR)/usr/share/fonts
	@cp $(FONTS) $(ROOTFS_DIR)/usr/share/fonts/
	@$(if $(HAVE_BUSYBOX),,echo "initramfs: note: no busybox ($(if $(filter n,$(CONFIG_BUSYBOX)),CONFIG_BUSYBOX=n,no $(BUSYBOX))); the image boots the minimal /init")
	@[ -n "$(BUSYBOX_APPLETS)" ] || [ -z "$(HAVE_BUSYBOX)" ] || echo "initramfs: warning: could not list busybox applets (no --list-full/--list, or not runnable on this host); no applet links created"
	@for a in $(BUSYBOX_APPLETS); do \
		mkdir -p "$(ROOTFS_DIR)/$$(dirname "$$a")"; \
		ln -sfn /busybox "$(ROOTFS_DIR)/$$a"; \
	done
	@$(if $(MUSL_CC),,echo "initramfs: note: no x86_64-linux-musl-gcc (MUSL_CC); demos and the desktop not built")
	@for d in $(DEMOS) $(DESKTOP) $(GUI_APP_BINS) $(PELOAD) $(WIN_EXES); do cp "$$d" $(ROOTFS_DIR)/bin/; done
	@[ ! -e $(ROOTFS_DIR)/init ] || chmod +x $(ROOTFS_DIR)/init
	@python3 $(KERNEL_PATH)/scripts/initramfs.py \
		--root $(ROOTFS_DIR) \
		--output $@
	@echo "initramfs: generated $@"

# What this configuration builds, and with what.
show = printf '  %-15s %-5s %s\n'
config:
	@echo "Userland (usr/Makefile):"
	@$(show) CONFIG_BUSYBOX "$(CONFIG_BUSYBOX)" "$(if $(HAVE_BUSYBOX),$(BUSYBOX) ($(words $(BUSYBOX_APPLETS)) applets),$(if $(filter n,$(CONFIG_BUSYBOX)),not built,not built (no $(BUSYBOX))))"
	@$(show) CONFIG_DEMOS "$(CONFIG_DEMOS)" "$(if $(DEMO_CC),$(words $(DEMO_SRCS)) demos + peload,not built)"
	@$(show) CONFIG_GUI "$(CONFIG_GUI)" "$(if $(GUI_CC),desktop + $(GUI_APPS),not built)"
	@$(show) CONFIG_WIN "$(CONFIG_WIN)" "$(if $(WIN_CC),$(words $(WIN_SRCS)) Windows programs,not built)"
	@$(show) CONFIG_DISK "$(CONFIG_DISK)" "$(if $(MAKE_DISK),$(DISK_IMG) (FAT32 $(DISK_FAT_MB) MB + ext2 $(DISK_EXT2_MB) MB),not built$(if $(filter n,$(CONFIG_DISK)),, (needs $(need_CONFIG_DISK))))"
	@$(show) MUSL_CC "" "$(or $(MUSL_CC),not found)"
	@$(show) MINGW_CC "" "$(or $(MINGW_CC),not found)"
	@$(if $(MISSING),$(foreach o,$(MISSING),echo "  error: $(o)=y needs $(need_$(o)), which was not found";))

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(INITRAMFS_IMG)
	@echo "initramfs: clean complete"
