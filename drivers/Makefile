DRIVERS_DIR := $(abspath $(ROOT_DIR)/drivers)
DRIVERS_BUILD_DIR := $(BUILD_DIR)/drivers
DRIVERS_LIB := $(DRIVERS_BUILD_DIR)/libdrivers.a
export DRIVERS_DIR DRIVERS_BUILD_DIR DRIVERS_LIB

.PHONY: build

build: $(BUILD_TOOL)
	$(Q)$(BUILD_TOOL) $(BUILD_TOOL_FLAGS) \
		--src-dir $(DRIVERS_DIR) \
		--build-dir $(DRIVERS_BUILD_DIR) \
		--cc $(CC) \
		--cflags "$(CFLAGS)" \
		--includes "$(INCLUDES)" \
		--ar $(AR) \
		--output $(DRIVERS_LIB) \
		--type archive
