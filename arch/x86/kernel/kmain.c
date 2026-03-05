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

#include "gui/background.h"

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

void kernel_main(BootInfo *bi) __attribute__((alias("kernel_entry")));

void fps_delay(uint32_t fps)
{
	if (fps == 0)
		return;

	uint64_t ms_per_frame = 1000 / fps;
	hpet_delay_ms(ms_per_frame);
}

static void int_to_str(int val, char *buf, int buf_size)
{
	if (buf_size <= 0)
		return;
	int i = 0;
	bool negative = false;
	if (val < 0)
	{
		negative = true;
		val = -val;
	}
	do
	{
		if (i >= buf_size - 1)
			break;
		buf[i++] = '0' + (val % 10);
		val /= 10;
	} while (val > 0);
	if (negative && i < buf_size - 1)
		buf[i++] = '-';
	buf[i] = '\0';

	for (int j = 0; j < i / 2; j++)
	{
		char tmp = buf[j];
		buf[j] = buf[i - 1 - j];
		buf[i - 1 - j] = tmp;
	}
}

int formula_parabola(int x) { return x * x / 100; } // делим на 100, чтобы поместилось в canvas
int formula_line(int x) { return x; }

// глобальные переменные для текущего графика
static int (*current_formula)(int x) = NULL;
static canvas_t *current_canvas = NULL;
static uint32_t current_color = 0;

// функция рисования графика
void draw_graph(void)
{
	if (!current_canvas || !current_formula)
		return;

	canvas_t *cnv = current_canvas;
	int cx = cnv->base.width / 2;
	int cy = cnv->base.height / 2;

	// очищаем
	canvas_clear(cnv, rgb(20, 20, 20));

	// оси
	canvas_draw_line(cnv, cx, 0, cx, cnv->base.height, rgb(255, 255, 255));
	canvas_draw_line(cnv, 0, cy, cnv->base.width, cy, rgb(255, 255, 255));

	// деления каждые 40 пикселей
	int step = 40;
	char buf[8];

	// X
	for (int x = cx; x < cnv->base.width; x += step)
	{
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
		int_to_str(x - cx, buf, sizeof(buf));
		canvas_draw_text(cnv, x - 4, cy + 5, buf, rgb(200, 200, 200));
	}
	for (int x = cx - step; x > 0; x -= step)
	{
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
		int_to_str(x - cx, buf, sizeof(buf));
		canvas_draw_text(cnv, x - 8, cy + 5, buf, rgb(200, 200, 200));
	}

	// Y
	for (int y = cy; y < cnv->base.height; y += step)
	{
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));
		int_to_str(cy - y, buf, sizeof(buf));
		canvas_draw_text(cnv, cx + 5, y - 4, buf, rgb(200, 200, 200));
	}
	for (int y = cy - step; y > 0; y -= step)
	{
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));
		int_to_str(cy - y, buf, sizeof(buf));
		canvas_draw_text(cnv, cx + 5, y - 4, buf, rgb(200, 200, 200));
	}

	// график
	int prev_x = 0, prev_y = 0, first = 1;
	for (int x = 0; x < cnv->base.width; x++)
	{
		int fx = x - cx;
		int fy = current_formula(fx);
		int y_canvas = cy - fy;

		if (y_canvas < 0)
			y_canvas = 0;
		if (y_canvas >= cnv->base.height)
			y_canvas = cnv->base.height - 1;

		if (!first)
			canvas_draw_line(cnv, prev_x, prev_y, x, y_canvas, current_color);

		prev_x = x;
		prev_y = y_canvas;
		first = 0;
	}
}

// callback кнопки
void on_parabola(void *unused)
{
	current_formula = formula_parabola;
	current_color = rgb(0, 255, 0);
}

void on_line(void *unused)
{
	current_formula = formula_line;
	current_color = rgb(255, 0, 0);
}

void render_task(void)
{
	compositor_init();

	object_t *bg = background_create(rgb(0, 0, 0));
	window_t *win = window_create(100, 100, 800, 600);

	// кнопки
	canvas_t *cnv = canvas_create(50, 50, 600, 400);
	window_addElement(win, &cnv->base);

	button_t *btn_parabola = button_create(700, 100, 80, 30, "Parabola");
	btn_parabola->on_click = on_parabola;
	// btn_parabola->data = cnv;
	window_addElement(win, &btn_parabola->base);

	button_t *btn_line = button_create(700, 150, 80, 30, "Line");
	btn_line->on_click = on_line;
	// btn_line->data = cnv;
	window_addElement(win, &btn_line->base);

	// стартовая формула
	current_formula = formula_parabola;
	current_canvas = cnv;
	current_color = rgb(0, 255, 0);

	// начальная отрисовка
	draw_graph();

	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));
	mouse_t *m = get_mouse_info();

	while (1)
	{
		draw_graph();
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
