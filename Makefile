# Main Makefile
Q = @
export Q

ARCH ?= x86

OUT_DIR ?= $(abspath out)
BUILD_DIR := $(OUT_DIR)/$(ARCH)/build
ISO_DIR := $(OUT_DIR)/$(ARCH)/iso
ARCH_DIR := $(abspath arch/$(ARCH))

LIB_DIR := lib
TOOLS_DIR := tools

CONFIG_MK := $(TOOLS_DIR)/config/config.mk
DEV_TOOLS_DIR := $(TOOLS_DIR)/dev
SCRIPT_DIR := $(TOOLS_DIR)/scripts
BUILD_TOOL := $(OUT_DIR)/tools/dev/build/build_main
STATS_TOOL := $(OUT_DIR)/tools/dev/stats/stats
DEBUG_TOOL := $(OUT_DIR)/tools/dev/debug/debug_tool
QEMU_TOOL := $(OUT_DIR)/tools/dev/qemu/qemu

SUPPORTED_ARCHES := x86 x86_64 arm64
ifneq ($(ARCH),$(filter $(ARCH),$(SUPPORTED_ARCHES)))
  $(error Unsupported architecture: $(ARCH). Supported architectures are: $(SUPPORTED_ARCHES))
endif

CFLAGS = -ffreestanding -O2 -Wall -Wextra -mcmodel=kernel
CFLAGS += -g

ASMFLAGS := -f elf64 -g -F dwarf

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

LD = $(CROSS)ld
CC = $(CROSS)gcc
AS = $(CROSS)as
AR = $(CROSS)ar
OBJCOPY = $(CROSS)objcopy

HOST_LD = ld
HOST_CC = gcc
HOST_AS = as
HOST_AR = ar
HOST_OBJCOPY = objcopy

MINGW_PREFIX = x86_64-w64-mingw32-
MINGW_OBJCOPY = $(MINGW_PREFIX)objcopy
MINGW_OBJDUMP = $(MINGW_PREFIX)objdump
MINGW_LD = $(MINGW_PREFIX)ld

OBJCPYFLAGS = binary

GNU_EFI_DIR := $(ARCH_DIR)/gnu-efi

BOOT_CFLAGS = -Iinclude -Ikernel \
	-I$(GNU_EFI_DIR) -I$(GNU_EFI_DIR)/inc \
	-fpic -ffreestanding -fno-stack-protector \
	-fno-stack-check -fshort-wchar -mno-red-zone \
	-maccumulate-outgoing-args -c

BOOT_LDFLAGS = -shared -Bsymbolic \
	-L$(GNU_EFI_DIR)/x86_64/lib \
	-L$(GNU_EFI_DIR)/x86_64/gnuefi \
	-T$(GNU_EFI_DIR)/gnuefi/elf_x86_64_efi.lds

# Правильные пути к библиотекам
BOOT_LIBS = $(GNU_EFI_DIR)/x86_64/gnuefi/libgnuefi.a \
            $(GNU_EFI_DIR)/x86_64/lib/libefi.a

EFI_SECTIONS = -j .text -j .sdata -j .data -j .rodata \
	-j .dynamic -j .dynsym -j .rel -j .rela \
	-j .rel.* -j .rela.* -j .reloc

export GNU_EFI_DIR

export LD CC AS AR OBJCOPY ASM
export HOST_LD HOST_CC HOST_AS HOST_AR HOST_OBJCOPY

export MINGW_PREFIX MINGW_OBJCOPY MINGW_OBJDUMP MINGW_LD

export CFLAGS OBJCPYFLAGS ASMFLAGS
export BOOT_CFLAGS BOOT_LDFLAGS BOOT_LIBS EFI_SECTIONS

export ARCH
export OUT_DIR
export BUILD_DIR
export ISO_DIR
export ARCH_DIR
export LIB_DIR
export CONFIG_MK
export TOOLS_DIR
export SCRIPT_DIR
export BUILD_TOOL

INCLUDES += -I$(abspath include)
INCLUDES += -I$(ARCH_DIR)/include
INCLUDES += -I$(ARCH_DIR)/kernel

