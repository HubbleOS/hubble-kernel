ARCH     ?= x86_64
CROSS    ?= x86_64-elf-
CC       := $(CROSS)gcc
LD       := $(CROSS)ld
OBJCOPY  := $(CROSS)objcopy

BUILD_DIR := ../build
OUT_DIR   := ../bin

SRC := user_main.c
OBJ := $(BUILD_DIR)/user_main.o
OUT := $(OUT_DIR)/user.elf
LDSCRIPT := user.ld

CFLAGS   := -ffreestanding -O2 -m64 -nostdlib -fno-pic -Wall -Wextra -nostartfiles -static -fno-pie -mno-red-zone -mcmodel=large
# CFLAGS := -ffreestanding -O2 -m64 -mno-red-zone \
#           -Wall -Wextra -static \
#           -fno-pic -fno-pie
# CFLAGS += -nostdlib -nostartfiles

INCLUDES ?= -I../include
INCLUDES += -I../../lib/libc/include
INCLUDES += -I../../arch/$(ARCH)/include
LIBC := ../../out/lib/libc.a
LDFLAGS  := -T $(LDSCRIPT) -Ttext-segment=0x400000

all: $(OUT)

$(OBJ): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OUT): $(OBJ) $(LDSCRIPT)
	@mkdir -p $(OUT_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(LIBC)

clean:
	rm -rf $(OUT_DIR) $(BUILD_DIR)
