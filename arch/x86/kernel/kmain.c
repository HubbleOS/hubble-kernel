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
#include <fs/vfs/dev.h>

#include "gui/background.h"
#include <errno.h>

extern int elf_load(const char *path, uint64_t *entry_out);

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
	uint64_t fb_mmap(uint64_t offset, size_t size)
	{
		return (uint64_t)g_fb->base;
	};
	dev_vfs_register("fb0", fb_mmap, NULL);

	printk(KERN_INFO "\n=== Kernel Initialization Complete ===\n\n");
	outb(0x3F8, 'A');

	apic_debug_check();
	ps2_init();
	mouse_init();
	keyboard_init();
	dev_vfs_register("mouse", mouse_mmap, mouse_read_file);

	smp_init();

	screen_init(bi->framebuffer);

	scheduler_init();

	while (1)
		asm volatile("hlt");
}

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

#include <lab/tasks.h>

static void fps_delay(uint32_t fps)
{
	if (fps == 0)
		return;
	hpet_delay_ms(1000 / fps);
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
		graph_app_update();
		geometry_app_update();

		mouse_update(m->x, m->y, m->left);

		if (cursor->x != m->x || cursor->y != m->y)
			cursor_move(cursor, m->x, m->y);

		if (g_mouse.drag_obj)
		{
			if (g_mouse.drag_el)
				object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
						    g_mouse.x - g_mouse.drag_offset_x - g_mouse.drag_obj->x,
						    g_mouse.y - g_mouse.drag_offset_y - g_mouse.drag_obj->y);
			else
				compositor_move_object(g_mouse.drag_obj,
						       g_mouse.x - g_mouse.drag_offset_x,
						       g_mouse.y - g_mouse.drag_offset_y);

			compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
		}

		compositor_render();
		fps_delay(60);
	}
}
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
