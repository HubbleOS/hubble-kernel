// user_main.c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libc.h"
#include <stdio.h>

#include <sys/syscall.h>

void *mmap_(uint64_t addr, size_t length, int prot, int flags, int fd,
            uint64_t offset) {
  return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

int read_file(int fd, void *buf, size_t size) {
  return syscall3(2, fd, (long)buf, size);
}

int write_file(int fd, void *buf, size_t size) {
  return syscall3(1, fd, (long)buf, size);
}

int lseek(int fd, uint64_t offset, int whence) {
  return syscall3(8, fd, offset, whence);
}

int spawn(void *entry_point, void *arg, uint32_t priority) {
  return syscall3(6, (long)entry_point, (long)arg, priority);
}

int module_load(const char *path) { return syscall1(9, (long)path); }
int module_unload(const char *name) { return syscall1(10, (long)name); }

typedef struct {
  int32_t x, y;
  bool left, right, middle; // current held state
  bool left_clicked;        // set on press, you clear it after handling
  bool right_clicked;
} mouse_t;

uint32_t open(const char *path, int flags) {
  return syscall2(4, (long)path, flags);
}

void handle_command(char *cmd);

void terminal(void) {
  char buf[256];
  int tty_fd = open("/dev/tty0", 0);

  printf("> ");
  while (1) {
    int n = read_file(tty_fd, buf, sizeof(buf) - 1);
    if (n <= 0)
      continue;
    buf[n] = '\0';

    handle_command(buf);
    printf("> ");
  }
}

void handle_command(char *cmd) {
  int len = strlen(cmd);
  while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r')) {
    cmd[--len] = '\0';
  }

  if (strcmp(cmd, "help") == 0) {
    printf("Available commands: help, echo, clear, hello, lsmod, modprobe, "
           "rmmod, net\n");
  } else if (strncmp(cmd, "echo ", 5) == 0) {
    printf("%s\n", cmd + 5);
  } else if (strcmp(cmd, "hello") == 0) {
    printf("Loading hello.ko...\n");
    int ret = module_load("/modules/hello.ko");
    if (ret == 0) {
      printf("Module hello.ko loaded successfully!\n");
    } else {
      printf("Failed to load module: %d\n", ret);
    }
  } else if (strcmp(cmd, "net") == 0) {
    printf("Loading net.ko...\n");
    int ret = module_load("/modules/net.ko");
    if (ret == 0) {
      printf("Module net.ko loaded successfully!\n");
    } else {
      printf("Failed to load module: %d\n", ret);
    }
  } else if (strcmp(cmd, "lsmod") == 0) {
    int fd = open("/proc/modules", 0);
    if (fd >= 0) {
      char buf[256];
      int n;
      while ((n = read_file(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
      }
    } else {
      printf("No /proc/modules available\n");
    }
  } else if (strncmp(cmd, "modprobe ", 9) == 0) {
    char *modname = cmd + 9;
    /* Skip leading spaces */
    while (*modname == ' ')
      modname++;
    if (*modname == '\0') {
      printf("usage: modprobe <module>\n");
    } else {
      char path[256] = "/modules/";
      strcat(path, modname);
      strcat(path, ".ko");
      printf("Loading %s...\n", path);
      int ret = module_load(path);
      if (ret == 0)
        printf("Loaded %s\n", path);
      else
        printf("Failed to load %s: %d\n", path, ret);
    }
  } else if (strncmp(cmd, "rmmod ", 6) == 0) {
    char *modname = cmd + 6;
    while (*modname == ' ')
      modname++;
    if (*modname == '\0') {
      printf("usage: rmmod <module>\n");
    } else {
      printf("Unloading %s...\n", modname);
      int ret = module_unload(modname);
      if (ret == 0)
        printf("Unloaded %s\n", modname);
      else
        printf("Failed to unload %s: %d\n", modname, ret);
    }
  } else {
    printf("Unknown command: %s\n", cmd);
  }
}

void load_essential_modules(void) {
  const char *modules[] = {
      "/modules/input.ko",
      "/modules/tty.ko",
  };
  for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
    int ret = module_load(modules[i]);
    if (ret == 0)
      printf("Auto-loaded %s\n", modules[i]);
    else
      printf("Note: %s (%d)\n", modules[i], ret);
  }
}

void _start(void) {
  libc_init();
  printf("Hello from user space 2!\n");
  load_essential_modules();
  int pid2 = spawn(terminal, NULL, 0);
  while (1) {
    ;
  }
}
