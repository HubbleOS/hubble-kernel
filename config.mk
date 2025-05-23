# config.mk

ARCH ?= x86

OUT_DIR ?= out
BUILD_DIR := $(abspath $(OUT_DIR)/$(ARCH)/build)
ISO_DIR := $(abspath $(OUT_DIR)/$(ARCH)/iso)
ARCH_DIR := arch/$(ARCH)

# Cross compiler
ifeq ($(ARCH),x86)
	CROSS = x86_64-elf-

	LD = $(CROSS)ld
	CC = $(CROSS)gcc
	AS = $(CROSS)as
	OBJCOPY = $(CROSS)objcopy
	HOST_OBJCOPY = objcopy

	CFLAGS = -ffreestanding -m64 -O2 -Wall -Wextra -c
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
