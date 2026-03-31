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
	-I$(ROOT_DIR)/arch/x86/include # temporary for bootinfo.h

BOOT_LDFLAGS := -nostdlib \
	-Wl,-dll \
	-Wl,--subsystem,10 \
	-Wl,-e,_start

EFI_OUT := $(ISO_DIR)/EFI/BOOT/BOOTx64.EFI
EFI_BIN := $(BOOT_BUILD)/BOOTx64.EFI
CRT0_OBJ := $(BOOT_BUILD)/crt0.o

# Все .c файлы в arch/x86/boot/
# BOOT_SRCS := $(shell find $(BOOT_SRC_DIR) -name "*.c")
BOOT_SRCS := $(wildcard $(BOOT_SRC_DIR)/*.c) # только корневые .c файлы
BOOT_OBJS := $(patsubst $(BOOT_SRC_DIR)/%.c,$(BOOT_BUILD)/%.o,$(BOOT_SRCS))

.PHONY: all bootloader

all: bootloader

# crt0.asm -> crt0.o
$(CRT0_OBJ): $(BOOT_SRC_DIR)/crt0.asm
	@mkdir -p $(BOOT_BUILD)
	$(BOOT_ASM) -f win64 $< -o $@

# .c -> .o
$(BOOT_BUILD)/%.o: $(BOOT_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(BOOT_CC) $(BOOT_CFLAGS) -c $< -o $@

# линкуем всё в EFI
$(EFI_BIN): $(CRT0_OBJ) $(BOOT_OBJS)
	$(BOOT_CC) $(BOOT_LDFLAGS) $^ -o $@

# копируем на место
bootloader: $(EFI_BIN)
	@mkdir -p $(dir $(EFI_OUT))
	cp $(EFI_BIN) $(EFI_OUT)
