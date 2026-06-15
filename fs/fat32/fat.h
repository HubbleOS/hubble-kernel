#pragma once
#include <stdint.h>
#include <stddef.h>
#include <fs/gpt/gpt_struct.h>
#include "fat_structs.h"
#include <fs/vfs/vfs.h>
#include <stdbool.h>

int fat32_init(void *ramdisk_base);
size_t fat32_read_file(FAT32_FS *fs, const char *path, const char *filename11, uint8_t *out_buf, size_t max_size);
bool fat32_create_file(FAT32_FS *fs, const char *path);
bool fat32_delete_file(FAT32_FS *fs, const char *path);
int fat32_rename_file(const char *oldname, const char *newname);
bool fat32_write_file(FAT32_FS *fs, const char *path, const char *filename11, const uint8_t *data, size_t size);
Directory fat32_list_files(FAT32_FS *fs, uint32_t cluster);
Directory fat32_list_files_from_path(FAT32_FS *fs, const char *path);
bool fat32_create_directory(FAT32_FS *fs, const char *path);
int fat32_delete_dir(FAT32_FS *fs, const char *path);
int fat32_init_from_lba(uint32_t first_lba, FAT32_FS *fs);
bool fat32_delete_directory(FAT32_FS *fs, const char *path);
FAT32_BPB fat_init(gpt_partition_t part);
uint32_t fat32_resolve_path(FAT32_FS *fs, const char *path);
bool fat32_mount(FAT32_FS *fs, VFS_Device *device, uint32_t start_lba);
bool fat32_unmount(FAT32_FS *fs);
FAT32_File *fat32_open(FAT32_FS *fs, const char *path);
int fat32_read(VFS_File *node, uint8_t *buffer, uint32_t size);
int fat32_write(VFS_File *node, const uint8_t *buffer, uint32_t size);
int fat32_mkdir(FAT32_FS *fs, const char *path);
int fat32_delete(FAT32_FS *fs, const char *path);
int fat32_update_fat_entry(FAT32_FS *fs, FAT32_File *file);
