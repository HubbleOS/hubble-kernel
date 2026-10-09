/**
 * @file file.h
 * @brief Open file descriptions — what open() and pipe() create, and what
 *        dup() and fork() share
 *
 * A file descriptor is a per-task slot pointing at a file_t. Several
 * descriptors (in one task after dup(), or in parent and child after
 * fork()) can point at the same file_t; they then share its offset and
 * status flags, as on Linux. The file_t goes away when its last
 * descriptor is closed.
 */
#pragma once

#include <smp/atomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vfs.h"

struct wait_queue;
typedef struct file file_t;

/* Linux open(2) status flags (x86_64), kept in file_t.flags. */
#define FILE_O_ACCMODE 0x3
#define FILE_O_RDONLY 0x0
#define FILE_O_WRONLY 0x1
#define FILE_O_RDWR 0x2
#define FILE_O_APPEND 0x400
#define FILE_O_NONBLOCK 0x800

/* poll(2) events */
#define FILE_POLLIN 0x001
#define FILE_POLLOUT 0x004
#define FILE_POLLERR 0x008
#define FILE_POLLHUP 0x010

typedef struct {
  long (*read)(file_t *file, void *buf, size_t len);
  long (*write)(file_t *file, const void *buf, size_t len);
  /** Events possible right now (FILE_POLL*). NULL: always ready. */
  short (*poll)(file_t *file);
  /** Woken when poll()'s answer may have changed. Optional. */
  struct wait_queue *(*wait_queue)(file_t *file);
  /** Last reference gone: free what the file owns. */
  void (*release)(file_t *file);
} file_ops_t;

struct file {
  const file_ops_t *ops;
  atomic_t refs;
  int flags;     /**< FILE_O_* status flags */
  VFS_File *vfs; /**< VFS-backed files only (NULL for pipes) */
  void *priv;    /**< The implementation's own state */
};

/** A new file_t (one reference) for a VFS file it takes over. */
file_t *file_from_vfs(VFS_File *vfs, int flags);

/** A new file_t (one reference) with custom operations. */
file_t *file_alloc(const file_ops_t *ops, int flags, void *priv);

static inline file_t *file_get(file_t *file) {
  atomic_inc(&file->refs);
  return file;
}

/** Drop a reference; the last one releases the file. */
void file_put(file_t *file);

/** The device behind a VFS-backed character device file, or NULL. */
struct VFS_device_reg *file_device(file_t *file);

/** A new pipe: a read end and a write end, one reference each. */
int pipe_create(file_t **read_end, file_t **write_end);
