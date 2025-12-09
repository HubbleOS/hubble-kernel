# libs/libc/Makefile

LIBC_DIR := $(abspath src)
LIBC_OUT := $(BUILD_DIR)/libc.a

BUILD_TOOL ?= $(abspath ../../tools/dev/build/build/build_main)
LOG_DIR ?= $(OUT_DIR)/logs

INCLUDES ?= -Iinclude
CFLAGS_EXTRA := -mcmodel=kernel -ffreestanding

.PHONY: all
all: $(LIBC_OUT)

$(LIBC_OUT): $(BUILD_TOOL)
	@echo "📦 Building libc with BUILD_TOOL..."
	$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(LIBC_DIR) \
		--build-dir $(BUILD_DIR)/libc \
		--cc $(CC) \
		--cflags "$(CFLAGS) $(CFLAGS_EXTRA)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $@ \
		--type archive \
		-v
