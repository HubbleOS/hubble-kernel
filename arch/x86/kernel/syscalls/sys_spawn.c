#include <smp/scheduler.h>
#include <smp/task.h>

#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include "higher_half.h"
#include <hubble/string.h>
#include <hubble/printk.h>
#include <user/elf.h>

#include <user/elf.h>
#include <user/exec.h>

#include <mm/pmm.h>
#include <mm/vmm.h>

#include <asm.h>
#include "syscall_entry.h"
#include <hubble/syscalls.h>

long sys_spawn(void *entry_point, void *arg, uint32_t priority)
{
	if (!entry_point)
		return -1;

	task_t *task = task_create((void *)entry_point, priority, 1);
	if (!task)
		return -1;

	scheduler_add_task(task);
	return (long)task->pid;
}
long sys_spawn_file(const char *path, void *arg, uint32_t priority)
{

	task_t *task = exec(path);
	scheduler_add_task(task);
	return (long)task->pid;
}
