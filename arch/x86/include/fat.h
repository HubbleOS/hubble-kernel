#pragma once
#include <stdint.h>
#include <stddef.h>
#include "utils/framebuffer.h"
#include "utils/gpt/gpt_struct.h"
#include <stdbool.h>
int fat32_init(void *ramdisk_base);
int fat32_read_file(const char *path, void *out_buf, size_t *out_size);
int fat32_write_file(const char *filename, const void *data, size_t size);
int fat32_delete_file(const char *filename);
int fat32_rename_file(const char *oldname, const char *newname);
void fat32_list_files(uint32_t cluster, char *out_buf);
void fat32_list_files_from_path(const char *path, char *out_buf);
bool fat32_create_directory(const char *path, const char *dirname11);
int fat32_delete_dir(const char *path);
int fat32_init_from_lba(gpt_partition_t part);
void debug_fat32(framebuffer_info_t *fb);
extern uint32_t fat_start_lba, cluster_heap_lba, root_cluster;
