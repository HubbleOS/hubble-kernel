#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/smp.h>
#include "init/init.h"
#include <printk.h>
#include "higher_half.h"
#include "io.h"

// Forward declarations
extern void os_main(BootInfo *bi);
extern int load_elf_and_run(const char *path);

// -----------------------------------------------------------------------------
// Kernel entry
// -----------------------------------------------------------------------------

__attribute__((section(".text.boot")))
__attribute__((used)) void
kernel_entry(BootInfo *bi)
{
	clear_bss();
	relocate_boot_info(bi);

	early_printk_init(bi->framebuffer);

	printk(KERN_INFO "=== Higher-Half Kernel Starting ===\n");

	acpi_init(bi->rsdp);

	init.memory(bi);
	init.cpu();

	hpet_init();

	init.filesystems();

	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");
	outb(0x3F8, 'A');
	hpet_delay_ms(3000);

	apic_debug_check();
	smp_init();
	scheduler_init();

	// acpi_reboot();
	// acpi_shutdown();

	// load_elf_and_run("/usr/bin/user.elf");

	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
