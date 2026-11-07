# Main Makefile

Q = @
export Q

# ---------------------------------------------------------------------------
# Paths and configuration
# ---------------------------------------------------------------------------

ROOT_DIR := $(abspath .)
export ROOT_DIR

# Local settings (toolchain paths, optional userland parts, QEMU): copy
# config.mk.example to config.mk, or run `make config`. Command-line
# variables still win over it.
-include $(ROOT_DIR)/config.mk

ARCH ?= x86
OUT_DIR ?= $(abspath out)
ARCH_DIR := $(abspath arch/$(ARCH))

TOOLS_DIR := tools
DEV_TOOLS_DIR := tools/dev
BUILD_TOOL := $(OUT_DIR)/tools/dev/build/build_main

SUPPORTED_ARCHES := x86 x86_64 arm64

ifneq ($(ARCH),$(filter $(ARCH),$(SUPPORTED_ARCHES)))
  $(error Unsupported architecture: $(ARCH). Supported: $(SUPPORTED_ARCHES))
endif

# ---------------------------------------------------------------------------
# Build mode
#   RELEASE=1  release build in out/release: -O3, no debug info by default
#   OPT_LEVEL  -O level (g, 0, 1, 2, 3, s); DEBUG_INFO y/n for -g
# ---------------------------------------------------------------------------

RELEASE ?= 0

ifeq ($(RELEASE),1)
  BUILD_TYPE := release
  OPT_LEVEL ?= 3
  DEBUG_INFO ?= n
else
  BUILD_TYPE := build
  OPT_LEVEL ?= g
  DEBUG_INFO ?= y
endif

# Kernel options, compiled in through the generated kconfig.h below.
CONFIG_LOGLEVEL        ?= 8
CONFIG_LOGLEVEL_SERIAL ?= 8
CONFIG_TIMER_HZ        ?= 100

# Loadable modules (.ko in out/modules, put in the initramfs and on the
# test disk), as in Linux: CONFIG_MOD_<NAME> is y (built and loaded at
# boot), m (built, load it yourself) or n (not built). Boot loads them in
# MODULE_ORDER; "modules=a,b" on the kernel command line replaces the
# list for one boot. MOD_<NAME>_CFLAGS adds compiler flags for one
# module (C modules), e.g. -DDEBUG.
uc = $(subst z,Z,$(subst y,Y,$(subst x,X,$(subst w,W,$(subst v,V,$(subst u,U,$(subst t,T,$(subst s,S,$(subst r,R,$(subst q,Q,$(subst p,P,$(subst o,O,$(subst n,N,$(subst m,M,$(subst l,L,$(subst k,K,$(subst j,J,$(subst i,I,$(subst h,H,$(subst g,G,$(subst f,F,$(subst e,E,$(subst d,D,$(subst c,C,$(subst b,B,$(subst a,A,$(1)))))))))))))))))))))))))))
MODULE_ORDER := input tty usb procfs video net
CONFIG_MOD_INPUT  ?= y
CONFIG_MOD_TTY    ?= y
CONFIG_MOD_USB    ?= y
CONFIG_MOD_PROCFS ?= y
CONFIG_MOD_VIDEO  ?= y
CONFIG_MOD_NET    ?= y
mod_state = $(CONFIG_MOD_$(call uc,$(1)))
KERNEL_MODULES := $(strip $(foreach m,$(MODULE_ORDER),$(if $(filter y m,$(call mod_state,$(m))),$(m))))
BOOT_MODULES   := $(strip $(foreach m,$(MODULE_ORDER),$(if $(filter y,$(call mod_state,$(m))),$(m))))
MOD_CFLAGS_ALL := $(strip $(foreach m,$(MODULE_ORDER),$(MOD_$(call uc,$(m))_CFLAGS)))

# Module options (y/n), compiled in like the rest of kconfig.h.
CONFIG_INPUT_KEYBOARD ?= y
CONFIG_INPUT_MOUSE    ?= y
CONFIG_USB_XHCI       ?= y
yn01 = $(if $(filter y,$(1)),1,0)

# ---------------------------------------------------------------------------
# Build directory
# ---------------------------------------------------------------------------

BUILD_DIR := $(OUT_DIR)/$(BUILD_TYPE)/$(ARCH)
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
export KERNEL_ELF

# ---------------------------------------------------------------------------
# Parallel jobs: defaults to number of CPUs
# ---------------------------------------------------------------------------

JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------

