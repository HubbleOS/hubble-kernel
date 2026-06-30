/* ── FAT32 public API ─────────────────────────────────────────────
 * High-level interface for FAT32 filesystem operations including
 * file/directory creation, deletion, reading, writing, and mount.
 * ────────────────────────────────────────────────────────────────── */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <fs/gpt/gpt_struct.h>
#include "fat_structs.h"
#include <fs/vfs/vfs.h>

/** @brief Initialise a FAT32 filesystem from a ramdisk base address. */
int fat32_init(void *ramdisk_base);

/** @brief Read a file's contents into a buffer. */
size_t fat32_read_file(FAT32_FS *fs, const char *path, const char *filename11, uint8_t *out_buf, size_t max_size);

/** @brief Create a new file. */
bool fat32_create_file(FAT32_FS *fs, const char *path);

/** @brief Delete a file. */
bool fat32_delete_file(FAT32_FS *fs, const char *path);

/** @brief Rename a file. */
int fat32_rename_file(const char *oldname, const char *newname);

/** @brief Write data to a file. */
bool fat32_write_file(FAT32_FS *fs, const char *path, const char *filename11, const uint8_t *data, size_t size);

/** @brief List files in a directory given a cluster number. */
Directory fat32_list_files(FAT32_FS *fs, uint32_t cluster);

/** @brief List files from a path. */
Directory fat32_list_files_from_path(FAT32_FS *fs, const char *path);

/** @brief Create a directory. */
bool fat32_create_directory(FAT32_FS *fs, const char *path);

/** @brief Delete a directory. */
int fat32_delete_dir(FAT32_FS *fs, const char *path);

/** @brief Initialise FAT32 from a given LBA. */
int fat32_init_from_lba(uint32_t first_lba, FAT32_FS *fs);

/** @brief Delete a directory (wrapper). */
bool fat32_delete_directory(FAT32_FS *fs, const char *path);

/** @brief Initialise FAT32 BPB from a GPT partition. */
FAT32_BPB fat_init(gpt_partition_t part);

/** @brief Resolve a path to a cluster number. */
uint32_t fat32_resolve_path(FAT32_FS *fs, const char *path);

/** @brief Mount a FAT32 filesystem. */
bool fat32_mount(FAT32_FS *fs, VFS_Device *device, uint32_t start_lba);

/** @brief Unmount a FAT32 filesystem. */
bool fat32_unmount(FAT32_FS *fs);

/** @brief Open a file by path. */
FAT32_File *fat32_open(FAT32_FS *fs, const char *path);

/** @brief Read from a FAT32 file. */
int fat32_read(VFS_File *node, uint8_t *buffer, uint32_t size);

/** @brief Write to a FAT32 file. */
int fat32_write(VFS_File *node, const uint8_t *buffer, uint32_t size);

/** @brief Create a directory (VFS wrapper). */
int fat32_mkdir(FAT32_FS *fs, const char *path);

/** @brief Delete a file/directory (VFS wrapper). */
int fat32_delete(FAT32_FS *fs, const char *path);

/** @brief Update the FAT entry for a file. */
int fat32_update_fat_entry(FAT32_FS *fs, FAT32_File *file);
