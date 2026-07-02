/* -- FAT32 utility functions --------------------------------------
 * Low-level helpers for cluster I/O, FAT table access, directory
 * entry manipulation, and path parsing.
 * ------------------------------------------------------------------ */

#pragma once

#include "fat_structs.h"
#include <fs/gpt/gpt.h>
#include <stdbool.h>

/** @brief Callback type for directory iteration. */
typedef void (*directory_entry_callback_t)(const char *name, bool is_dir,
                                           Directory *context);

/** @brief Callback that populates a Directory listing. */
void list_files_callback(const char *name, bool is_dir, Directory *ctx_ptr);

/** @brief Convert a cluster number to its corresponding LBA. */
uint32_t cluster_to_lba(FAT32_FS *fs, uint32_t cluster);

/** @brief Read a full cluster into a buffer. */
void fat32_read_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer);

/** @brief Write a buffer to a full cluster. */
void fat32_write_cluster(FAT32_FS *fs, uint32_t cluster, uint8_t *buffer);

/** @brief Write data to a cluster via ATA (legacy). */
void ata_write_cluster(uint32_t cluster, const uint8_t *data);

/** @brief Read the next cluster in a chain from the FAT table. */
uint32_t get_next_cluster(FAT32_FS *fs, uint32_t cluster);

/** @brief Set the next cluster pointer in the FAT table. */
void set_next_cluster(FAT32_FS *fs, uint32_t cluster, uint32_t value);

/** @brief Flush dirty FAT cache to disk. */
bool fat_flush(FAT32_FS *fs);

/** @brief Clean up and free FAT cache. */
void fat_cleanup(FAT32_FS *fs);

/** @brief Convert a human-readable filename to 8.3 FAT format. */
void format_filename_fat(const char *in, char *out11);

/** @brief Split a path string into PathParts. */
PathParts format_folder_path(const char *in);

/** @brief Walk a directory cluster and invoke callback for each entry. */
void iterate_directory(FAT32_FS *fs, uint32_t cluster,
                       directory_entry_callback_t callback, void *ctx);

/** @brief Resolve a path to its final cluster. */
uint32_t resolve_path_to_cluster(FAT32_FS *fs, const char *path);

/** @brief Find a directory entry's cluster in a directory chain. */
uint32_t find_directory_entry_cluster(FAT32_FS *fs, uint32_t dir_cluster,
                                      const char *name11);

/** @brief Read a FAT entry value from the cache or disk. */
uint32_t get_fat_entry(FAT32_FS *fs, uint32_t cluster);

/** @brief Parse a raw directory entry into a human-readable name. */
bool parse_directory_entry(FAT32_DirectoryEntry *entry, char *name_out,
                           bool *is_dir_out);

/** @brief Allocate a new cluster in the FAT. */
uint32_t fat32_allocate_cluster(FAT32_FS *fs);

/** @brief Free (zero) a cluster. */
void fat32_free_cluster(FAT32_FS *fs, uint32_t cluster);

/** @brief Write a FAT entry value. */
void set_fat_entry(FAT32_FS *fs, uint32_t cluster, uint32_t value);

/** @brief Add a new directory entry to a directory cluster. */
bool fat32_add_directory_entry(FAT32_FS *fs, uint32_t dir_cluster,
                               FAT32_DirectoryEntry *new_entry);

/** @brief Free memory allocated for a PathParts structure. */
void free_folder_path(PathParts *pp);

/** @brief Check if a directory entry is empty. */
bool is_empty_dir(FAT32_DirectoryEntry *entry);

/** @brief Create a file or directory entry in a given cluster. */
int fat32_create_entry(FAT32_FS *fs, uint32_t cluster, PathPart *pp,
                       bool is_dir);

/** @brief Delete a named entry from a directory cluster. */
int fat32_delete_entry(FAT32_FS *fs, uint32_t cluster, const char *name);

/** @brief Format a newly allocated directory cluster with . and .. entries. */
void fat32_format_directory_cluster(FAT32_FS *fs, uint32_t cluster,
                                    uint32_t parent_cluster);

/** @brief Initialise a Directory structure. */
Directory Directory_init(Directory dir);
