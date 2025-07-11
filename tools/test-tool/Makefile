CC := gcc
CCFLAGS := 
INCLUDES := -Iinclude -Isrc

LDFLAGS := -lncurses

BUILD_DIR := build

SRC_DIR := src
TARGET := $(BUILD_DIR)/main

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

TEST_DIR := test
TEST_TARGET := $(BUILD_DIR)/testProgram

TEST_SRCS := $(shell find $(TEST_DIR) -name "*.c")
TEST_OBJS := $(filter-out $(BUILD_DIR)/src/main.o, $(OBJS) $(TEST_SRCS:%.c=$(BUILD_DIR)/%.o))

.PHONY: all build run clean test init

all: build run

init:
	@mkdir -p $(SRC_DIR) $(TEST_DIR) $(BUILD_DIR) include
	@test -f $(SRC_DIR)/main.c || echo '#include <stdio.h>\n\nint main() {\n  printf("Hello, World!\\n");\n  return 0;\n}' > $(SRC_DIR)/main.c
	@test -f include/utils.h || echo '// Example header\n#pragma once\n\nvoid hello();' > include/utils.h
	@test -f $(TEST_DIR)/app.c || echo '#include <stdio.h>\n\nint main() {\n  printf("Running tests...\\n");\n  return 0;\n}' > $(TEST_DIR)/app.c
	@echo "Project structure initialized."

build: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_TARGET)
	@$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(TEST_OBJS) $(LDFLAGS) -o $(TEST_TARGET)

run: build
	@$(TARGET)

clean:
	@rm -rf $(BUILD_DIR)

