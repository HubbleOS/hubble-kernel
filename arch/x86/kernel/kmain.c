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

#include "gui/background.h"
#include "gui/window.h"
#include "gui/cursor.h"

#include "gui/ui/rect/rect.h"

#include "gui/screen.h"
#include "gui/compositor.h"

#include <sys/keyboard.h>

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
	ps2_init();
	mouse_init();
	keyboard_init();

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

	object_t *bg = background_create(rgb(0, 0, 0));

	window_t *win = window_create(100, 100, 800, 600);
	// window_t *win2 = window_create(100, 100, 800, 600);

	element_t *square = create_rect(0, 0, 100, 100, rgb(255, 0, 0));
	element_t *square1 = create_rect(0, 110, 290, 10, rgb(14, 255, 54));

	window_addElement(win, square1);
	window_addElement(win, square);

	int x, y = 0;
	int dx = 1;

	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));

	mouse_t *m = get_mouse_info();

	while (1)
	{
		if (cursor->x != m->x || cursor->y != m->y)
			cursor_move(cursor, m->x, m->y);

		if (square->x != m->x || square->y != m->y)
			if (m->left == 1)
				object_move_element(win->surface, square, m->x - win->surface->x, m->y - win->surface->y);

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

	// while (1)
	// {
	// 	char c = keyboard_get_char();
	// 	printk("key: %c\n", c);
	// }

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

// task_t *task2 = task_create(counter_task, 255);
// scheduler_add_task(task2);
// while (1)
// {
// 	printk("kmain thread\n");
// 	hpet_delay_ms(1000);
// 	asm volatile("hlt");
// }

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
