/* ── Virtual Filesystem (VFS) core interface ──────────────────────
 * Defines the VFS node, file, and filesystem structures along with
 * the public API for mounting, opening, reading, writing, and
 * managing files across different underlying filesystems.
 * ────────────────────────────────────────────────────────────────── */

#pragma once

#include <_cheader.h>

#include <stdint.h>
#include <stdbool.h>

#include "vfs_standart_struct.h"
#include <fs/gpt/gpt.h>

_Begin_C_Header

/** @brief VFS node representing an open file or directory. */
typedef struct VFS_Node
{
	char name[256];
	bool is_dir;
	uint32_t size;
	uint32_t mode;
	uint32_t pos;
	void *fs_node;
	struct VFS_FS *fs;
} VFS_Node;

/** @brief VFS file descriptor (per-open instance). */
typedef struct
{
	uint32_t flags;
	uint32_t pos;
	VFS_Node *node;
} VFS_File;

/** @brief VFS filesystem dispatch table. */
typedef struct VFS_FS
{
	FileSystemType type;
	void *fs;

	bool (*mount)(struct VFS_FS *fs, VFS_Device *device, uint32_t start_lba);
	void (*unmount)(struct VFS_FS *fs);
	VFS_Node *(*create_file)(struct VFS_FS *fs, const char *path);

	VFS_Node *(*open)(struct VFS_FS *fs, const char *path);
	int (*read)(VFS_File *file, void *buf, uint32_t size);
	int (*write)(VFS_File *file, const void *buf, uint32_t size);
	uint64_t (*mmap)(VFS_File *file, uint64_t offset, size_t size);

	bool (*mkdir)(struct VFS_FS *fs, const char *path);
	bool (*unlink)(struct VFS_FS *fs, const char *path);

	int (*close)(VFS_File *file);
	Directory (*readdir)(struct VFS_FS *fs, const char *path);
} VFS_FS;

/** @brief Mount a filesystem partition at the given mountpoint. */
bool vfs_mount(const char *mountpoint, gpt_partition_t *partition, FileSystemType type);

/** @brief Open a file by path with the given flags. */
VFS_File *vfs_open(const char *path, int flags);

/** @brief Read from an open VFS file. */
int vfs_read(VFS_File *node, void *buf, uint32_t size);

/** @brief Write to an open VFS file. */
int vfs_write(VFS_File *node, const void *buf, uint32_t size);

/** @brief Create a directory. */
bool vfs_mkdir(const char *path);

/** @brief Unlink (delete) a file or directory. */
bool vfs_unlink(const char *path);

/** @brief Read a directory listing. */
Directory vfs_readdir(const char *path);

extern VFS_FS *root_fs;

/** @brief Create a new file. */
VFS_Node *vfs_create_file(const char *path);

/** @brief Seek to a position in an open file. */
int vfs_lseek(VFS_File *node, int offset, int whence);

/** @brief Close an open file. */
int vfs_close(VFS_File *file);

_End_C_Header
