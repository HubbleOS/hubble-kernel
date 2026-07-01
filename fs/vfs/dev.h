/* -- VFS device file interface ------------------------------------
 * Device file abstraction for the VFS layer. Allows registering
 * hardware-backed devices (MMIO, char devices, etc.) and accessing
 * them through the standard VFS open/read/write interface.
 * ------------------------------------------------------------------ */

#pragma once

#include "vfs.h"
#include "vfs_standart_struct.h"
#include <stdbool.h>

/** @brief VFS device file callbacks for a single file handle. */
typedef struct {
  uint64_t (*mmap)(uint64_t offset, size_t size);
  uint64_t (*read)(uint64_t offset, size_t size);
} VFS_device_file;

/** @brief Registered device descriptor (linked list node). */
typedef struct VFS_device_reg {
  char name[32];
  uint64_t (*mmap)(uint64_t offset, size_t size);
  uint64_t (*read)(uint64_t offset, size_t size, void *buf);
  uint64_t (*write)(uint64_t offset, size_t size, const void *buf);
  struct VFS_device_reg *next;
} VFS_device_reg;

/** @brief Open a device file by path. */
VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path);

/** @brief Create a new device file node. */
VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path);

/** @brief Write to a device file. */
int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size);

/** @brief MMAP a device file. */
uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size);

/** @brief Find a registered device by path. */
VFS_device_reg *dev_vfs_find_device(VFS_FS *fs, const char *path);

/** @brief Register a new device with the VFS layer. */
void dev_vfs_register(const char *name, uint64_t (*mmap)(uint64_t, size_t),
                      uint64_t (*read)(uint64_t, size_t, void *),
                      uint64_t (*write)(uint64_t, size_t, const void *));
