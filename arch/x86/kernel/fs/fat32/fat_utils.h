#pragma once
#include <stdbool.h>
#include "fat_structs.h"
#include <fs/gpt/gpt.h>
typedef void (*directory_entry_callback_t)(const char *name, bool is_dir, Directory *context);

void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr);
uint32_t cluster_to_lba(FAT32_FS *fs, uint32_t cluster);
void fat32_read_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer);
void fat32_write_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer);
void ata_write_cluster(uint32_t cluster, const uint8_t *data);
uint32_t get_next_cluster(FAT32_FS *fs, uint32_t cluster);
void set_next_cluster(FAT32_FS *fs, uint32_t cluster, uint32_t value);
bool fat_flush(FAT32_FS *fs);
void fat_cleanup(FAT32_FS *fs);
void format_filename_fat(const char *in, char *out11);
PathParts format_folder_path(const char *in);
void iterate_directory(FAT32_FS *fs, uint32_t cluster, directory_entry_callback_t callback, void *ctx);
uint32_t resolve_path_to_cluster(FAT32_FS *fs, const char *path);
uint32_t find_directory_entry_cluster(FAT32_FS *fs, uint32_t dir_cluster, const char *name11);
uint32_t get_fat_entry(FAT32_FS *fs, uint32_t cluster);
bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out, bool *is_dir_out);
uint32_t fat32_allocate_cluster(FAT32_FS *fs);
void fat32_free_cluster(FAT32_FS *fs, uint32_t cluster);
void set_fat_entry(FAT32_FS *fs, uint32_t cluster, uint32_t value);
bool fat32_add_directory_entry(FAT32_FS *fs, uint32_t dir_cluster, FAT32_DirectoryEntry *new_entry);
void free_folder_path(PathParts *pp);
bool is_empty_dir(FAT32_DirectoryEntry *entry);
int fat32_create_entry(FAT32_FS *fs, uint32_t cluster, PathPart *pp, bool is_dir);
int fat32_delete_entry(FAT32_FS *fs, uint32_t cluster, const char *name);
void fat32_format_directory_cluster(FAT32_FS *fs, uint32_t cluster, uint32_t parent_cluster);
Directory Directory_init(Directory dir);
