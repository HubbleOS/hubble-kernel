BOOT_CC  := x86_64-w64-mingw32-gcc
BOOT_ASM := nasm

BOOT_SRC_DIR := $(ROOT_DIR)/arch/x86/boot
BOOT_BUILD   := $(BUILD_DIR)/boot

BOOT_CFLAGS := -ffreestanding -O2 -Wall -Wextra \
	-fno-stack-protector -fshort-wchar \
	-m64 -mno-red-zone \
	-I$(ROOT_DIR)/include \
	-I$(BOOT_SRC_DIR)/include \
	-I$(BOOT_SRC_DIR)/ \
	-I$(ROOT_DIR)/arch/x86/include

BOOT_LDFLAGS := -nostdlib \
	-Wl,-dll \
	-Wl,--subsystem,10 \
	-Wl,-e,_start

EFI_OUT := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
EFI_BIN := $(BOOT_BUILD)/BOOTx64.EFI

# Всі .c і .asm рекурсивно
BOOT_SRCS     := $(shell find $(BOOT_SRC_DIR) -name "*.c")
BOOT_ASM_SRCS := $(shell find $(BOOT_SRC_DIR) -name "*.asm")

# Зберігаємо структуру підпапок в build/
BOOT_OBJS     := $(patsubst $(BOOT_SRC_DIR)/%,$(BOOT_BUILD)/%.o,$(BOOT_SRCS))
BOOT_ASM_OBJS := $(patsubst $(BOOT_SRC_DIR)/%,$(BOOT_BUILD)/%.o,$(BOOT_ASM_SRCS))

.PHONY: all bootloader

all: bootloader

# .asm -> .o
$(BOOT_BUILD)/%.asm.o: $(BOOT_SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(BOOT_ASM) -f win64 $< -o $@

# .c -> .o
$(BOOT_BUILD)/%.c.o: $(BOOT_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(BOOT_CC) $(BOOT_CFLAGS) -c $< -o $@

# лінкуємо всё в EFI
$(EFI_BIN): $(BOOT_ASM_OBJS) $(BOOT_OBJS)
	@mkdir -p $(dir $@)
	$(BOOT_CC) $(BOOT_LDFLAGS) $^ -o $@

# копіюємо на місце
bootloader: $(EFI_BIN)
	@mkdir -p $(dir $(EFI_OUT))
	cp $(EFI_BIN) $(EFI_OUT)
