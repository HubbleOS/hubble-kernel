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
#include "gui/object.h"
#include "gui/compositor.h"
#include "gui/ui/button/button.h"

#include "gui/ui/mouse/mouse.h"

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

void on_click(void *data)
{
	window_create(0, 0, 400, 300);
}

void render_task(void)
{
	compositor_init();

	object_t *bg = background_create(rgb(0, 0, 0));
	window_t *win = window_create(100, 100, 800, 600);

	button_t *btn = button_create(100, 100, 100, 50, "Button");
	btn->on_click = on_click;

	window_addElement(win, &btn->base);

	element_t *square = create_rect(0, 0, 100, 100, rgb(255, 0, 0));
	element_t *square1 = create_rect(0, 110, 290, 50, rgb(14, 255, 54));

	window_addElement(win, square1);
	window_addElement(win, square);

	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));
	mouse_t *m = get_mouse_info();

	while (1)
	{
		mouse_update(m->x, m->y, m->left);

		if (cursor->x != m->x || cursor->y != m->y)
			cursor_move(cursor, m->x, m->y);

		// Drag update
		if (g_mouse.drag_obj)
		{
			if (g_mouse.drag_el)
			{
				object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
						    g_mouse.x - g_mouse.drag_offset_x - g_mouse.drag_obj->x,
						    g_mouse.y - g_mouse.drag_offset_y - g_mouse.drag_obj->y);
			}
			else
			{
				compositor_move_object(g_mouse.drag_obj,
						       g_mouse.x - g_mouse.drag_offset_x,
						       g_mouse.y - g_mouse.drag_offset_y);
			}
			compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
		}

		compositor_render();
		fps_delay(60);
	}
}

void kmain_thread(void)
{
	printk("kmain thread\n");
	task_t *task1 = task_create(render_task, 255);
	scheduler_add_task(task1);
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));
