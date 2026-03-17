# usr/lib/gui/Makefile
GUI_SRC := src
GUI_OUT := $(USR_DIR)/lib/libgui.a
INCLUDES := -Iinclude -I../libc/include

.PHONY: all
all: $(GUI_OUT)

$(GUI_OUT): $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(GUI_SRC) \
		--build-dir $(OUT_DIR)/build/lib/gui \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $@ \
		--type archive

	@mkdir -p $(USR_DIR)/include/gui
	@cp include/*.h $(USR_DIR)/include/gui/
