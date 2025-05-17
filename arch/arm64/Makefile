# Cross-compiler prefix
# CROSS = aarch64-unknown-linux-gnu-
CROSS = aarch64-elf-

# Tools
AS = $(CROSS)as
CC = $(CROSS)gcc
LD = $(CROSS)ld
QEMU = qemu-system-aarch64

# Flags
CFLAGS = -ffreestanding -c
LDFLAGS = -nostdlib -T linker.ld

INCLUDES := -Iinclude

# Directories
SRC_DIRS = boot kernel
BUILD_DIR = build
BIN_DIR = bin

# Source files
SRC_FILES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.[csS]))
OBJ_FILES = $(patsubst %.c,$(BUILD_DIR)/%.o,$(patsubst %.s,$(BUILD_DIR)/%.o,$(patsubst %.S,$(BUILD_DIR)/%.o,$(SRC_FILES))))

# Output ELF
OUTPUT = $(BIN_DIR)/kernel.elf

# Default target
all: run

# Build only
build: $(OUTPUT)

# Link
$(OUTPUT): $(OBJ_FILES) linker.ld | $(BIN_DIR)
	@echo "Linking: $@"
	$(LD) $(LDFLAGS) $(OBJ_FILES) -o $@

# Compile/Assemble rules
$(BUILD_DIR)/%.o: %.c | create-dir
	@echo "Compiling C: $< -> $@"
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BUILD_DIR)/%.o: %.s | create-dir
	@echo "Assembling ASM: $< -> $@"
	$(AS) $< -o $@

$(BUILD_DIR)/%.o: %.S | create-dir
	@echo "Assembling preprocessed ASM: $< -> $@"
	$(CC) $(CFLAGS) $(INCLUDES) -x assembler-with-cpp $< -o $@

# Create directory structure in build/ and bin/
create-dir:
	@echo "Creating build and bin directories..."
	@mkdir -p $(addprefix $(BUILD_DIR)/,$(SRC_DIRS))
	@mkdir -p $(BIN_DIR)

# Run in QEMU
run: $(OUTPUT)
	@echo "Running in QEMU..."
	# $(QEMU) -machine virt -cpu cortex-a57 -kernel $(OUTPUT)
	$(QEMU) -machine virt -cpu cortex-a57 -kernel $(OUTPUT) -nographic

# Clean build
clean:
	@echo "Cleaning build and bin directories..."
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all run clean build create-dir
