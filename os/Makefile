# # os/Makefile

# INCLUDES := -Iinclude -Isrc
# SRC_DIR := $(abspath src)
# TARGET := $(BUILD_DIR)/main

# SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
# OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# TEST_DIR := $(abspath test)
# TEST_TARGET := $(BUILD_DIR)/testProgram

# TEST_SRCS := $(shell find $(TEST_DIR) -name "*.cpp")
# TEST_OBJS := $(filter-out $(BUILD_DIR)/src/main.o, $(OBJS) $(TEST_SRCS:%.cpp=$(BUILD_DIR)/%.o))

# OS_DIR := $(abspath os)

# .PHONY: all build test init

# all: build 

# # init:
# # 	@mkdir -p $(SRC_DIR) $(TEST_DIR) $(BUILD_DIR) include
# # 	@test -f $(SRC_DIR)/main.cpp || echo '#include <iostream>\n\nint main() {\n    std::cout << "Hello, World!" << std::endl;\n    return 0;\n}' > $(SRC_DIR)/main.cpp
# # 	@test -f include/utils.h || echo '// Example header\n#pragma once\n\nvoid hello();' > include/utils.h
# # 	@test -f $(TEST_DIR)/app.cpp || echo '#include <iostream>\n\nint main() {\n    std::cout << "Running tests..." << std::endl;\n    return 0;\n}' > $(TEST_DIR)/main.cpp
# # 	@echo "Project structure initialized."

# build: $(TARGET)

# $(TARGET): $(OBJS)
# 	@mkdir -p $(BUILD_DIR)
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) $(LDFLAGS) -o $(TARGET)

# $(BUILD_DIR)/%.o: %.cpp
# 	@mkdir -p $(dir $@)
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# test: $(TEST_TARGET)
# 	@$(TEST_TARGET)

# $(TEST_TARGET): $(TEST_OBJS)
# 	@mkdir -p $(BUILD_DIR)
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_OBJS) $(LDFLAGS) -o $(TEST_TARGET)
