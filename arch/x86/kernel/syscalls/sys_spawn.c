#include <smp/scheduler.h>
#include <sys/syscall.h>
#include <smp/task.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include "higher_half.h"
#include <string.h>
#include <printk.h>
#include <user/elf.h>
#include <mm/pmm.h>

#include <asm.h>

#include <mm/vmm.h>

#include "syscall_entry.h"

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
	uint64_t *new_pm = vmm_create_user_pagemap();
	if (!new_pm)
		return -1;

	printk("Spawning at 0x%016lx\n", new_pm);

	uint64_t entry_point = 0;
	if (elf_load(path, &entry_point, new_pm) < 0)
		return -1;

	task_t *task = task_create((void *)entry_point, priority, 1);
	if (!task)
		return -1;

	// Reuse already-allocated physical pages, just remap into new_pm
	uint64_t stack_base = task->kernel_stack;
	for (uint64_t i = 0; i < task->stack_size + PAGE_SIZE; i += PAGE_SIZE)
	{
		// Get phys from kernel CR3 where task_create mapped it
		uint64_t phys = vmm_get_phys(stack_base + i);
		if (!phys)
		{
			printk("Failed to get phys for stack page 0x%016lx\n", stack_base + i);
			continue;
		}

		vmm_map_page_into(new_pm, stack_base + i, phys,
				  PTE_PRESENT | PTE_USER | PTE_WRITE);
	}

	task->page_table = new_pm;
	scheduler_add_task(task);
	return 0;
}
