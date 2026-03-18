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
// #include <tasks/task.h>

extern int elf_load(const char *path, uint64_t *entry_out);

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

	smp_init();

	scheduler_init();

	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

void kmain_thread(void)
{
	printk("kmain thread\n");
	// task_t *task1 = task_create(render_task, 255);
	// scheduler_add_task(task1);
	uint64_t entry;
	elf_load("/usr/bin/user.elf", &entry);
	task_t *task1 = task_create((void *)entry, 255, 1);
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
