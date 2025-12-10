# arch/x86/Makefile
SRC_DIR := kernel
BOOT_SRC := boot

# Output paths
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel
BOOT_BUILD_DIR := $(BUILD_DIR)/boot

OUTPUT := $(BUILD_DIR)/kernel.elf
ISO := $(ISO_DIR)/kernel.bin

BOOT_SO := $(BOOT_BUILD_DIR)/boot.so
EFI_OUT := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
EFI_OUT_DIR := $(ISO_DIR)/EFI/BOOT

.PHONY: all bootloader build iso kernel boot-objs

# Default target
all: build iso bootloader

# ===========================================================
# Сборка kernel (C)
# ===========================================================
.PHONY: kernel
kernel: $(BUILD_TOOL)
	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(SRC_DIR) \
		--build-dir $(KERNEL_BUILD_DIR) \
		--cc $(CC) \
		--asm $(ASM) \
		--cflags "$(CFLAGS)" \
		--asmflags "$(ASMFLAGS)" \
		--includes "$(INCLUDES)" \
		--log-file $(LOG_FILE) \
		-v

# ===========================================================
# Сборка bootloader объектных файлов с помощью BUILD_TOOL
# ===========================================================
.PHONY: boot-objs
boot-objs: $(BUILD_TOOL)
	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(BOOT_SRC) \
		--build-dir $(BOOT_BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(BOOT_CFLAGS)" \
		--includes "$(INCLUDES)" \
		--log-file $(LOG_FILE) \
		-v

# ===========================================================
# Общая сборка всех компонентов
# ===========================================================
build: kernel boot-objs

# ===========================================================
# Линковка kernel.elf
# ===========================================================
$(OUTPUT): build
	@mkdir -p $(dir $@)
	@OBJS="$$(find $(KERNEL_BUILD_DIR) -type f -name '*.o' 2>/dev/null)"; \
	KERNEL_MAIN="$(KERNEL_BUILD_DIR)/kmain.o"; \
	LINK_OBJS="$$KERNEL_MAIN $$(echo $$OBJS | tr ' ' '\n' | grep -v kmain.o | tr '\n' ' ')"; \
	echo "LINK_OBJS: $$LINK_OBJS"; \
	$(LD) -nostdlib -T $(SRC_DIR)/kernel.ld -o $@ $$LINK_OBJS

# ===========================================================
# Создание kernel.bin для ISO
# ===========================================================
iso: $(ISO)

$(ISO): $(OUTPUT)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O $(OBJCPYFLAGS) $< $@

# ===========================================================
# Линковка bootloader
# ===========================================================
$(BOOT_SO): boot-objs
	@mkdir -p $(dir $@)
	@BOOT_OBJS="$$(find $(BOOT_BUILD_DIR) -type f -name '*.o' 2>/dev/null | tr '\n' ' ')"; \
	$(LD) $(BOOT_LDFLAGS) gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o $$BOOT_OBJS -o $@ $(BOOT_LIBS)

# ===========================================================
# Создание EFI binary
# ===========================================================
bootloader: $(EFI_OUT)

$(EFI_OUT): $(BOOT_SO)
	@mkdir -p $(EFI_OUT_DIR)
	$(OBJCOPY) $(EFI_SECTIONS) --target=efi-app-x86_64 --subsystem=10 $< $@
