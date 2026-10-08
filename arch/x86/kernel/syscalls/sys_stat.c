#include <fs/vfs/vfs.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscall.h>
#include <stddef.h>
#include <stdint.h>

#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

/* x86_64 kernel ABI layout (asm/stat.h), 144 bytes. libc reads fields
 * at these exact offsets: the old layout put st_mode where libc reads
 * st_nlink, so st_mode came back 0 and PATH lookups failed with EACCES. */
struct stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_nlink;
  uint32_t st_mode;
  uint32_t st_uid;
  uint32_t st_gid;
  uint32_t __pad0;
  uint64_t st_rdev;
  int64_t st_size;
  int64_t st_blksize;
  int64_t st_blocks;
  uint64_t st_atime;
  uint64_t st_atime_nsec;
  uint64_t st_mtime;
  uint64_t st_mtime_nsec;
  uint64_t st_ctime;
  uint64_t st_ctime_nsec;
  int64_t __unused[3];
};
_Static_assert(sizeof(struct stat) == 144, "x86_64 struct stat is 144 bytes");

#define S_IFCHR 0020000
#define S_IFDIR 0040000
#define S_IFREG 0100000

/* No permission model yet: everything is owned by root and allowed. */
static void fill_stat(struct stat *st, uint32_t type, uint64_t size) {
  memset(st, 0, sizeof(*st));
  st->st_dev = 1;
  st->st_ino = 1;
  st->st_nlink = 1;
  st->st_mode = type | (type == S_IFCHR ? 0620 : 0755);
  st->st_size = size;
  st->st_blksize = 4096;
  st->st_blocks = (size + 511) / 512;
}

static void fill_stat_file(struct stat *st, VFS_File *f) {
  fill_stat(st, f->node->is_dir ? S_IFDIR : S_IFREG, f->node->size);
}

long sys_stat(const char *path, struct stat *st) {
  char abs[SYS_PATH_MAX];
  if (!sys_abs_path(path, abs, sizeof(abs)))
    return path ? -ENAMETOOLONG : -EFAULT;

  VFS_File *f = vfs_open(abs, VFS_O_RDONLY);
  if (IS_ERR(f))
    return PTR_ERR(f);
  if (!f)
    return -ENOENT;

  fill_stat_file(st, f);
  vfs_close(f);
  return 0;
}

/**
 * @brief lstat: stat without following a final symlink.
 *
 * The VFS always follows symlinks and has no way to report a link
 * itself yet, so this describes the target, like stat. Correct for
 * everything except "is this a symlink" (ls -l shows no "->").
 */
long sys_lstat(const char *path, struct stat *st) {
  return sys_stat(path, st);
}

long sys_fstat(int fd, struct stat *st) {
  /* 0-2 are the console, served by sys_read/sys_write directly. */
  if (fd >= 0 && fd <= 2) {
    fill_stat(st, S_IFCHR, 0);
    return 0;
  }

  fd_entry_t *entry = task_get_fd(get_current_task(), fd);
  if (!entry || !entry->data)
    return -EBADF;
  if (entry->type != FD_FILE) {
    fill_stat(st, S_IFCHR, 0);
    return 0;
  }

  fill_stat_file(st, entry->data);
  return 0;
}
