/* -- VFS standard type definitions --------------------------------
 * Common types, enumerations, and helper macros shared across all
 * VFS implementations (FAT32, EXT2, device, pipe).
 * ------------------------------------------------------------------ */

#pragma once

#include <mm/kmalloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Forward declaration of Directory. */
typedef struct Directory Directory;

/** @brief Directory entry for readdir results. */
typedef struct {
  uint32_t cluster;
  char *name;
  bool is_dir;
} Entry;

/** @brief Directory listing structure. */
struct Directory {
  Entry *entries;
  int count;
  bool (*free_entries)(Directory *ctx);
};

/** @brief File open flags. */
typedef enum {
  OPEN_READ = 'r',
  OPEN_WRITE = 'w',
  OPEN_BYTE = 'b',
  OPEN_APPEND = 'a',
  OPEN_CREATE = '+'
} OpenFlags;

/** @brief Filesystem type identifiers. */
typedef enum { FS_NONE, FS_FAT32, FS_EXT2, FS_DEV, FS_PIPE } FileSystemType;

/** @brief Abstract block device descriptor. */
typedef struct {
  void *device;
  int (*read)(void *device, uint32_t lba, void *buffer);
  int (*write)(void *device, uint32_t lba, const void *buffer);
} VFS_Device;

#define ERR_PTR(x) ((void *)(intptr_t)(x))
#define PTR_ERR(p) ((int)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)

/* -- Open modes ---------------------------------------------------- */
#define VFS_O_RDONLY 0x01
#define VFS_O_WRONLY 0x02
#define VFS_O_RDWR 0x03

#define VFS_O_CREAT 0x10
#define VFS_O_EXCL 0x20
#define VFS_O_TRUNC 0x40
#define VFS_O_APPEND 0x80

/* -- Standard file modes -------------------------------------------- */
#define MODE_FILE 0x1000
#define MODE_DIR 0x2000
#define MODE_CHAR 0x3000
#define MODE_BLOCK 0x4000
#define MODE_PIPE 0x5000
#define MODE_SYMLINK 0x6000

/* -- Access modes --------------------------------------------------- */
#define MODE_READ 0x0004
#define MODE_WRITE 0x0002
#define MODE_EXEC 0x0001

/* -- Seek modes ----------------------------------------------------- */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/** @brief Free all entry names in a Directory structure. */
static bool free_entries(Directory *dir) {
  if (!dir || !dir->entries)
    return false;

  for (int i = 0; i < dir->count; i++)
    kfree(dir->entries[i].name);

  kfree(dir->entries);
  dir->entries = NULL;
  dir->count = 0;

  return true;
}

/** @brief Initialise a Directory structure with default values. */
static Directory Directory_init(Directory dir) {
  dir.free_entries = free_entries;
  dir.count = 0;
  return dir;
}
