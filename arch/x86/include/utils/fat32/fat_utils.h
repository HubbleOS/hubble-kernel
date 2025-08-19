#pragma once
#include <stdbool.h>
#include "utils/fat32/fat_structs.h"
#include "utils/gpt/gpt.h"

typedef void (*directory_entry_callback_t)(const char *name, bool is_dir, Directory *context);

void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr);
uint32_t cluster_to_lba(uint32_t cluster);
void fat32_read_cluster(uint32_t cluster, uint8_t *buffer);
void fat32_write_cluster(uint32_t cluster, uint8_t *buffer);
void ata_write_cluster(uint32_t cluster, const uint8_t *data);
uint32_t get_next_cluster(uint32_t cluster);
void set_next_cluster(uint32_t cluster, uint32_t value);
void fat_flush();
void fat_cleanup();
void format_filename_fat(const char *in, char *out11);
PathParts format_folder_path(const char *in);
void iterate_directory(uint32_t cluster, directory_entry_callback_t callback, void *ctx);
uint32_t resolve_path_to_cluster(const char *path);
uint32_t find_directory_entry_cluster(uint32_t dir_cluster, const char *name11);
uint32_t get_fat_entry(uint32_t cluster);
bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out, bool *is_dir_out);
uint32_t fat32_allocate_cluster();
void fat32_free_cluster(uint32_t cluster);
void set_fat_entry(uint32_t cluster, uint32_t value);
uint32_t get_fat_entry(uint32_t cluster);
bool fat32_add_directory_entry(uint32_t dir_cluster, FAT32_DirectoryEntry *new_entry);
void free_folder_path(PathParts *pp);
bool is_empty_dir(FAT32_DirectoryEntry *entry);
bool fat32_create_entry(uint32_t cluster, PathPart *pp, bool is_dir);
void fat32_format_directory_cluster(uint32_t cluster, uint32_t parent_cluster);