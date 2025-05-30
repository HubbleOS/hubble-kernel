# tools/config/config.mk
ARCH ?= x86

OUT_DIR ?= out
BUILD_DIR := $(abspath $(OUT_DIR)/$(ARCH)/build)
ISO_DIR := $(abspath $(OUT_DIR)/$(ARCH)/iso)
ARCH_DIR := arch/$(ARCH)

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

# Cross compiler
ifeq ($(ARCH),x86)
	CROSS = x86_64-elf-

	LD = $(CROSS)ld
	CC = $(CROSS)gcc
	CXX = $(CROSS)g++
	AS = $(CROSS)as
	AR = $(CROSS)ar
	OBJCOPY = $(CROSS)objcopy
	HOST_OBJCOPY = objcopy

	CFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra -c
	CXXFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra -c
	LDFLAGS = -nostdlib -T kernel/linker.ld
	OBJCPYFLAGS = binary
	
	BOOT_CFLAGS = -Iinclude -Ignu-efi/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -c
	BOOT_LDFLAGS = -shared -Bsymbolic -Lgnu-efi/x86_64/lib -Lgnu-efi/x86_64/gnuefi -Tgnu-efi/gnuefi/elf_x86_64_efi.lds
	BOOT_LIBS = -lgnuefi -lefi
	EFI_SECTIONS = -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc
endif

ifeq ($(ARCH),arm64)
endif

INCLUDES := -Iinclude

LIBS := -lefi -lgnuefi