export INCLUDES

LOG_DIR := $(OUT_DIR)/logs/$(shell date +%Y-%m-%d)
LOG_FILE := $(LOG_DIR)/$(shell date +%H-%M-%S).log

export LOG_DIR
export LOG_FILE

PHONY += all
all:
	@$(MAKE) -C $(TOOLS_DIR)/dev

###########################################################################

subdirs += $(LIB_DIR)
subdirs += $(ARCH_DIR) 
subdirs += usr

# Собираем BUILD_TOOL перед началом сборки
PHONY += build-tool
build-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/build build

PHONY += stats-tool
stats-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/stats build

PHONY += stats
stats: stats-tool
	@$(STATS_TOOL)

PHONY += debug-tool
debug-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/debug build

PHONY += debug
debug: debug-tool
	@$(DEBUG_TOOL) 

PHONY += qemu-tool
qemu-tool:
	@$(MAKE) -C $(DEV_TOOLS_DIR)/qemu build

BUILD_TOOL_FLAGS := --log-file $(LOG_FILE)

PHONY += build
build: build-tool
	$(Q)set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir BUILD_TOOL_FLAGS="$(BUILD_TOOL_FLAGS)"; \
	done

###########################################################################

PHONY += run
run: build
	$(MAKE) host-run

PHONY += host-run
host-run: qemu-tool
	$(QEMU_TOOL)

###########################################################################

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
	@echo "  make [TARGET] [ARCH=<arch>] [VARIABLE=value]"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build the kernel (default)"
	@echo "  build          - Build the kernel with BUILD_TOOL"
	@echo "  rebuild        - Clean and build from scratch"
	@echo "  run            - Run the built kernel via QEMU (on host)"
	@echo "  qemu           - Run QEMU using current build "
	@echo "  img            - Build the kernel image (if implemented)"
	@echo "  flash          - Flash the kernel image to a USB device (if implemented)"
	@echo "  clean          - Remove all build output"
	@echo "  mkvars         - Print key build variables (debug info)"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "Docker Targets:"
	@echo "  docker-build   - Build the kernel inside Docker container"
	@echo "  docker-run     - Build in Docker, run kernel on host QEMU"
	@echo "  docker-clean   - Clean build output via Docker"
	@echo "  docker-<target>- Run any target inside Docker, e.g., 'make docker-img'"
	@echo ""
	@echo "Host-only Targets:"
	@echo "  host-run       - Run QEMU from host using current build output"
	@echo ""
	@echo "Variables:"
	@echo "  ARCH           - Target architecture (e.g. x86, arm64). Default: x86"
	@echo "  DOCKER_RUN     - Override Docker run command if needed"
	@echo ""
	@echo "Available ARCH values:"
	@echo "  x86, arm64 (extendable in config/config.mk and docker-compose.yml)"
	@echo ""

PHONY += mkvars
mkvars:
	@echo "Build Variables:"
	@echo "  ARCH         = $(ARCH)"
	@echo "  OUT_DIR      = $(OUT_DIR)"
	@echo "  BUILD_DIR    = $(BUILD_DIR)"
	@echo "  ISO_DIR      = $(ISO_DIR)"
	@echo "  ARCH_DIR     = $(ARCH_DIR)"
	@echo "  LIB_DIR      = $(LIB_DIR)"
	@echo "  CONFIG_MK    = $(CONFIG_MK)"
	@echo "  TOOLS_DIR    = $(TOOLS_DIR)"
	@echo "  SCRIPT_DIR   = $(SCRIPT_DIR)"
	@echo "  BUILD_TOOL   = $(BUILD_TOOL)"
	@echo "  INCLUDES     = $(INCLUDES)"
	@echo "  DOCKER_RUN   = $(DOCKER_RUN)"
	@echo "  subdirs      = $(subdirs)"

include $(SCRIPT_DIR)/scripts.mk
include $(SCRIPT_DIR)/docker/docker.mk

.PHONY: $(PHONY)
