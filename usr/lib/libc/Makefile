# usr/lib/libc/Makefile

LIBC_SRC := src
LIBC_OUT := $(USR_DIR)/lib/libc.a
INCLUDES := -Iinclude -I../../include

# Collect all source files for incremental build
LIBC_SRCS := $(shell find $(LIBC_SRC) -name '*.c' -o -name '*.cpp' 2>/dev/null)
LIBC_HDRS := $(shell find include -name '*.h' 2>/dev/null)

.PHONY: all
all: $(LIBC_OUT)

$(LIBC_OUT): $(BUILD_TOOL) $(LIBC_SRCS) $(LIBC_HDRS)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(LIBC_SRC) \
		--build-dir $(OUT_DIR)/build/lib/libc \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $@ \
		--type archive

clean:
	@rm -f $(LIBC_OUT)
	@rm -rf $(OUT_DIR)/build/lib/libc
