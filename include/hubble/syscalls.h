#pragma once

/**
 * @brief System call dispatcher and individual handler declarations.
 */

#include <stddef.h>
#include <stdint.h>

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6);

long sys_read(int fd, char *buf, size_t count);
long sys_mmap(uint64_t addr, size_t length, int prot, int flags, int fd,
              uint64_t offset);
long sys_write(int fd, const char *buf, size_t count);
long sys_open(const char *path, int flags);
long sys_lseek(int fd, uint64_t offset, int whence);
long sys_close(int fd);
long sys_spawn(void *entry_point, void *arg, uint32_t priority);
long sys_spawn_file(const char *path, void *arg, uint32_t priority);
long sys_module_load(const char *path);
long sys_module_unload(const char *name);
