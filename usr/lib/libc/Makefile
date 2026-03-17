# usr/lib/libc/Makefile
LIBC_SRC := src
LIBC_OUT := $(USR_DIR)/lib/libc.a
INCLUDES := -Iinclude

.PHONY: all
all: $(LIBC_OUT)

$(LIBC_OUT): $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(LIBC_SRC) \
		--build-dir $(OUT_DIR)/build/lib/libc \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $@ \
		--type archive

	# копируем заголовки libc
	@mkdir -p $(USR_DIR)/include/libc
	@cp include/*.h $(USR_DIR)/include/libc/
