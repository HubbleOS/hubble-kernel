BOOT_SRC := .
BOOT_BUILD_DIR := $(BUILD_DIR)/boot

BOOT_SO := $(BOOT_BUILD_DIR)/boot.so
EFI_OUT := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
EFI_OUT_DIR := $(ISO_DIR)/EFI/BOOT

GNU_EFI_DIR := $(LIB_DIR)/gnu-efi

BOOT_CFLAGS = -Iinclude -Ikernel \
	-I$(GNU_EFI_DIR) -I$(GNU_EFI_DIR)/inc \
	-fpic -ffreestanding -fno-stack-protector \
	-fno-stack-check -fshort-wchar -mno-red-zone \
	-maccumulate-outgoing-args -c

BOOT_LDFLAGS = -shared -Bsymbolic \
	-L$(GNU_EFI_DIR)/x86_64/lib \
	-L$(GNU_EFI_DIR)/x86_64/gnuefi \
	-T$(GNU_EFI_DIR)/gnuefi/elf_x86_64_efi.lds \
	$(GNU_EFI_DIR)/x86_64/gnuefi/crt0-efi-x86_64.o

BOOT_LIBS = $(GNU_EFI_DIR)/x86_64/gnuefi/libgnuefi.a \
            $(GNU_EFI_DIR)/x86_64/lib/libefi.a

EFI_SECTIONS = -j .text -j .sdata -j .data -j .rodata \
	-j .dynamic -j .dynsym -j .rel -j .rela \
	-j .rel.* -j .rela.* -j .reloc

.PHONY: all 

all: boot-objs bootloader

.PHONY: boot-objs
boot-objs: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(BOOT_SRC) \
		--build-dir $(BOOT_BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(BOOT_CFLAGS)" \
		--includes "$(INCLUDES)" \
		--output $(BOOT_SO) \
		--type exe \
		--ld $(LD) \
		--ldflags "$(BOOT_LDFLAGS)" \
		--libs "$(BOOT_LIBS)"

# EFI bootloader
.PHONY: bootloader
bootloader: $(EFI_OUT)
$(EFI_OUT): $(BOOT_SO)
	@mkdir -p $(EFI_OUT_DIR)
	$(OBJCOPY) $(EFI_SECTIONS) --target=efi-app-x86_64 --subsystem=10 $< $@