ifeq ($(ARCH),x86)
	CROSS   ?= x86_64-elf-
	CC       = $(CROSS)gcc
	AS       = $(CROSS)as
	LD       = $(CROSS)ld
	AR       = $(CROSS)ar
	OBJCOPY  = $(CROSS)objcopy
	ASM      = nasm
	ASMFLAGS := -g -f elf64
endif

ifeq ($(ARCH),arm64)
	CROSS   ?= aarch64-elf-
	CC       = $(CROSS)gcc
	AS       = $(CROSS)as
	LD       = $(CROSS)ld
	AR       = $(CROSS)ar
	OBJCOPY  = $(CROSS)objcopy
	ASM      := $(AS)
	ASMFLAGS := -g -march=armv8-a
endif

export LD CC AS AR OBJCOPY ASM CFLAGS ASMFLAGS CROSS
export ARCH OUT_DIR BUILD_DIR ARCH_DIR TOOLS_DIR BUILD_TOOL

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------

CFLAGS = -ffreestanding -O$(OPT_LEVEL) -Wall -Wextra
ifeq ($(DEBUG_INFO),y)
	CFLAGS += -g -gdwarf-4
endif

ifeq ($(ARCH),x86)
	CFLAGS += -mcmodel=kernel -m64 \
		-mno-mmx -mno-sse -mno-sse2 -mno-sse3 \
		-mno-avx -mno-avx2 -mno-red-zone
endif

ifeq ($(ARCH),arm64)
	CFLAGS += -march=armv8-a
endif

# kconfig.h: the kernel options above as #defines, force-included into
# every C file. It also records CFLAGS, so changing an option or a flag
# rebuilds what depends on it (the build tool tracks headers). The file
# is rewritten only when its text changes.
KCONFIG_H := $(BUILD_DIR)/include/generated/kconfig.h
KCONFIG_CFLAGS := $(CFLAGS)
define KCONFIG_TEXT
/* Generated by the Makefile from config.mk; do not edit. */
/* CFLAGS: $(KCONFIG_CFLAGS) */
#define CONFIG_LOGLEVEL $(CONFIG_LOGLEVEL)
#define CONFIG_LOGLEVEL_SERIAL $(CONFIG_LOGLEVEL_SERIAL)
#define CONFIG_TIMER_HZ $(CONFIG_TIMER_HZ)
#define CONFIG_BOOT_MODULES "$(BOOT_MODULES)"
#define CONFIG_INPUT_KEYBOARD $(call yn01,$(CONFIG_INPUT_KEYBOARD))
#define CONFIG_INPUT_MOUSE $(call yn01,$(CONFIG_INPUT_MOUSE))
#define CONFIG_USB_XHCI $(call yn01,$(CONFIG_USB_XHCI))
/* Module CFLAGS: $(MOD_CFLAGS_ALL) */
endef
export KCONFIG_TEXT
CFLAGS += -include $(KCONFIG_H)

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------

INCLUDES += -I$(abspath .)
INCLUDES += -I$(abspath include)
INCLUDES += -I$(ARCH_DIR)/include
INCLUDES += -I$(ARCH_DIR)/kernel
INCLUDES += -I$(ARCH_DIR)/boot/limine
export INCLUDES

# ---------------------------------------------------------------------------
# Derived paths
# ---------------------------------------------------------------------------

LIB_DIR := $(abspath lib)
export LIB_DIR

LOG_DIR  := $(OUT_DIR)/logs/$(shell date +%Y-%m-%d)
LOG_FILE := $(LOG_DIR)/$(shell date +%H-%M-%S).log
export LOG_DIR LOG_FILE

ifeq ($(ARCH),arm64)
	EFI_NAME := BOOTAA64.EFI
	EFI_TARGET := efi-app-aarch64
endif

# ---------------------------------------------------------------------------
# Build graph: auto-collected outputs from declarative module declarations
# ---------------------------------------------------------------------------

# obj-y directories: compiled .o files are auto-discovered for kernel link
__obj_y_dirs :=

# lib-y directories: archive paths are auto-resolved at link time
define lib-path
$(BUILD_DIR)/$(1)/lib$(notdir $(1)).a
endef

# ---------------------------------------------------------------------------
# Build tool flags
# ---------------------------------------------------------------------------

BUILD_TOOL_FLAGS = --log-file $(LOG_FILE) -v --jobs $(JOBS)

# ---------------------------------------------------------------------------
# Build macros
# ---------------------------------------------------------------------------

