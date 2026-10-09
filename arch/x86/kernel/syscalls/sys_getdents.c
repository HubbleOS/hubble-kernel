/*
 * Syscall: getdents64 - read directory entries from an open directory fd.
 */

#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <fs/vfs/vfs.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

/* Linux struct linux_dirent64; records are padded to 8 bytes. */
struct linux_dirent64 {
  uint64_t d_ino;
  int64_t d_off;
  uint16_t d_reclen;
  uint8_t d_type;
  char d_name[];
} __attribute__((packed));

#define DT_DIR 4
#define DT_REG 8

/**
 * @brief Fill @p dirp with as many entries as fit in @p count bytes.
 *
 * The fd position counts entries already returned. The filesystems only
 * offer a whole-directory listing, so it is re-read on every call and
 * the first `pos` entries skipped.
 *
 * @return Bytes written, 0 at end of directory, or negative errno.
 */
long sys_getdents64(int fd, void *dirp, size_t count) {
  file_t *open_file = fd_file(get_current_task(), fd);
  if (!open_file)
    return -EBADF;
  if (!open_file->vfs)
    return -ENOTDIR;

  VFS_File *file = open_file->vfs;
  if (!file->node->is_dir)
    return -ENOTDIR;

  Directory dir = vfs_readdir(file->path);

  uint8_t *out = dirp;
  size_t used = 0;
  uint32_t index = file->pos;

  for (; index < (uint32_t)dir.count; index++) {
    Entry *e = &dir.entries[index];
    if (!e->name)
      continue;

    size_t name_len = strlen(e->name);
    size_t reclen =
        (sizeof(struct linux_dirent64) + name_len + 1 + 7) & ~(size_t)7;
    if (used + reclen > count)
      break;

    struct linux_dirent64 *d = (struct linux_dirent64 *)(out + used);
    /* No stable inode numbers everywhere; never report 0 (some libcs
     * treat d_ino == 0 as a deleted entry). */
    d->d_ino = e->cluster ? e->cluster : index + 1;
    d->d_off = index + 1;
    d->d_reclen = (uint16_t)reclen;
    d->d_type = e->is_dir ? DT_DIR : DT_REG;
    memcpy(d->d_name, e->name, name_len + 1);
    used += reclen;
  }

  bool buffer_too_small = used == 0 && index < (uint32_t)dir.count;
  file->pos = index;
  if (dir.free_entries)
    dir.free_entries(&dir);

  return buffer_too_small ? -EINVAL : (long)used;
}
