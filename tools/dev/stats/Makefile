SRC_DIR := src

CC := gcc
CCFLAGS := 
INCLUDES := -Iinclude -I$(SRC_DIR)

LDFLAGS := 

override BUILD_DIR := $(if $(OUT_DIR),$(OUT_DIR)/tools/dev/stats,build)

TARGET := $(BUILD_DIR)/stats

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all build run clean

all: build run

build: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

run: build
	@$(TARGET)

clean:
	@rm -rf $(BUILD_DIR)
