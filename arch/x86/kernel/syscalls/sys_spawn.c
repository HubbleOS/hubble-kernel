#include <smp/scheduler.h>
#include <sys/syscall.h>
#include <smp/task.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include "higher_half.h"
#include <hubble/string.h>
#include <hubble/printk.h>
#include <user/elf.h>

#include "syscall_entry.h"

long sys_spawn(void *entry_point, void *arg, uint32_t priority)
{
	if (!entry_point)
		return -1;

	task_t *task = task_create((void *)entry_point, priority, 1);

	printk("spawn task %p\n", task);

	if (!task)
		return -1;

	scheduler_add_task(task);
	return (long)task->pid;
}

long sys_spawn_file(const char *path, void *arg, uint32_t priority)
{
	// todo relative elf_load so no use for that only template
	uint64_t entry_point = 0;

	elf_load(path, &entry_point);

	return sys_spawn((void *)entry_point, arg, priority);
}