# $(d) = relpath, expanded when called via foreach
define build-lib-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--cflags    "$(CFLAGS) $(ccflags-y)" \
		--includes  "$(INCLUDES)" \
		--ar $(AR) \
		--output    $(BUILD_DIR)/$(d)/lib$(notdir $(d)).a \
		--type archive
endef

define build-lib-asm-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(CFLAGS) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--ar $(AR) \
		--output    $(BUILD_DIR)/$(d)/lib$(notdir $(d)).a \
		--type archive
endef

define build-obj-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(CFLAGS) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--type objects
	$(eval __obj_y_dirs += $(d))
endef

define build-exe-module
	$(Q)__obj_files=""; \
	for __dir in $(__obj_y_dirs); do \
		for __f in $$(find $(BUILD_DIR)/$$__dir -name '*.o' 2>/dev/null); do \
			__obj_files="$$__obj_files $$__f"; \
		done; \
	done; \
	__whole_archive=""; \
	for __mod in $(exe-whole-archive); do \
		__base=$$(basename $$__mod); \
		__whole_archive="$$__whole_archive $(BUILD_DIR)/$$__mod/lib$$__base.a"; \
	done; \
	__libs=""; \
	for __mod in $(exe-libs); do \
		__base=$$(basename $$__mod); \
		__libs="$$__libs $(BUILD_DIR)/$$__mod/lib$$__base.a"; \
	done; \
	__libs_arg=""; \
	if [ -n "$$__whole_archive" ]; then \
		__libs_arg="--whole-archive $$__whole_archive"; \
	fi; \
	if [ -n "$$__libs" ]; then \
		__libs_arg="$$__libs_arg --no-whole-archive $$__libs"; \
	fi; \
	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(if $(cflags-y),$(cflags-y),$(CFLAGS)) $(ccflags-y)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--output    $(exe-output-y) \
		--type exe \
		--ld $(LD) \
		$(if $(exe-ldflags-y), --ldflags "$(exe-ldflags-y)") \
		--obj-files "$$__obj_files" \
		--libs "$$__libs_arg"
endef

define build-mod-module
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir   $(ROOT_DIR)/$(d) \
		--build-dir $(BUILD_DIR)/$(d) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags    "$(if $(cflags-y),$(cflags-y),$(CFLAGS)) $(ccflags-y) $(MOD_$(call uc,$(notdir $(mod-output-y:.ko=)))_CFLAGS)" \
		--asmflags  "$(ASMFLAGS) $(asflags-y)" \
		--includes  "$(INCLUDES)" \
		--output    $(mod-output-y) \
		--type module \
		--ld $(LD) \
		$(if $(mod-ldflags-y), --ldflags  "$(mod-ldflags-y)") \
		$(if $(mod-objs-y),    --obj-files "$(mod-objs-y)") \
		$(if $(mod-libs-y),    --libs      "$(mod-libs-y)")
endef

define reset-module-vars
	$(eval lib-y            :=)
	$(eval lib-asm-y        :=)
	$(eval obj-y            :=)
	$(eval exe-y            :=)
	$(eval exe-output-y     :=)
	$(eval exe-ldflags-y    :=)
	$(eval exe-whole-archive :=)
	$(eval exe-libs         :=)
	$(eval mod-y            :=)
	$(eval mod-output-y     :=)
	$(eval mod-ldflags-y    :=)
	$(eval mod-objs-y       :=)
	$(eval mod-libs-y       :=)
	$(eval subdir-y         :=)
	$(eval always-y         :=)
	$(eval ccflags-y        :=)
	$(eval asflags-y        :=)
	$(eval cppflags-y       :=)
	$(eval ldflags-y        :=)
endef

define load-module
	$(if $(wildcard $(ROOT_DIR)/$(1)/Makefile),  $(eval include $(ROOT_DIR)/$(1)/Makefile)) \
	$(if $(wildcard $(ROOT_DIR)/$(1)/module.mk), $(eval include $(ROOT_DIR)/$(1)/module.mk)) \
	$(if $(wildcard $(ROOT_DIR)/$(1)/Makefile)$(wildcard $(ROOT_DIR)/$(1)/module.mk), \
		$(foreach d,$(lib-y),     $(call build-lib-module)) \
		$(foreach d,$(lib-asm-y), $(call build-lib-asm-module)) \
		$(foreach d,$(obj-y),     $(call build-obj-module)) \
		$(foreach d,$(exe-y),     $(call build-exe-module)) \
		$(foreach d,$(if $(filter $(notdir $(mod-output-y:.ko=)),$(KERNEL_MODULES)),$(mod-y)), \
			$(call build-mod-module)) \
		$(eval _subdirs_$(subst /,_,$(1)) := $(subdir-y)) \
		$(call reset-module-vars) \
		$(foreach s,$(_subdirs_$(subst /,_,$(1))),$(call load-module,$(s))) \
	, \
		$(error no module.mk or Makefile found in: $(ROOT_DIR)/$(1)) \
	)
