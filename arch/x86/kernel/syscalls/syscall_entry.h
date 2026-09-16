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
