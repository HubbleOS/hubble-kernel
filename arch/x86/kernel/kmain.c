#include <bootinfo/bootinfo.h>
#include <acpi/acpi.h>
#include <apic/apic.h>
#include <hpet/hpet.h>
#include <smp/scheduler.h>
#include <smp/smp.h>
#include "init/init.h"
#include <printk.h>
#include "higher_half.h"
#include <dev/mouse.h>
#include <dev/ps2.h>
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
	hpet_init();
	init.memory(bi);
	init.cpu();

	init.filesystems();

	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");
	outb(0x3F8, 'A');
	hpet_delay_ms(3000);

	apic_debug_check();
	ps2_init();
	mouse_init();

	smp_init();
	scheduler_init();

	// acpi_reboot();
	// acpi_shutdown();

	// load_elf_and_run("/usr/bin/user.elf");

	while (1)
		asm volatile("hlt");
}
void counter_task(void);

void kmain_thread(void)
{
	printk("kmain thread\n");
	uint8_t counter = 0;

	// mouse_init();
	mouse_t *m = get_mouse_info();
	uint32_t old_x = m->x;
	uint32_t old_y = m->y;
	while (1)
	{
		if (old_x != m->x || old_y != m->y)
		{
			printk("x: %d y: %d l:%d r:%d \n", m->x, m->y, m->left_clicked, m->right_clicked);
			old_x = m->x;
			old_y = m->y;
		}
		asm volatile("hlt");
	}
}

void counter_task(void)
{
	outb(0x3f8, 'c');
	uint16_t count = 0;
	while (1)
	{
		printk("CPU %d counter, time: %d ", lapic_get_id(), count++);
		hpet_delay_ms(1000);
		printk("%dend\n", lapic_get_id());
		if (count == 10)
		{
			break;
		}
		// asm volatile("hlt");
	}
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