endef

# ---------------------------------------------------------------------------
# Module list
# ---------------------------------------------------------------------------

MODULES :=

ifeq ($(ARCH),arm64)
	MODULES += arch/arm64
else
	MODULES += init
	MODULES += net
	MODULES += fs
	MODULES += drivers
	MODULES += lib
	MODULES += sound
	MODULES += kernel
	MODULES += arch/$(ARCH)
endif

# ---------------------------------------------------------------------------
# Subdirectory discovery (boot, tools, etc.)
# ---------------------------------------------------------------------------

# Note: use spaces, not tabs, inside this define because it's used
# with $(eval ...) where leading tabs are treated as recipe lines.
define kbuild-subdir
  subdir-y :=
  include $(1)/Makefile
  subdirs += $(addprefix $(1)/, $(subdir-y))
  subdir-y :=
endef

subdirs :=
$(eval $(call kbuild-subdir,arch/$(ARCH)))

# ---------------------------------------------------------------------------
# Default target: build the kernel
# ---------------------------------------------------------------------------

PHONY += all
all: build

# ---------------------------------------------------------------------------
# Userland: minimal test payload for standalone kernel development
# ---------------------------------------------------------------------------

PHONY += usr
usr:
	@$(MAKE) -C usr KERNEL_PATH=$(ROOT_DIR) OUT_DIR=$(OUT_DIR)

PHONY += disk
disk:
	@$(MAKE) -C usr KERNEL_PATH=$(ROOT_DIR) OUT_DIR=$(OUT_DIR) disk

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

PHONY += build-tool
build-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/build build

# ---------------------------------------------------------------------------
# Rust crate
# ---------------------------------------------------------------------------

RUST_TARGET := x86_64-unknown-none
RUST_DIR    := $(ROOT_DIR)/rust
RUST_LIB    := $(RUST_DIR)/target/$(RUST_TARGET)/release/libhubble_rust.a

PHONY += rust
rust:
	@echo "Building Rust crate..."
	RUSTFLAGS="-C code-model=kernel" cargo build \
		--manifest-path $(RUST_DIR)/Cargo.toml \
		--target $(RUST_TARGET) \
		--release

# ---------------------------------------------------------------------------
# Rust loadable modules
# ---------------------------------------------------------------------------
# Each directory is a no_std staticlib crate with a module.ld. The archive
# is linked into one relocatable .ko (ld -r) the way C modules are, then:
# --gc-sections drops unused core/alloc code (rooted at <name>_init), debug
# info is stripped, and every symbol is made local so the module exports
# nothing. Same code model as C modules (kernel, static relocations).

RUST_MODULES := $(foreach m,fs/procfs,$(if $(filter $(notdir $(m)),$(KERNEL_MODULES)),$(m)))

PHONY += rust-modules
rust-modules:
	@mkdir -p $(OUT_DIR)/modules
	$(Q)set -e; for dir in $(RUST_MODULES); do \
		name=$$(basename $$dir); \
		echo "Building Rust module $$name..."; \
		RUSTFLAGS="-C code-model=kernel -C relocation-model=static" \
			cargo build --manifest-path $(ROOT_DIR)/$$dir/Cargo.toml \
			--target $(RUST_TARGET) --release; \
		$(LD) -r --gc-sections -u $${name}_init -T $(ROOT_DIR)/$$dir/module.ld \
			--whole-archive $(ROOT_DIR)/$$dir/target/$(RUST_TARGET)/release/lib$$name.a \
			-o $(OUT_DIR)/modules/$$name.ko; \
		$(OBJCOPY) --strip-debug --wildcard --localize-symbol='*' \
			$(OUT_DIR)/modules/$$name.ko; \
	done

SRC_DIRS := .

PHONY: format
format:
	@echo "Formatting all files..."
	@find $(SRC_DIRS) -type f \( -name "*.c" -o -name "*.h" \) \
		-not -path "*/.*" \
		-not -path "*/build/*" \
		| xargs -r clang-format -style=file -i
	@echo "Formatting complete!"

