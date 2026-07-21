#include <fs/vfs/vfs.h>
#include <hubble/string.h>
#include <hubble/syscall.h>
#include <stddef.h>
#include <stdint.h>

struct stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint32_t st_mode;
  uint32_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;
  uint64_t st_rdev;
  uint64_t st_size;
};
#define S_IFREG 0100000

long sys_stat(const char *path, struct stat *st) {
  VFS_File *f = vfs_open(path, VFS_O_RDONLY);
  if (IS_ERR(f) || !f)
    return -2;

  memset(st, 0, sizeof(*st));
  st->st_size = 1;
  st->st_dev = 1;
  st->st_rdev = 1;
  st->st_ino = 1;
  st->st_nlink = 1;
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_mode = S_IFREG | 0777;

  vfs_close(f);
  return 0;
}
