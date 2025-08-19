#pragma once
#include <stdint.h>
#include <stddef.h>
#include "utils/gpt/gpt_struct.h"
#include "utils/fat32/fat_structs.h"
#include <stdbool.h>

int fat32_init(void *ramdisk_base);
size_t fat32_read_file(const char *path, const char *filename11, uint8_t *out_buf, size_t max_size);
bool fat32_write_file(const char *path, const char *filename11, const uint8_t *data, size_t size);
bool fat32_create_file(const char *path);
bool fat32_delete_file(const char *path, const char *filename11);
int fat32_rename_file(const char *oldname, const char *newname);
bool fat32_write_file(const char *path, const char *filename11, const uint8_t *data, size_t size);
Directory fat32_list_files(uint32_t cluster);
Directory fat32_list_files_from_path(const char *path);
bool fat32_create_directory(const char *path);
int fat32_delete_dir(const char *path);
int fat32_init_from_lba(gpt_partition_t part);
bool fat32_delete_directory(const char *path);