PHONY: format-check
format-check:
	@echo "Checking formatting..."
	@find $(SRC_DIRS) -type f \( -name "*.c" -o -name "*.h" \) \
		-not -path "*/.*" \
		-not -path "*/build/*" \
		| xargs -r clang-format -style=file --dry-run --Werror

# ---------------------------------------------------------------------------
# Build kernel + userland (no ISO yet)
# ---------------------------------------------------------------------------

PHONY += build
build: build-tool rust rust-modules kconfig
	@mkdir -p $(LOG_DIR)
	$(foreach mod,$(MODULES),$(call load-module,$(mod)))
	$(Q)set -e; for dir in $(filter-out arch/$(ARCH)/kernel,$(subdirs)); do \
		$(MAKE) -C $$dir; \
	done
	@echo "Build complete"

PHONY += kconfig
kconfig:
	@mkdir -p $(dir $(KCONFIG_H))
	@printf '%s\n' "$$KCONFIG_TEXT" | cmp -s - $(KCONFIG_H) || \
		printf '%s\n' "$$KCONFIG_TEXT" > $(KCONFIG_H)

PHONY += release
release-build:
	@$(MAKE) RELEASE=1 build

# ---------------------------------------------------------------------------
# Run kernel in QEMU (requires Limine)
# ---------------------------------------------------------------------------

# Read by scripts/qemu/run-qemu.sh and build-minimal-iso.sh.
QEMU_ACCEL ?= auto
QEMU_MEM   ?= 800M
QEMU_SMP   ?= 2
QEMU_ARGS  ?=
CMDLINE    ?=
INIT       ?=
QEMU_DISPLAY   ?=
QEMU_SERIAL    ?= stdio
QEMU_LOG       ?= int
QEMU_LOG_FILE  ?= /tmp/qemu-kernel.log
QEMU_NO_REBOOT ?= y
QEMU_NO_SHUTDOWN ?= y
QEMU_NIC       ?= e1000
QEMU_HOSTFWD   ?=
# The test disk from `make usr` / `make disk`, attached when it exists.
QEMU_DISK  ?= $(if $(filter n,$(CONFIG_DISK)),,$(OUT_DIR)/disk.img)
export QEMU_ACCEL QEMU_MEM QEMU_SMP QEMU_ARGS QEMU_DISK CMDLINE INIT \
	QEMU_DISPLAY QEMU_SERIAL QEMU_LOG QEMU_LOG_FILE QEMU_NO_REBOOT QEMU_NO_SHUTDOWN \
	QEMU_NIC QEMU_HOSTFWD

PHONY += run
run: build usr
	@bash $(ROOT_DIR)/scripts/qemu/build-minimal-iso.sh $(KERNEL_ELF) $(OUT_DIR)/kernel-dev.iso
	@bash $(ROOT_DIR)/scripts/qemu/run-qemu.sh --iso $(OUT_DIR)/kernel-dev.iso

# ---------------------------------------------------------------------------
# Debug: build debug kernel, create minimal ISO, launch QEMU paused
# ---------------------------------------------------------------------------

PHONY += debug
debug: build usr
	@bash $(ROOT_DIR)/scripts/qemu/build-minimal-iso.sh $(KERNEL_ELF) $(OUT_DIR)/kernel-debug.iso
	@bash $(ROOT_DIR)/scripts/qemu/run-qemu.sh --debug --iso $(OUT_DIR)/kernel-debug.iso

# ---------------------------------------------------------------------------
# GDB: connect to running QEMU debug session
# ---------------------------------------------------------------------------

PHONY += gdb
gdb:
	@gdb -q -ex "file $(KERNEL_ELF)" -ex "target remote :1234"

# ---------------------------------------------------------------------------
# Release: build release kernel, create minimal ISO, launch QEMU
# ---------------------------------------------------------------------------

PHONY += release
release: release-build usr
	@bash $(ROOT_DIR)/scripts/qemu/build-minimal-iso.sh $(KERNEL_ELF) $(OUT_DIR)/kernel.iso
	@bash $(ROOT_DIR)/scripts/qemu/run-qemu.sh --iso $(OUT_DIR)/kernel.iso

PHONY += clean
clean:
	@rm -rf $(OUT_DIR)
	@echo "Clean complete"

PHONY += rebuild
rebuild: clean build

PHONY += mkvars
mkvars:
	@echo "ARCH         = $(ARCH)"
	@echo "BUILD_DIR    = $(BUILD_DIR)"
	@echo "MODULES      = $(MODULES)"
	@echo "__obj_y_dirs = $(__obj_y_dirs)"
	@echo "subdirs      = $(subdirs)"

