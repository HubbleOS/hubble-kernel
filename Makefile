Q = @
export Q

ARCH ?= x86

OUT_DIR ?= $(abspath out)
BUILD_DIR := $(abspath $(OUT_DIR)/$(ARCH)/build)
ISO_DIR := $(abspath $(OUT_DIR)/$(ARCH)/iso)
ARCH_DIR := $(abspath arch/$(ARCH))

LIB_DIR := $(abspath lib)
CONFIG_MK := $(abspath tools/config/config.mk)

TOOLS_DIR := tools
SCRIPT_DIR := $(abspath $(TOOLS_DIR)/scripts)

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
  $(warning ⚠️  You are running inside WSL. Please ensure Docker Desktop's WSL 2 integration is enabled:
  $(warning https://docs.docker.com/docker-for-windows/wsl/)
endif

SUPPORTED_ARCHES := x86 arm64
ifneq ($(ARCH),$(filter $(ARCH),$(SUPPORTED_ARCHES)))
  $(error Unsupported architecture: $(ARCH). Supported architectures are: $(SUPPORTED_ARCHES))
endif

# Cross compiler
ifeq ($(ARCH),x86)
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

CFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra -c
CXXFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra -c
LDFLAGS = -nostdlib -T
OBJCPYFLAGS = binary

BOOT_CFLAGS = -Iinclude -Ignu-efi/inc \
			-fpic -ffreestanding -fno-stack-protector \
			-fno-stack-check -fshort-wchar -mno-red-zone \
			-maccumulate-outgoing-args -c

BOOT_LDFLAGS = -shared -Bsymbolic -Lgnu-efi/x86_64/lib \
		 -Lgnu-efi/x86_64/gnuefi -Tgnu-efi/gnuefi/elf_x86_64_efi.lds

BOOT_LIBS = -lgnuefi -lefi

EFI_SECTIONS = -j .text -j .sdata -j .data -j .rodata \
		-j .dynamic -j .dynsym -j .rel -j .rela \
		-j .rel.* -j .rela.* -j .reloc

export LD CC CXX AS AR OBJCOPY
export HOST_LD HOST_CC HOST_CXX HOST_AS HOST_AR HOST_OBJCOPY

export CFLAGS CXXFLAGS LDFLAGS OBJCPYFLAGS
export BOOT_CFLAGS BOOT_LDFLAGS BOOT_LIBS EFI_SECTIONS

# export ARCH
export OUT_DIR
export BUILD_DIR
export ISO_DIR
export ARCH_DIR
export LIB_DIR
export CONFIG_MK
export TOOLS_DIR
export SCRIPT_DIR

INCLUDES += -I$(abspath include)
INCLUDES += -I$(LIB_DIR)/libc/include
INCLUDES += -I$(ARCH_DIR)/include 

export INCLUDES

PHONY += all
all:
	@$(MAKE) -C tools/dev-tool

###########################################################################

subdirs += $(LIB_DIR)
subdirs += $(ARCH_DIR) 

GNU_EFI_BUILD_DIR := $(OUT_DIR)/$(ARCH)/gnu-efi
GNU_EFI_BUILT_MARK := $(GNU_EFI_BUILD_DIR)/.built

gnu-efi: $(GNU_EFI_BUILT_MARK)

$(GNU_EFI_BUILT_MARK):
	$(MAKE) -C $(ARCH_DIR)/gnu-efi
	@mkdir -p $(dir $@)
	@touch $@


LOG_DIR := $(OUT_DIR)/logs

prepare-log-dir:
	@mkdir -p $(LOG_DIR)

PHONY += build
build: gnu-efi | prepare-log-dir
	@timestamp=$$(date +%Y%m%d-%H%M%S); \
	logfile="$(LOG_DIR)/build $$timestamp.log"; \
	echo "📦 Logging build to $$logfile"; \
	{ \
		echo "== Build started at $$(date) =="; \
		for dir in $(subdirs); do \
			$(MAKE) -C $$dir; \
		done; \
		echo "== Build finished at $$(date) =="; \
	} 2>&1 | tee "$$logfile"
	@echo "✅ Build complete for $(ARCH)"

# PHONY += build
# build: gnu-efi
# 	$(Q)set -e; \
# 	for dir in $(subdirs); do \
#         $(MAKE) -C $$dir; \
#     done
# 	@echo "✅ Build complete for $(ARCH)"
###########################################################################

PHONY += run
run: build
	@echo "🚀 Running kernel for $(ARCH)..."
	$(MAKE) host-run

PHONY += host-run
host-run:
	@echo "🖥  Launching QEMU from host..."
	$(MAKE) $(SCRIPT_DIR) qemu

PHONY += clean
clean:
	@echo "🧹 Cleaning build output for $(ARCH)..."
	@rm -rf $(OUT_DIR)
	@echo "✅ Clean complete"

PHONY += help
help:
	@echo "🧰 Kernel Build System"
	@echo ""
	@echo "📦 Usage:"
	@echo "  make [TARGET] [ARCH=<arch>] [VARIABLE=value]"
	@echo ""
	@echo "🎯 Targets:"
	@echo "  all            - Build the kernel (default)"
	@echo "  build          - Build the kernel"
	@echo "  run            - Run the built kernel via QEMU (on host)"
	@echo "  qemu           - Run QEMU using current build "
	@echo "  img            - Build the kernel image (if implemented)"
	@echo "  flash          - Flash the kernel image to a USB device (if implemented)"
	@echo "  clean          - Remove all build output"
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
	@echo "  LIB_DIR     = $(LIB_DIR)"
	@echo "  CONFIG_MK    = $(CONFIG_MK)"
	@echo "  TOOLS_DIR    = $(TOOLS_DIR)"
	@echo "  SCRIPT_DIR   = $(SCRIPT_DIR)"
	@echo "  INCLUDES     = $(INCLUDES)"
	@echo "  DOCKER_RUN   = $(DOCKER_RUN)"
	@echo "  subdirs      = $(subdirs)"

include $(SCRIPT_DIR)/scripts.mk
include $(SCRIPT_DIR)/docker/docker.mk

.PHONY: $(PHONY)
