#include <smp/task.h>
#include <smp/scheduler.h>

#include <user/elf.h>

#include <mm/vmm.h>
#include <mm/pmm.h>

#include <hubble/printk.h>

task_t *exec(const char *path)
{

	uint64_t *pml4 = vmm_create_user_pagemap();

	uint64_t entry;
	elf_load(path, &entry, (uint64_t *)pml4);

	task_t *task1 = task_create((void *)entry, 255, 1);

	task_map_user_stack(task1, (uint64_t *)pml4);

	task1->page_table = pml4;
	return task1;
}
