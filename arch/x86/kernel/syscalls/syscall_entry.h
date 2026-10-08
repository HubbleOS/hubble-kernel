#pragma once

#include <_cheader.h>

#include <fs/vfs/dev.h>
#include <hubble/errno.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <stddef.h>
#include <stdint.h>

typedef struct registers registers_t;

_Begin_C_Header;

void syscall_init(void);
uint64_t syscall_handler_wrapper(registers_t *regs);

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6);

fd_entry_t *task_get_fd(task_t *task, int fd);

#define SYS_PATH_MAX 512

/**
 * @brief Absolute form of a user path: relative paths are resolved
 *        against "/" (there is no per-process working directory yet).
 * @return @p buf, or NULL if @p path is NULL or too long
 */
const char *sys_abs_path(const char *path, char *buf, size_t size);

struct VFS_device_reg;
/** @brief Character device behind @p fd (0-2: the console tty), or NULL. */
struct VFS_device_reg *fd_get_device(int fd);
fd_entry_t *task_get_free_fd(task_t *task);

typedef struct {
  uint64_t rsp0;
  uint64_t cpu_id;
} cpu_local_t;

struct iovec {
  void *iov_base;
  size_t iov_len;
};

_End_C_Header;
