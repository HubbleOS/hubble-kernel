#include <fs/vfs/file.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscall.h>
#include <hubble/syscalls.h>
#include <smp/scheduler.h>

#include "syscall_entry.h"

long probe(const char *abs, bool *is_dir);

long sys_getcwd(char *buf, size_t size) {
  //   if (size < 2)
  //     return -ERANGE;

  //   buf[0] = '/';
  //   buf[1] = '\0';

  //   return 2;
  task_t *task = get_current_task();
  if (!task)
    return -EFAULT;
  if (task->fdtable.cwd[0] == '\0') {
    task->fdtable.cwd[0] = '/';
    task->fdtable.cwd[1] = '\0';
  }
  size_t len = strlen(task->fdtable.cwd);
  if (len + 1 > size)
    return -ERANGE;

  memcpy(buf, task->fdtable.cwd, len + 1);
  return len + 1;
}

/* The cwd is kept absolute and normalised, so sys_abs_path can join
 * relative paths onto it as they are. */
long sys_chdir(const char *path) {
  task_t *task = get_current_task();
  char abs[SYS_PATH_MAX];
  if (!sys_abs_path(path, abs, sizeof(abs)))
    return path ? -ENAMETOOLONG : -EFAULT;
  if (strlen(abs) >= sizeof(task->fdtable.cwd))
    return -ENAMETOOLONG;

  bool is_dir = false;
  long err = probe(abs, &is_dir);
  if (err < 0)
    return err;
  if (!is_dir)
    return -ENOTDIR;
  strcpy(task->fdtable.cwd, abs);
  return 0;
}
