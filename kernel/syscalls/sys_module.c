/*
 * Module-related system call implementations.
 */

#include <hubble/errno.h>
#include <hubble/module.h>
#include <hubble/printk.h>

long sys_module_load(const char *path) {
  if (!path)
    return -EINVAL;
  return module_load(path);
}

long sys_module_unload(const char *name) {
  if (!name)
    return -EINVAL;
  return module_unload(name);
}
