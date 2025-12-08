# arch/x86/Makefile

SRC_DIR := kernel
APPS_DIR := apps
BOOT_SRC := boot

BUILD_TOOL ?= $(abspath ../../tools/dev/build/build/build_main)
LOG_DIR ?= $(OUT_DIR)/logs

CFLAGS_EXTRA := -mcmodel=kernel -g
CXXFLAGS_EXTRA := -mcmodel=kernel -g
ASMFLAGS := -f elf64 -g -F dwarf

# Output paths
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel
APPS_BUILD_DIR := $(BUILD_DIR)/apps
BOOT_BUILD_DIR := $(BUILD_DIR)/boot

OUTPUT := $(BUILD_DIR)/kernel.elf
ISO := $(ISO_DIR)/kernel.bin

BOOT_SO := $(BOOT_BUILD_DIR)/boot.so
EFI_OUT := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
EFI_OUT_DIR := $(ISO_DIR)/EFI/BOOT

.PHONY: all bootloader build iso kernel apps boot-objs

# Default target
all: build iso bootloader

# ===========================================================
# Сборка kernel (C)
# ===========================================================
.PHONY: kernel
kernel: $(BUILD_TOOL)
	@echo "🔨 Building kernel sources with BUILD_TOOL..."
	$(BUILD_TOOL) \
		--src-dir $(SRC_DIR) \
		--build-dir $(KERNEL_BUILD_DIR) \
		--cc $(CC) \
		--asm nasm \
		--cflags "$(CFLAGS) $(CFLAGS_EXTRA)" \
		--asmflags "$(ASMFLAGS)" \
		--includes "$(INCLUDES)" \
		--log-file $(LOG_FILE) \
		-v

# ===========================================================
# Сборка apps (C++)
# ===========================================================
.PHONY: apps
apps: $(BUILD_TOOL)
	@echo "🔨 Building C++ apps with BUILD_TOOL..."
	$(BUILD_TOOL) \
		--src-dir $(APPS_DIR) \
		--build-dir $(APPS_BUILD_DIR) \
		--cxx $(CXX) \
		--cflags "$(CXXFLAGS) $(CXXFLAGS_EXTRA)" \
		--includes "$(INCLUDES)" \
		--log-file $(LOG_FILE) \
		-v; 

# ===========================================================
# Сборка bootloader объектных файлов с помощью BUILD_TOOL
# ===========================================================
.PHONY: boot-objs
boot-objs: $(BUILD_TOOL)
	@echo "🔨 Building bootloader objects with BUILD_TOOL..."
	$(BUILD_TOOL) \
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
build: kernel apps boot-objs

# ===========================================================
# Линковка kernel.elf
# ===========================================================
$(OUTPUT): build
	@echo "🔗 Linking kernel ELF: $@"
	@mkdir -p $(dir $@)
	@OBJS="$$(find $(KERNEL_BUILD_DIR) $(APPS_BUILD_DIR) -type f -name '*.o' 2>/dev/null)"; \
	KERNEL_MAIN="$(KERNEL_BUILD_DIR)/kmain.o"; \
	LINK_OBJS="$$KERNEL_MAIN $$(echo $$OBJS | tr ' ' '\n' | grep -v kmain.o | tr '\n' ' ')"; \
	echo "LINK_OBJS: $$LINK_OBJS"; \
	$(CXX) $(LDFLAGS_NOSTDLIB)$(SRC_DIR)/kernel.ld -o $@ $$LINK_OBJS $(BUILD_DIR)/libc.a

# ===========================================================
# Создание kernel.bin для ISO
# ===========================================================
iso: $(ISO)

$(ISO): $(OUTPUT)
	@echo "📀 Creating kernel binary: $@"
	@mkdir -p $(dir $@)
	$(MINGW_OBJCOPY) -O $(OBJCPYFLAGS) $< $@

# ===========================================================
# Линковка bootloader
# ===========================================================
$(BOOT_SO): boot-objs
	@echo "🔗 Linking bootloader SO: $@"
	@mkdir -p $(dir $@)
	@BOOT_OBJS="$$(find $(BOOT_BUILD_DIR) -type f -name '*.o' 2>/dev/null | tr '\n' ' ')"; \
	$(LD) $(BOOT_LDFLAGS) gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o $$BOOT_OBJS -o $@ $(BOOT_LIBS)

# ===========================================================
# Создание EFI binary
# ===========================================================
bootloader: $(EFI_OUT)

$(EFI_OUT): $(BOOT_SO)
	@echo "💾 Creating EFI binary: $@"
	@mkdir -p $(EFI_OUT_DIR)
	$(MINGW_OBJCOPY) $(EFI_SECTIONS) --target=efi-app-x86_64 --subsystem=10 $< $@
# 	$(HOST_OBJCOPY) $(EFI_SECTIONS) --target=efi-app-x86_64 --subsystem=10 $< $@

# ===========================================================
# Вспомогательные targets
# ===========================================================
.PHONY: clean
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
