#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

#define SYS_module_load 9
#define SYS_module_unload 10

static long sys_module_load(const char *path) {
  return syscall1(SYS_module_load, (long)path);
}

static long sys_module_unload(const char *name) {
  return syscall1(SYS_module_unload, (long)name);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: modprobe <module_name> [args]\n");
    printf("       modprobe -r <module_name>\n");
    return 1;
  }

  int unload = 0;
  const char *modname = NULL;

  if (strcmp(argv[1], "-r") == 0 && argc >= 3) {
    unload = 1;
    modname = argv[2];
  } else {
    modname = argv[1];
  }

  if (unload) {
    long ret = sys_module_unload(modname);
    if (ret == 0)
      printf("modprobe: unloaded %s\n", modname);
    else
      printf("modprobe: failed to unload %s: %ld\n", modname, ret);
    return (int)ret;
  }

  char path[256];
  path[0] = '\0';
  strcat(path, "/modules/");
  strcat(path, modname);
  strcat(path, ".ko");

  long ret = sys_module_load(path);
  if (ret == 0)
    printf("modprobe: loaded %s\n", path);
  else
    printf("modprobe: failed to load %s: %ld\n", path, ret);

  return (int)ret;
}

void _start(void) {
  libc_init();
  int ret = main(__argc, __argv);
  exit(ret);
}
