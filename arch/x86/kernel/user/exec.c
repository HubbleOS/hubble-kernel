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

	uint64_t stack_base = task1->kernel_stack;
	for (uint64_t i = 0; i < task1->stack_size + PAGE_SIZE; i += PAGE_SIZE)
	{
		uint64_t phys = pmm_alloc_page();
		if (!phys)
			break;
		vmm_map_page_into(pml4, stack_base + i, phys,
				  PTE_PRESENT | PTE_USER | PTE_WRITE);
	}

	stack_base = task1->rsp0;
	for (uint64_t i = 0; i < task1->rsp0_size + PAGE_SIZE; i += PAGE_SIZE)
	{
		uint64_t phys = pmm_alloc_page();
		if (!phys)
		{
			printk(KERN_ERR "failed to alloc page\n");
			break;
		}
		vmm_map_page_into(task1->page_table, stack_base + i, phys,
				  PTE_PRESENT | PTE_USER | PTE_WRITE);
	}

	task1->page_table = pml4;
	return task1;
}
