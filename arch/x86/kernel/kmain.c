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

#include <src/early_console.h>

BootInfo *g_boot_info;

void kmain()
{
	g_platform.fb_base = (uint64_t)g_boot_info->framebuffer.base;
	g_platform.fb_width = g_boot_info->framebuffer.width;
	g_platform.fb_height = g_boot_info->framebuffer.height;
	g_platform.fb_pitch = g_boot_info->framebuffer.pitch;

	early_printk_init(&g_boot_info->framebuffer);

	init_cpu();		      // GDT, IDT, TSS, PIC remap
	acpi_init(g_boot_info->rsdp); // parses MADT, learns LAPIC/IOAPIC addresses
	init_memory(g_boot_info);
	apic_init_bsp(); // now the LAPIC address is known
	hpet_init();

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
