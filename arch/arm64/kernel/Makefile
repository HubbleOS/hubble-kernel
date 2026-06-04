CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

BUILD_DIR := build
TARGET_ELF := $(BUILD_DIR)/kernel8.elf
TARGET_IMG := $(BUILD_DIR)/kernel8.img
TARGET_MAP := $(BUILD_DIR)/kernel8.map

CFLAGS := -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-pic -fno-pie \
          -mgeneral-regs-only -mcpu=cortex-a53
ASFLAGS := -x assembler-with-cpp -mcpu=cortex-a53
LDFLAGS := -nostdlib -static -T kernel.ld -Wl,-Map,$(TARGET_MAP)

.PHONY: all quick test disasm clean

all: $(TARGET_IMG)

quick: all

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/start.o: start.asm | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_ELF): $(BUILD_DIR)/start.o $(BUILD_DIR)/main.o kernel.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(BUILD_DIR)/start.o $(BUILD_DIR)/main.o -o $@

$(TARGET_IMG): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

test: $(TARGET_IMG)
	timeout 5s qemu-system-aarch64 -M raspi3b -kernel $(TARGET_IMG) -serial stdio -display none; test $$? -eq 124

disasm: $(TARGET_ELF)
	$(OBJDUMP) -d $<

clean:
	rm -rf $(BUILD_DIR)
