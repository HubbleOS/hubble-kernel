# usr/lib/libgui/Makefile

GUI_SRC := .
GUI_OUT := $(USR_DIR)/lib/libgui.a
INCLUDES := -Iinclude -Isrc -I../libc/include

# Collect all source files for incremental build
GUI_SRCS := $(shell find $(GUI_SRC) -name '*.c' -o -name '*.cpp' 2>/dev/null)
GUI_HDRS := $(shell find include -name '*.h' 2>/dev/null)

.PHONY: all
all: $(GUI_OUT)

$(GUI_OUT): $(BUILD_TOOL) $(GUI_SRCS) $(GUI_HDRS)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(GUI_SRC) \
		--build-dir $(OUT_DIR)/build/lib/libgui \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $@ \
		--type archive

clean:
	@rm -f $(GUI_OUT)
	@rm -rf $(OUT_DIR)/build/lib/libgui
