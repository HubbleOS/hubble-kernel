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

#define SYS_PATH_MAX 512

/**
 * @brief Absolute, normalised form of a user path
 *
 * Relative paths are resolved against the calling task's cwd; "." and
 * empty components are dropped and ".." removes the previous component
 * (never above "/"). This is lexical, as the VFS has no "." / "..".
 *
 * @return @p buf, or NULL if @p path is NULL or too long
 */
const char *sys_abs_path(const char *path, char *buf, size_t size);

typedef struct {
  uint64_t rsp0;
  uint64_t cpu_id;
} cpu_local_t;

struct iovec {
  void *iov_base;
  size_t iov_len;
};

_End_C_Header;
