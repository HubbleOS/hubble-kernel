.PHONY: build

build: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(COLOR_DIR) \
		--build-dir $(COLOR_BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $(COLOR_LIB) \
		--type archive
