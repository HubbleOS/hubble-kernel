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

#include <gui/core/screen/screen.h>

#include <tasks/task.h>

extern int load_elf_and_run(const char *path);

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

	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();

	smp_init();

	screen_init(g_boot_info->framebuffer);
	scheduler_init();

	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
