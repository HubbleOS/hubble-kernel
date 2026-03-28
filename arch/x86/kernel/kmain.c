#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/smp.h>
#include "init/init.h"
#include <printk.h>
#include "higher_half.h"
#include <dev/mouse.h>
#include <dev/keyboard.h>
#include <dev/ps2.h>
#include "io.h"

#include <fs/vfs/dev.h>

extern int elf_load(const char *path, uint64_t *entry_out, uint64_t *pm);

uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return (uint64_t)g_boot_info->framebuffer->base;
};

__attribute__((section(".text.boot")))
__attribute__((used)) void
kernel_entry(BootInfo *bi)
{
	clear_bss();
	relocate_boot_info(bi);
	g_boot_info = bi;

	early_printk_init(g_boot_info->framebuffer);

	acpi_init(g_boot_info->rsdp);
	hpet_init();
	init_memory(g_boot_info);
	init_cpu();

	init_filesystems();

	dev_vfs_register("fb0", fb_mmap, NULL);

	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file);
	dev_vfs_register("kbd", NULL, kbd_read);

	smp_init();

	scheduler_init();

	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

#include <asm.h>
#include <mm/pmm.h>

void kmain_thread(void)
{
	printk("kmain thread\n");
	// task_t *task1 = task_create(render_task, 255);
	// scheduler_add_task(task1);
	// dump_kernel_pagemap();

	uint64_t *pml4 = vmm_create_user_pagemap();

	// uint64_t cr3 = get_cr3();

	// set_cr3((uint64_t)pml4);

	uint64_t entry;
	elf_load("/usr/bin/user.elf", &entry, (uint64_t *)pml4);

	// set_cr3(cr3);
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
			break;
		vmm_map_page_into(task1->page_table, stack_base + i, phys,
				  PTE_PRESENT | PTE_USER | PTE_WRITE);
	}

	task1->page_table = pml4;
	scheduler_add_task(task1);

	while (1)
	{
		// char c = keyboard_get_char();
		// printk("key: %c\n", c);
		asm volatile("hlt");
	}

	// uint8_t counter = 0;

	// // mouse_init();
	// mouse_t *m = get_mouse_info();
	// uint32_t old_x = m->x;
	// uint32_t old_y = m->y;
	// while (1)
	// {
	// 	if (old_x != m->x || old_y != m->y)
	// 	{
	// 		printk("x: %d y: %d l:%d r:%d \n", m->x, m->y, m->left_clicked, m->right_clicked);
	// 		old_x = m->x;
	// 		old_y = m->y;
	// 	}
	// 	asm volatile("hlt");
	// }
}
