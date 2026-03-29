#pragma once

#include <_cheader.h>

#include <stddef.h>
#include <stdint.h>
#include <fs/vfs/dev.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <hubble/errno.h>

#include <interrupt/interrupt.h> // for registers_t

_Begin_C_Header;

void syscall_init(void);

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
			 uint64_t a4, uint64_t a5, uint64_t a6);
uint64_t syscall_handler_wrapper(registers_t *regs);

VFS_File *task_get_fd(task_t *task, int fd);

long sys_read(int, char *, size_t);
long sys_write(int, const char *, size_t);
long sys_fb(void);
long sys_mmap(uint64_t addr, size_t length, int prot, int flags,
	      int fd, uint64_t offset);

long sys_open(const char *path, int flags);
long sys_close(int fd);
long sys_spawn(void *entry_point, void *arg, uint32_t priority);

_End_C_Header;
