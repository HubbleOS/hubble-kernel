# usr/lib/libgui/Makefile

GUI_SRC := .
GUI_OUT := $(USR_DIR)/lib/libgui.a
INCLUDES := -Iinclude -Isrc -I../libc/include

.PHONY: all
all: $(GUI_OUT)

$(GUI_OUT): $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(GUI_SRC) \
		--build-dir $(OUT_DIR)/build/lib/libgui \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $@ \
		--type archive
