# usr/apptest/Makefile

SRC_DIR := $(abspath src)
USR_BUILD_DIR := $(OUT_DIR)/build/apptest
OUTPUT := $(OUT_DIR)/usr/user1.elf
LDSCRIPT := $(SRC_DIR)/user.ld

INCLUDE_DIR := ../include
INCLUDES := -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/libc \
	-I../lib/libc/include -I../lib/libgui/src -I../lib/libgui/include

LIBC := ../lib/libc.a
LIBGUI := ../lib/libgui.a
LDFLAGS := -T$(LDSCRIPT) -Ttext-segment=0x400000

# Collect all source files for incremental build
SRCS := $(shell find $(SRC_DIR) -name '*.c' -o -name '*.cpp' 2>/dev/null)

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(BUILD_TOOL) $(LIBC) $(LIBGUI) $(SRCS)
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

clean:
	@echo "Cleaning build and symlinks..."
	@rm -rf $(USR_BUILD_DIR)
	@rm -f $(INCLUDE_DIR)/libgui
	@rm -f $(INCLUDE_DIR)/libc
