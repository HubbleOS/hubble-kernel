#include "tasks.h"

#include <gui/core/compositor/compositor.h>
#include <gui/core/object/object.h>
#include <gui/ui/button/button.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/window/window.h>
#include <gui/utils/color/color.h>
#include <hpet/hpet.h>
#include <dev/mouse.h>
#include "gui/background.h"

static int (*current_formula)(int x) = NULL;
static canvas_t *current_canvas = NULL;
static window_t *current_window = NULL;
static uint32_t current_color = 0;

static int formula_parabola(int x) { return x * x / 100; }
static int formula_line(int x) { return x; }

static int g_graph_dirty = 1;
static int g_graph_render_dirty = 1;

static void on_parabola(void *unused)
{
	current_formula = formula_parabola;
	current_color = rgb(0, 255, 0);
	g_graph_dirty = 1;
}

static void on_line(void *unused)
{
	current_formula = formula_line;
	current_color = rgb(255, 0, 0);
	g_graph_dirty = 1;
}

void draw_graph(void)
{
	if (!current_canvas || !current_formula)
		return;

	canvas_t *cnv = current_canvas;
	int cx = cnv->base.width / 2;
	int cy = cnv->base.height / 2;

	canvas_clear(cnv, rgb(20, 20, 20));

	canvas_draw_line(cnv, cx, 0, cx, cnv->base.height, rgb(255, 255, 255));
	canvas_draw_line(cnv, 0, cy, cnv->base.width, cy, rgb(255, 255, 255));

	int step = 40;
	char buf[8];

	for (int x = cx; x < cnv->base.width; x += step)
	{
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
	}
	for (int x = cx - step; x > 0; x -= step)
	{
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
	}

	for (int y = cy; y < cnv->base.height; y += step)
	{
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));
	}
	for (int y = cy - step; y > 0; y -= step)
	{
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));
	}

	int prev_x = 0, prev_y = 0, first = 1;
	for (int x = 0; x < cnv->base.width; x++)
	{
		int fx = x - cx;
		int fy = current_formula(fx);
		int y_canvas = cy - fy;

		if (y_canvas < 0)
		{
			y_canvas = 0;
			continue;
			;
		}
		if (y_canvas >= cnv->base.height)
		{
			y_canvas = cnv->base.height - 1;
			continue;
		}

		if (!first)
			canvas_draw_line(cnv, prev_x, prev_y, x, y_canvas, current_color);

		prev_x = x;
		prev_y = y_canvas;
		first = 0;
	}

	if (current_window)
	{
		object_t *obj = current_window->surface;

		object_redraw_elements(obj);

		compositor_add_damage(
		    obj->layer,
		    obj->x + current_canvas->base.x,
		    obj->y + current_canvas->base.y,
		    current_canvas->base.width,
		    current_canvas->base.height);
	}
}

void graph_app_init(void)
{
	window_t *win = window_create(100, 100, 800, 600);

	canvas_t *cnv = canvas_create(50, 50, 600, 400);
	window_addElement(win, &cnv->base);

	button_t *btn_parabola = button_create(700, 100, 80, 30, "Parabola");
	btn_parabola->on_click = on_parabola;
	window_addElement(win, &btn_parabola->base);

	button_t *btn_line = button_create(700, 150, 80, 30, "Line");
	btn_line->on_click = on_line;
	window_addElement(win, &btn_line->base);

	current_formula = formula_parabola;
	current_canvas = cnv;
	current_color = rgb(0, 255, 0);
	current_window = win;
}

void graph_app_update(void)
{
	if (!g_graph_dirty)
		return;

	g_graph_dirty = 0;
	g_graph_render_dirty = 1;
}

void graph_app_render(void)
{
	if (!current_canvas || !current_formula)
		return;

	if (!g_graph_render_dirty)
		return;
	g_graph_render_dirty = 0;

	draw_graph();

	object_redraw_elements(current_window->surface);

	compositor_add_damage(
	    current_window->surface->layer,
	    current_window->surface->x + current_canvas->base.x,
	    current_window->surface->y + current_canvas->base.y,
	    current_canvas->base.width,
	    current_canvas->base.height);
}
