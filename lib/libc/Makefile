# libs/libc/Makefile

LIBC_DIR := src
LIBC_OUT := $(OUT_DIR)/lib/libc.a

INCLUDES += -Iinclude

.PHONY: all
all: $(LIBC_OUT)

$(LIBC_OUT): $(BUILD_TOOL)
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
