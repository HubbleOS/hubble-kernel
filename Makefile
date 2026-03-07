# Main Makefile
Q = @
export Q

ROOT_DIR := $(abspath .)
export ROOT_DIR


ARCH ?= x86
OUT_DIR ?= $(abspath out)
BUILD_DIR := $(OUT_DIR)/build/$(ARCH)
ARCH_DIR := $(abspath arch/$(ARCH))

TOOLS_DIR := tools
CONFIG_MK := tools/config/config.mk
DEV_TOOLS_DIR := tools/dev
BUILD_TOOL := $(OUT_DIR)/tools/dev/build/build_main

SUPPORTED_ARCHES := x86 x86_64 arm64

ifneq ($(ARCH),$(filter $(ARCH),$(SUPPORTED_ARCHES)))
  $(error Unsupported architecture: $(ARCH). Supported architectures are: $(SUPPORTED_ARCHES))
endif

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel
CFLAGS += -g
ASMFLAGS := -f elf64 -F dwarf
ASMFLAGS += -g

# Cross compiler
ifeq ($(ARCH),x86)
	CROSS = x86_64-elf-
	ASM = nasm
	CFLAGS += -m64
endif

ifeq ($(ARCH), x86_64)
	CROSS = x86_64-elf-
	ASM = nasm
	CFLAGS += -m64
endif

ifeq ($(ARCH),arm64)
	CROSS = aarch64-elf-
endif

CFLAGS += -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-avx -mno-avx2
CFLAGS += -mno-red-zone

LD = $(CROSS)ld
CC = $(CROSS)gcc
AS = $(CROSS)as
AR = $(CROSS)ar
OBJCOPY = $(CROSS)objcopy

export LD CC AS AR OBJCOPY ASM
export CFLAGS ASMFLAGS
export ARCH OUT_DIR BUILD_DIR ARCH_DIR CONFIG_MK TOOLS_DIR BUILD_TOOL

INCLUDES += -I$(abspath include)
INCLUDES += -I$(ARCH_DIR)/include
INCLUDES += -I$(ARCH_DIR)/kernel
export INCLUDES

LIB_DIR := $(abspath lib)
export LIB_DIR

LOG_DIR := $(OUT_DIR)/logs/$(shell date +%Y-%m-%d)
LOG_FILE := $(LOG_DIR)/$(shell date +%H-%M-%S).log
export LOG_DIR LOG_FILE

define kbuild-subdir
  obj-y :=
  subdir-y :=
  include $(1)/Makefile
  subdirs += $$(addprefix $(1)/, $$(subdir-y))
  obj-y :=
  subdir-y :=
endef

subdirs :=

# Architecture-specific directories
$(eval $(call kbuild-subdir,arch/$(ARCH)))

# Core subsystems
$(eval $(call kbuild-subdir,lib))
$(eval $(call kbuild-subdir,tools/dev))

# User space
subdirs += usr
USR_DIR := $(abspath usr)
export USR_DIR

# Tools Makefiles
include tools/dev/Makefile

ISO_DIR := $(BUILD_DIR)/iso
export ISO_DIR

BUILD_TOOL_FLAGS := --log-file $(LOG_FILE) -v

PHONY += all
all:
	@$(MAKE) -C tools/dev/shell run

PHONY += build-tool
build-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/build build

PHONY += build
build: build-tool
	$(Q)set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir BUILD_TOOL_FLAGS="$(BUILD_TOOL_FLAGS)"; \
	done
	@echo "Build complete"

PHONY += demo
demo:
	@${MAKE} -C tools/dev/demo run

PHONY += stats
stats:
	out/tools/dev/stats/stats

PHONY += debug
debug:
	out/tools/dev/debug/debug

PHONY += disk
disk:
	@mkdir -p out/disks
	out/tools/dev/disk/disk out/disks/disk.img 64 out/usr

PHONY += run
run: build
	out/tools/dev/qemu/qemu

PHONY += clean
clean:
	@rm -rf $(OUT_DIR)
	@echo "Clean complete"

PHONY += rebuild
rebuild: clean build

PHONY += help
help:
	@echo "Kernel Build System with BUILD_TOOL"
	@echo ""
	@echo "Usage:"
	@echo "  make [TARGET] [ARCH=<arch>]"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build the kernel (default)"
	@echo "  build          - Build the kernel with BUILD_TOOL"
	@echo "  rebuild        - Clean and build from scratch"
	@echo "  run            - Run the built kernel via QEMU"
	@echo "  clean          - Remove all build output"
	@echo "  mkvars         - Print key build variables"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  ARCH           - Target architecture (default: x86)"
	@echo "  Available: x86, x86_64, arm64"

PHONY += mkvars
mkvars:
	@echo "Build Variables:"
	@echo "  ARCH         = $(ARCH)"
	@echo "  OUT_DIR      = $(OUT_DIR)"
	@echo "  ARCH_DIR     = $(ARCH_DIR)"
	@echo "  BUILD_TOOL   = $(BUILD_TOOL)"
	@echo "  INCLUDES     = $(INCLUDES)"
	@echo "  subdirs      = $(subdirs)"

.PHONY: $(PHONY)
