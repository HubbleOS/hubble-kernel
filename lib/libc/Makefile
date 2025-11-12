# libs/libc/Makefile
LIBC_DIR := $(abspath src)
LIBC_SRC := $(shell find $(LIBC_DIR) -type f -name "*.c")
LIBC_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIBC_SRC))
LIBC_OUT := $(BUILD_DIR)/libc.a

INCLUDES ?= -Iinclude

CFLAGS += -mcmodel=kernel
CXXFLAGS += -mcmodel=kernel

all: $(LIBC_OUT)

$(BUILD_DIR)/%.o: %.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) -ffreestanding -c $(CFLAGS) $(INCLUDES) $< -o $@

$(LIBC_OUT): $(LIBC_OBJ)
	@echo "Archiving libc.a..."
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
