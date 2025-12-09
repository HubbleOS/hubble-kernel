# Main Makefile
Q = @
export Q

ARCH ?= x86

OUT_DIR ?= $(abspath out)
BUILD_DIR := $(OUT_DIR)/$(ARCH)/build
ISO_DIR := $(OUT_DIR)/$(ARCH)/iso
ARCH_DIR := $(abspath arch/$(ARCH))

LIB_DIR := $(abspath lib)
CONFIG_MK := $(abspath tools/config/config.mk)

TOOLS_DIR := tools
DEV_TOOLS_DIR := $(TOOLS_DIR)/dev
SCRIPT_DIR := $(TOOLS_DIR)/scripts
BUILD_TOOL := $(OUT_DIR)/tools/dev/build/build_main
STATS_TOOL := $(OUT_DIR)/tools/dev/stats/stats
DEBUG_TOOL := $(OUT_DIR)/tools/dev/debug/debug_tool
QEMU_TOOL := $(OUT_DIR)/tools/dev/qemu/qemu

UNAME_S := $(shell uname -s)

WIN_NAMES := CYGWIN MINGW MSYS

IS_WIN := $(filter-out ,$(foreach w,$(WIN_NAMES),$(findstring $(w),$(UNAME_S))))

ifeq ($(IS_WIN),)
  DOCKER_RUN := docker-compose run --rm $(ARCH)-builder
else
  DOCKER_RUN := powershell.exe -File $(SCRIPT_DIR)/docker/docker-run.ps1 $(ARCH)-builder
endif

IS_WSL := $(findstring Microsoft,$(UNAME_S))

