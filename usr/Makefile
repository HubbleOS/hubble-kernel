BUILD_DIR := build
BIN_DIR   := bin

SRC_DIR := src
USR_BUILD_DIR := $(BUILD_DIR)
OUTPUT := $(BIN_DIR)/user.elf
LDSCRIPT := $(SRC_DIR)/user.ld

CFLAGS   := -ffreestanding -O2 -m64 -nostdlib -fno-pic -Wall -Wextra -nostartfiles -static -fno-pie -mno-red-zone -mcmodel=large

INCLUDES := -Iinclude
INCLUDES += -I../include
INCLUDES += -I../lib/libc/include

LIBC := ../out/lib/libc.a
LDFLAGS  := -T$(LDSCRIPT) -Ttext-segment=0x400000

all: $(OUTPUT)

$(OUTPUT): $(BUILD_TOOL)
	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(SRC_DIR) \
		--build-dir $(USR_BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--output $(OUTPUT) \
		--type exe \
		--ld $(LD) \
		--ldflags "$(LDFLAGS)" \
		--libs "$(LIBC)"

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)
