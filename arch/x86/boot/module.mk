# GNU_EFI_DIR := $(LIB_DIR)/gnu-efi

# BOOT_CFLAGS = -ffreestanding -O2 -Wall -Wextra \
# 	-fpic -fno-stack-protector -fno-stack-check -fshort-wchar \
# 	-m64 -mno-red-zone -maccumulate-outgoing-args \
# 	-Iinclude -Ikernel -I$(GNU_EFI_DIR) -I$(GNU_EFI_DIR)/inc -I$(GNU_EFI_DIR)/inc/x86_64

# BOOT_LDFLAGS = -shared -Bsymbolic \
# 	-L$(GNU_EFI_DIR)/x86_64/lib \
# 	-L$(GNU_EFI_DIR)/x86_64/gnuefi \
# 	-T$(GNU_EFI_DIR)/gnuefi/elf_x86_64_efi.lds \
# 	$(GNU_EFI_DIR)/x86_64/gnuefi/crt0-efi-x86_64.o

# BOOT_LIBS = $(GNU_EFI_DIR)/x86_64/gnuefi/libgnuefi.a \
#             $(GNU_EFI_DIR)/x86_64/lib/libefi.a

# cflags-y := $(BOOT_CFLAGS) -DHAVE_USE_MS_ABI
# exe-ldflags-y := $(BOOT_LDFLAGS)
# exe-output-y := $(BUILD_DIR)/boot/boot.so
# exe-libs-y   := $(BOOT_LIBS)

# exe-y := arch/x86/boot


# Makefile.build
# BOOT_SO := $(BUILD_DIR)/boot/boot.so
# EFI_OUT := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
# EFI_OUT_DIR := $(ISO_DIR)/EFI/BOOT

# EFI_SECTIONS = -j .text -j .sdata -j .data -j .rodata \
# 	-j .dynamic -j .dynsym -j .rel -j .rela \
# 	-j .rel.* -j .rela.* -j .reloc

# .PHONY: all 

# all: bootloader

# # EFI bootloader
# .PHONY: bootloader
# bootloader: $(EFI_OUT)
# $(EFI_OUT): $(BOOT_SO)
# 	@mkdir -p $(EFI_OUT_DIR)
# 	objcopy $(EFI_SECTIONS) --output-target=efi-app-x86_64 --subsystem=10 $< $@
