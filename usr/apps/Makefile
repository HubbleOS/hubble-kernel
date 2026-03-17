# usr/apps/Makefile
SRC_DIR := src
USR_BUILD_DIR := $(OUT_DIR)/build/apps
OUTPUT := $(OUT_DIR)/usr/user.elf
LDSCRIPT := $(SRC_DIR)/user.ld

# CFLAGS для userland
CFLAGS := -ffreestanding -O2 -Wall -Wextra -m64 -mcmodel=large -g

# Include пути: сначала общий include, потом libc
INCLUDES := -I../include -I../include/libc

# Линковка с libc
LIBC := ../lib/libc.a
LDFLAGS := -T$(LDSCRIPT) -Ttext-segment=0x400000

.PHONY: all
all: $(OUTPUT)

$(OUTPUT): $(BUILD_TOOL) $(LIBC)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
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
