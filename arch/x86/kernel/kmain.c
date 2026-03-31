#include <hubble/kernel.h>
#include <hubble/platform.h>
#include <hubble/printk.h>

#include "init/init.h"

#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/smp.h>
#include <dev/mouse.h>
#include <dev/keyboard.h>
#include <dev/ps2.h>
#include <io.h>
#include <asm.h>

platform_info_t g_platform;

#include <user/exec.h>

extern int elf_load(const char *path, uint64_t *entry_out, uint64_t *pm);

uint64_t fb_mmap(uint64_t offset, size_t size)
{
	return (uint64_t)g_boot_info->framebuffer->base;
};

#include <src/early_console.h>

__attribute__((section(".text.boot"))) void
kernel_entry(BootInfo *bi)
{
	clear_bss();
	relocate_boot_info(bi);
	g_boot_info = bi;

	g_platform.fb_base = (uint64_t)g_boot_info->framebuffer->base;
	g_platform.fb_width = g_boot_info->framebuffer->width;
	g_platform.fb_height = g_boot_info->framebuffer->height;
	g_platform.fb_pitch = g_boot_info->framebuffer->pitch;

	early_printk_init(g_boot_info->framebuffer);

	acpi_init(g_boot_info->rsdp);
	init_memory(g_boot_info);
	hpet_init();
	init_cpu();

	//
	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();

	smp_init();

	kernel_main();

	scheduler_init();

	while (1)
	{
		hlt();
	}
}

void kmain_thread(void)
{
	printk("kmain thread\n");

	task_t *task1 = exec("/usr/bin/user.elf");
	printk("user at cr3: 0x%016lx\n", task1->page_table);
	scheduler_add_task(task1);

	while (1)
	{
		hlt();
	}
}
