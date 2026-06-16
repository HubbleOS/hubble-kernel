# usr/apps/Makefile

SRC_DIR := $(abspath src)
USR_BUILD_DIR := $(OUT_DIR)/build/apps
OUTPUT := $(OUT_DIR)/usr/user.elf
LDSCRIPT := $(SRC_DIR)/user.ld

# CFLAGS for userland
# CFLAGS := -ffreestanding -O2 -Wall -Wextra -m64 -g

# Shared include
INCLUDE_DIR := ../include
INCLUDES := -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/libc -I../lib/libc/include -I../lib/libgui/src -I../lib/libgui/include

# Linker flags
LIBC := ../lib/libc.a
LIBGUI := ../lib/libgui.a
LDFLAGS := -T$(LDSCRIPT) -Ttext-segment=0x400000

.PHONY: all clean $(OUTPUT)

# All
all: $(OUTPUT)

# Build user app
$(OUTPUT): $(BUILD_TOOL) $(LIBC) $(LIBGUI)
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
		--libs "$(LIBGUI) $(LIBC)"

# Clean
clean:
	@echo "Cleaning build and symlinks..."
	@rm -rf $(USR_BUILD_DIR)
	@rm -f $(INCLUDE_DIR)/libgui
	@rm -f $(INCLUDE_DIR)/libc
