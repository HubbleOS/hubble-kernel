#include "tasks.h"

#include <gui/core/compositor/compositor.h>
#include <gui/core/object/object.h>

#include <gui/ui/button/button.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/window/window.h>
#include <gui/ui/background/background.h>

#include <gui/utils/color/color.h>

static int (*current_formula)(int x) = NULL;
static canvas_t *current_canvas = NULL;
static window_t *current_window = NULL;
static uint32_t current_color = 0;

static int formula_parabola(int x) { return x * x / 100; }
static int formula_line(int x) { return x; }

static int g_graph_dirty = 1;

static void on_parabola()
{
	current_formula = formula_parabola;
	current_color = rgb(0, 255, 0);
	g_graph_dirty = 1;
}

static void on_line()
{
	current_formula = formula_line;
	current_color = rgb(255, 0, 0);
	g_graph_dirty = 1;
}

static void graph_canvas_draw(element_t *el)
{
	canvas_t *cnv = (canvas_t *)el;
	if (!current_formula)
		return;

	int cx = cnv->width / 2;
	int cy = cnv->height / 2;

	canvas_clear(cnv, rgb(20, 20, 20));

	canvas_draw_line(cnv, cx, 0, cx, cnv->height, rgb(255, 255, 255));
	canvas_draw_line(cnv, 0, cy, cnv->width, cy, rgb(255, 255, 255));

	int step = 40;
	for (int x = cx; x < cnv->width; x += step)
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
	for (int x = cx - step; x > 0; x -= step)
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
	for (int y = cy; y < cnv->height; y += step)
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));
	for (int y = cy - step; y > 0; y -= step)
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));

	int prev_x = 0, prev_y = 0, first = 1;
	for (int x = 0; x < cnv->width; x++)
	{
		int fx = x - cx;
		int fy = current_formula(fx);
		int y_canvas = cy - fy;

		if (y_canvas < 0 || y_canvas >= cnv->height)
		{
			first = 1;
			continue;
		}

		if (!first)
			canvas_draw_line(cnv, prev_x, prev_y, x, y_canvas, current_color);

		prev_x = x;
		prev_y = y_canvas;
		first = 0;
	}
}

void graph_app_init(void)
{
	window_t *win = window_create(100, 100, 800, 600);

	canvas_t *cnv = canvas_create(50, 50, 600, 400);
	cnv->draw = graph_canvas_draw;
	window_addElement(win, cnv);

	button_t *btn_parabola = button_create(700, 100, 80, 30, "Parabola");
	btn_parabola->on_click = on_parabola;
	window_addElement(win, btn_parabola);

	button_t *btn_line = button_create(700, 150, 80, 30, "Line");
	btn_line->on_click = on_line;
	window_addElement(win, btn_line);

	current_formula = formula_parabola;
	current_canvas = cnv;
	current_color = rgb(0, 255, 0);
	current_window = win;
}

void graph_app_render(void)
{
	if (!g_graph_dirty || !current_canvas || !current_window)
		return;

	g_graph_dirty = 0;

	current_canvas->needs_redraw = true;
	element_mark_dirty(current_canvas);
	object_flush(current_window->surface);
}