ifeq ($(IS_WSL),Microsoft)
  $(warning ⚠️  You are running inside WSL. Please ensure Docker Desktop's WSL 2 integration is enabled:)
  $(warning https://docs.docker.com/docker-for-windows/wsl/)
endif

SUPPORTED_ARCHES := x86 x86_64 arm64
ifneq ($(ARCH),$(filter $(ARCH),$(SUPPORTED_ARCHES)))
  $(error Unsupported architecture: $(ARCH). Supported architectures are: $(SUPPORTED_ARCHES))
endif

# Cross compiler
ifeq ($(ARCH),x86)
	CROSS = x86_64-elf-
endif
ifeq ($(ARCH), x86_64)
	CROSS = x86_64-elf-
endif
ifeq ($(ARCH),arm64)
	CROSS = aarch64-elf-
endif

LD = $(CROSS)ld
CC = $(CROSS)gcc
CXX = $(CROSS)g++
AS = $(CROSS)as
AR = $(CROSS)ar
OBJCOPY = $(CROSS)objcopy

HOST_LD = ld
HOST_CC = gcc
HOST_CXX = g++
HOST_AS = as
HOST_AR = ar
HOST_OBJCOPY = objcopy

MINGW_PREFIX = x86_64-w64-mingw32-
MINGW_OBJCOPY = $(MINGW_PREFIX)objcopy
MINGW_OBJDUMP = $(MINGW_PREFIX)objdump
MINGW_LD = $(MINGW_PREFIX)ld

CFLAGS = -MMD -MP -ffreestanding -m64 -O2 -Wall -Wextra -c
CXXFLAGS = -MMD -MP -ffreestanding -fno-exceptions -fno-rtti -m64 -O2 -Wall -Wextra -c
LDFLAGS_NOSTDLIB = -nostdlib -T
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

export LD CC CXX AS AR OBJCOPY
export HOST_LD HOST_CC HOST_CXX HOST_AS HOST_AR HOST_OBJCOPY

export MINGW_PREFIX MINGW_OBJCOPY MINGW_OBJDUMP MINGW_LD

export CFLAGS CXXFLAGS LDFLAGS_NOSTDLIB OBJCPYFLAGS
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
	@$(MAKE) -C tools/dev

###########################################################################

subdirs += $(LIB_DIR)
subdirs += $(ARCH_DIR) 

USR_DIR := $(abspath usr)
subdirs += $(USR_DIR)/src

# Собираем BUILD_TOOL перед началом сборки
PHONY += build-tool
build-tool:
	@echo "🔨 Building BUILD_TOOL..."
	@$(MAKE) -C $(DEV_TOOLS_DIR)/build build

PHONY += stats-tool
stats-tool:
	@echo "🔨 Building STATS_TOOL..."
	@$(MAKE) -C $(DEV_TOOLS_DIR)/stats build

PHONY += stats
stats: stats-tool
	@echo "📊 Running STATS_TOOL..."
	@$(STATS_TOOL)

PHONY += debug-tool
debug-tool:
	@echo "🔨 Building DEBUG_TOOL..."
	@$(MAKE) -C $(DEV_TOOLS_DIR)/debug build

PHONY += debug
debug: debug-tool
	@echo "🐞 Running DEBUG_TOOL..."
	@$(DEBUG_TOOL) 

PHONY += qemu-tool
qemu-tool:
	@echo "🔨 Building QEMU_TOOL..."
	@$(MAKE) -C $(DEV_TOOLS_DIR)/qemu build

PHONY += qemu
qemu: qemu-tool
	@echo "🖥  Running QEMU_TOOL..."
	@$(QEMU_TOOL)

BUILD_TOOL_FLAGS := --log-file $(LOG_FILE)

PHONY += build
build: build-tool
	@echo "🚀 Starting build with BUILD_TOOL for $(ARCH)..."
	$(Q)set -e; \
	for dir in $(subdirs); do \
		$(MAKE) -C $$dir BUILD_TOOL=$(BUILD_TOOL) BUILD_TOOL_FLAGS=""; \
	done
	@echo "✅ Build complete for $(ARCH)"

###########################################################################

PHONY += run
run: build
	@echo "🚀 Running kernel for $(ARCH)..."
	$(MAKE) host-run

PHONY += host-run
host-run: qemu-tool
	@echo "🖥  Launching QEMU from host..."
	$(QEMU_TOOL)

###########################################################################

PHONY += clean
clean:
	@echo "🧹 Cleaning build output for $(ARCH)..."
	@rm -rf $(OUT_DIR)
	@echo "✅ Clean complete"

PHONY += clean-all
clean-all: clean
	@echo "🧹 Cleaning BUILD_TOOL..."
	@$(MAKE) -C $(DEV_TOOLS_DIR)/build clean
	@echo "✅ Full clean complete"

PHONY += rebuild
rebuild: clean build

PHONY += help
help:
	@echo "🧰 Kernel Build System with BUILD_TOOL"
	@echo ""
	@echo "📦 Usage:"
	@echo "  make [TARGET] [ARCH=<arch>] [VARIABLE=value]"
	@echo ""
	@echo "🎯 Targets:"
	@echo "  all            - Build the kernel (default)"
	@echo "  build          - Build the kernel with BUILD_TOOL"
	@echo "  rebuild        - Clean and build from scratch"
	@echo "  run            - Run the built kernel via QEMU (on host)"
	@echo "  qemu           - Run QEMU using current build "
	@echo "  img            - Build the kernel image (if implemented)"
	@echo "  flash          - Flash the kernel image to a USB device (if implemented)"
	@echo "  clean          - Remove all build output"
	@echo "  clean-all      - Remove build output and BUILD_TOOL"
	@echo "  mkvars         - Print key build variables (debug info)"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "🐳 Docker Targets:"
	@echo "  docker-build   - Build the kernel inside Docker container"
	@echo "  docker-run     - Build in Docker, run kernel on host QEMU"
	@echo "  docker-clean   - Clean build output via Docker"
	@echo "  docker-<target>- Run any target inside Docker, e.g., 'make docker-img'"
	@echo ""
	@echo "🖥 Host-only Targets:"
	@echo "  host-run       - Run QEMU from host using current build output"
	@echo ""
	@echo "🛠 Variables:"
	@echo "  ARCH           - Target architecture (e.g. x86, arm64). Default: x86"
	@echo "  DOCKER_RUN     - Override Docker run command if needed"
	@echo ""
	@echo "✅ Available ARCH values:"
	@echo "  x86, arm64 (extendable in config/config.mk and docker-compose.yml)"
	@echo ""

PHONY += mkvars
mkvars:
	@echo "📦 Build Variables:"
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

# DEP_FILES := $(OBJ_FILES:.o=.d)
DEP_FILES = $(shell find $(BUILD_DIR) -name '*.d' 2>/dev/null)

-include $(DEP_FILES)

.PHONY: $(PHONY)
