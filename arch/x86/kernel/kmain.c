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
#include <gui/core/object/object.h>
#include <gui/core/compositor/compositor.h>

#include <gui/ui/button/button.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/window/window.h>
#include <gui/ui/rect/rect.h>
#include <gui/ui/text/text.h>
#include <gui/ui/canvas/canvas.h>

#include "gui/background.h"

extern int load_elf_and_run(const char *path);

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

	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();

	smp_init();

	screen_init(bi->framebuffer);
	scheduler_init();

	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

#include <lab/tasks.h>

spinlock_t gui_lock;

static void fps_delay(uint32_t fps)
{
	if (fps == 0)
		return;
	hpet_delay_ms(1000 / fps);
}

void update_task(void)
{
	mouse_t *m = get_mouse_info();

	while (1)
	{
		spinlock_acquire(&gui_lock);

		graph_app_update();
		geometry_app_update();

		mouse_update(m->x, m->y, m->left);

		if (g_mouse.drag_obj)
		{
			if (g_mouse.drag_el)
				object_move_element(
				    g_mouse.drag_obj,
				    g_mouse.drag_el,
				    g_mouse.x - g_mouse.drag_offset_x - g_mouse.drag_obj->x,
				    g_mouse.y - g_mouse.drag_offset_y - g_mouse.drag_obj->y);
			else
				compositor_move_object(
				    g_mouse.drag_obj,
				    g_mouse.x - g_mouse.drag_offset_x,
				    g_mouse.y - g_mouse.drag_offset_y);

			compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
		}

		spinlock_release(&gui_lock);

		fps_delay(120);
	}
}

void render_task(void)
{
	compositor_init();
	background_create(rgb(20, 20, 20));

	graph_app_init();
	geometry_app_init();

	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));
	mouse_t *m = get_mouse_info();

	while (1)
	{
		spinlock_acquire(&gui_lock);

		if (cursor->surface->x != m->x || cursor->surface->y != m->y)
			cursor_move(cursor, m->x, m->y);

		graph_app_render();
		geometry_app_render();

		compositor_render();

		spinlock_release(&gui_lock);

		fps_delay(60);
	}
}

void kmain_thread(void)
{
	spinlock_init(&gui_lock, "gui");

	task_t *render = task_create(render_task, 250);
	task_t *update = task_create(update_task, 200);

	scheduler_add_task(render);
	scheduler_add_task(update);
}