# ---------------------------------------------------------------------------
# Configuration: create config.mk from the example, show what is in effect
# ---------------------------------------------------------------------------

tool = $(if $(shell command -v $(1) 2>/dev/null),$(shell command -v $(1)),NOT FOUND ($(1)))

PHONY += config
config:
	@[ -e $(ROOT_DIR)/config.mk ] || { cp $(ROOT_DIR)/config.mk.example $(ROOT_DIR)/config.mk; \
		echo "Created config.mk from config.mk.example; edit it to change the settings below."; echo; }
	@echo "Kernel:"
	@printf '  %-23s %s\n' ARCH "$(ARCH)" BUILD "$(BUILD_TYPE) (RELEASE=$(RELEASE))" JOBS "$(JOBS)" \
		OPT_LEVEL "-O$(OPT_LEVEL)" DEBUG_INFO "$(DEBUG_INFO)" CONFIG_LOGLEVEL "$(CONFIG_LOGLEVEL)" \
		CONFIG_LOGLEVEL_SERIAL "$(CONFIG_LOGLEVEL_SERIAL)" CONFIG_TIMER_HZ "$(CONFIG_TIMER_HZ)" \
		KERNEL_MODULES "$(KERNEL_MODULES)" BOOT_MODULES "$(BOOT_MODULES)" \
		CONFIG_INPUT_KEYBOARD "$(CONFIG_INPUT_KEYBOARD)" CONFIG_INPUT_MOUSE "$(CONFIG_INPUT_MOUSE)" \
		CONFIG_USB_XHCI "$(CONFIG_USB_XHCI)" $(foreach m,$(MODULE_ORDER),$(if $(MOD_$(call uc,$(m))_CFLAGS),MOD_$(call uc,$(m))_CFLAGS "$(MOD_$(call uc,$(m))_CFLAGS)"))
	@printf '  %-23s %s\n' CC "$(call tool,$(CC))" LD "$(call tool,$(LD))" ASM "$(call tool,$(ASM))"
	@printf '  %-23s %s\n' cargo "$(call tool,cargo)" limine "$(call tool,limine)"
	@echo
	@$(MAKE) --no-print-directory -C usr KERNEL_PATH=$(ROOT_DIR) OUT_DIR=$(OUT_DIR) config
	@echo
	@echo "QEMU (make run):"
	@printf '  %-23s %s\n' qemu "$(call tool,qemu-system-x86_64)" QEMU_ACCEL "$(QEMU_ACCEL)" \
		QEMU_MEM "$(QEMU_MEM)" QEMU_SMP "$(QEMU_SMP)" QEMU_DISK "$(QEMU_DISK)" QEMU_ARGS "$(QEMU_ARGS)" \
		QEMU_DISPLAY "$(QEMU_DISPLAY)" QEMU_SERIAL "$(QEMU_SERIAL)" QEMU_LOG "$(QEMU_LOG)" \
		QEMU_LOG_FILE "$(QEMU_LOG_FILE)" QEMU_NO_REBOOT "$(QEMU_NO_REBOOT)" \
		QEMU_NO_SHUTDOWN "$(QEMU_NO_SHUTDOWN)" QEMU_NIC "$(QEMU_NIC)" QEMU_HOSTFWD "$(QEMU_HOSTFWD)" CMDLINE "$(CMDLINE)" INIT "$(INIT)"

PHONY += menuconfig
menuconfig:
	@bash $(ROOT_DIR)/scripts/menuconfig.sh
	@echo
	@$(MAKE) --no-print-directory config

PHONY += help
help:
	@echo "Usage: make [TARGET] [ARCH=<arch>]"
	@echo ""
	@echo "Targets:"
	@echo "  build        Build kernel ELF"
	@echo "  usr          Build minimal test userland (and the test disk)"
	@echo "  disk         Build the test disk out/disk.img (FAT32 + ext2)"
	@echo "  run          Build kernel + userland and run in QEMU"
	@echo "  debug        Build debug kernel + userland and run in QEMU"
	@echo "  release      Build release kernel + userland and run in QEMU"
	@echo "  gdb          Connect GDB to running QEMU debug session"
	@echo "  clean        Remove build artifacts"
	@echo "  rebuild      Clean and rebuild"
	@echo "  config       Create config.mk and show the settings and tools in use"
	@echo "  menuconfig   Edit config.mk in menus (needs gum)"
	@echo ""
	@echo "Architectures: ${SUPPORTED_ARCHES}"

.PHONY: $(PHONY)
