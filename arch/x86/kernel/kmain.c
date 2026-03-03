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

#include "gui/screen.h"
#include "gui/window.h"
#include "gui/compositor.h"

#include "gui/ui/rect/rect.h"

// Forward declarations
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
	g_boot_info = bi;

	early_printk_init(g_boot_info->framebuffer);

	printk(KERN_INFO "=== Higher-Half Kernel Starting ===\n");

	acpi_init(bi->rsdp);
	hpet_init();
	init.memory(bi);
	init.cpu();

	init.filesystems();

	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");
	outb(0x3F8, 'A');
	// hpet_delay_ms(3000);

	apic_debug_check();
	smp_init();

	screen_init(bi->framebuffer);
	scheduler_init();

	// acpi_reboot();
	// acpi_shutdown();

	// load_elf_and_run("/usr/bin/user.elf");

	while (1)
		asm volatile("hlt");
}

void fps_delay(uint32_t fps)
{
	if (fps == 0)
		return;

	uint64_t ms_per_frame = 1000 / fps;
	hpet_delay_ms(ms_per_frame);
}

#include <utils/font.h>

void render_task(void)
{
	compositor_init();

	window_t *win = window_create(0, 0, 300, 150);

	element_t *square = create_rect(0, 0, 100, 100, rgb(255, 0, 0));
	element_t *square1 = create_rect(0, 110, 290, 10, rgb(14, 255, 54));

	window_addElement(win, square);
	window_addElement(win, square1);

	int x = 0;
	int dx = 1;

	while (1)
	{
		window_resize(win, x, 300);
		window_move(win, x, 0);

		x += dx;

		if (x > 300 || x < 0)
			dx *= -1;

		compositor_render();
		fps_delay(60);
	}
}
// void counter_task(void)
// {
// 	outb(0x3f8, 'c');
// 	uint16_t count = 0;
// 	while (1)
// 	{
// 		printk("CPU %d counter, time: %d ", lapic_get_id(), count++);
// 		hpet_delay_ms(1000);
// 		printk("%dend\n", lapic_get_id());
// 		if (count == 10)
// 		{
// 			break;
// 		}
// 		// asm volatile("hlt");
// 	}
// }

void kmain_thread(void)
{
	printk("kmain thread\n");
	task_t *task1 = task_create(render_task, 255);
	scheduler_add_task(task1);

	// task_t *task2 = task_create(counter_task, 255);
	// scheduler_add_task(task2);
	// while (1)
	// {
	// 	printk("kmain thread\n");
	// 	hpet_delay_ms(1000);
	// 	asm volatile("hlt");
	// }
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
