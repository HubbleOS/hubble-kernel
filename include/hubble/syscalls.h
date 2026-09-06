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
uint64_t sys_brk(uint64_t new_addr);
long sys_stat(const char *path, struct stat *st);
long sys_writev(int fd, struct iovec *iov, int iovcnt);
long sys_mprotect(void *addr, size_t len, int prot);
long arch_prctl(long a1, long a2, long a3, long a4, long a5, long a6);
long sys_munmap(uint64_t addr, size_t length);

typedef struct registers registers_t;
long sys_fork(registers_t *regs);
long sys_execve(registers_t *regs);
long sys_wait4(int pid, int *status, int options, void *rusage);
