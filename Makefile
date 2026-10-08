# Main Makefile

Q = @
export Q

# ---------------------------------------------------------------------------
# Paths and configuration
# ---------------------------------------------------------------------------

ROOT_DIR := $(abspath .)
export ROOT_DIR

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
#   RELEASE=1  removes debug symbols, uses -O3
#   DEBUG=1    builds with debug symbols and -Og optimization
# ---------------------------------------------------------------------------

DEBUG ?= 1
RELEASE ?= 0

ifeq ($(RELEASE),1)
  DEBUG := 0
  BUILD_TYPE := release
  CFLAGS += -ffreestanding -O3 -Wall -Wextra
else
  DEBUG := 1
  BUILD_TYPE := build
  CFLAGS += -ffreestanding -Og -Wall -Wextra -g -gdwarf-4
endif

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
	CROSS    = x86_64-elf-
	CC       = $(CROSS)gcc
	AS       = $(CROSS)as
	LD       = $(CROSS)ld
	AR       = $(CROSS)ar
	OBJCOPY  = $(CROSS)objcopy
	ASM      = nasm
	ASMFLAGS := -g -f elf64
endif

ifeq ($(ARCH),arm64)
	CROSS    = aarch64-elf-
	CC       = $(CROSS)gcc
	AS       = $(CROSS)as
	LD       = $(CROSS)ld
	AR       = $(CROSS)ar
	OBJCOPY  = $(CROSS)objcopy
	ASM      := $(AS)
	ASMFLAGS := -g -march=armv8-a
endif

export LD CC AS AR OBJCOPY ASM CFLAGS ASMFLAGS
export ARCH OUT_DIR BUILD_DIR ARCH_DIR TOOLS_DIR BUILD_TOOL

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------

ifeq ($(RELEASE),1)
	CFLAGS = -ffreestanding -O3 -Wall -Wextra
else ifeq ($(DEBUG),1)
	CFLAGS = -ffreestanding -Og -Wall -Wextra -g -gdwarf-4
else
	CFLAGS = -ffreestanding -O2 -Wall -Wextra -g
endif

ifeq ($(ARCH),x86)
	CFLAGS += -mcmodel=kernel -m64 \
		-mno-mmx -mno-sse -mno-sse2 -mno-sse3 \
		-mno-avx -mno-avx2 -mno-red-zone
endif

ifeq ($(ARCH),arm64)
	CFLAGS += -march=armv8-a
endif

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
		--cflags    "$(if $(cflags-y),$(cflags-y),$(CFLAGS)) $(ccflags-y)" \
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
		$(foreach d,$(mod-y),     $(call build-mod-module)) \
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

RUST_MODULES := fs/procfs

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
build: build-tool rust rust-modules
	@mkdir -p $(LOG_DIR)
	$(foreach mod,$(MODULES),$(call load-module,$(mod)))
	$(Q)set -e; for dir in $(filter-out arch/$(ARCH)/kernel,$(subdirs)); do \
		$(MAKE) -C $$dir; \
	done
	@echo "Build complete"

PHONY += release
release-build:
	@$(MAKE) RELEASE=1 build

# ---------------------------------------------------------------------------
# Run kernel in QEMU (requires Limine)
# ---------------------------------------------------------------------------

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

PHONY += help
help:
	@echo "Usage: make [TARGET] [ARCH=<arch>]"
	@echo ""
	@echo "Targets:"
	@echo "  build        Build kernel ELF"
	@echo "  usr          Build minimal test userland"
	@echo "  run          Build kernel + userland and run in QEMU"
	@echo "  debug        Build debug kernel + userland and run in QEMU"
	@echo "  release      Build release kernel + userland and run in QEMU"
	@echo "  gdb          Connect GDB to running QEMU debug session"
	@echo "  clean        Remove build artifacts"
	@echo "  rebuild      Clean and rebuild"
	@echo ""
	@echo "Architectures: ${SUPPORTED_ARCHES}"

.PHONY: $(PHONY)
