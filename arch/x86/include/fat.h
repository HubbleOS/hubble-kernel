#pragma once
#include <stdint.h>
#include <stddef.h>
#include "utils/framebuffer.h"

int fat32_init(void *ramdisk_base);
int fat32_read_file(const char *filename, void *out_buf, size_t *out_size, framebuffer_info_t *fb);
int fat32_write_file(const char *filename, const void *data, size_t size);
void debug_fat32(framebuffer_info_t *fb);
