ARCH     ?= x86_64
CROSS    ?= x86_64-elf-
CC       := $(CROSS)gcc
LD       := $(CROSS)ld
OBJCOPY  := $(CROSS)objcopy

BUILD_DIR := ../../build/usr
OUT_DIR   := ../bin

SRC := user_main.c
OBJ := $(BUILD_DIR)/user_main.o
OUT := $(OUT_DIR)/user.elf
LDSCRIPT := user.ld

CFLAGS   := -ffreestanding -O2 -m64 -nostdlib -fno-pic -Wall -Wextra -nostartfiles -I../include
LDFLAGS  := -T $(LDSCRIPT)

all: $(OUT)

$(OBJ): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJ) $(LDSCRIPT)
	@mkdir -p $(OUT_DIR)
	$(LD) $(LDFLAGS) -o $@ $<

clean:
	rm -rf $(OUT_DIR) $(BUILD_DIR)
