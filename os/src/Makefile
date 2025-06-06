SRC_FILES := $(shell find . -name "*.cpp" -or -name "*.c")
OBJ_FILES := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SRC_FILES)))
OBJ_FILES += $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SRC_FILES)))

OUTPUT := $(BUILD_DIR)/kernel.elf

all: $(OUTPUT)

$(OUTPUT): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
