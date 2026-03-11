#include "tasks.h"

#include <gui/core/compositor/compositor.h>
#include <gui/core/object/object.h>

#include <gui/ui/button/button.h>
#include <gui/ui/input/input.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/window/window.h>
#include <gui/ui/background/background.h>

#include <gui/dev/mouse/mouse.h>
#include <gui/utils/color/color.h>
#include <stdlib.h>

#include "transform_object.h"

static int (*current_formula)(int x) = NULL;
static canvas_t *current_canvas = NULL;
static window_t *current_window = NULL;
static uint32_t current_color = 0;
static int g_graph_dirty = 1;

static transform_object_t *current_object = NULL;

static input_t *g_inp_dx = NULL;
static input_t *g_inp_dy = NULL;
static input_t *g_inp_scale = NULL;
static input_t *g_inp_angle = NULL;

static int formula_parabola(int x) { return x * x / 100; }
static int formula_line(int x) { return x; }

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

static void draw_transform_object(canvas_t *cnv, transform_object_t *obj)
{
	int cx = cnv->width / 2;
	int cy = cnv->height / 2;

	// color_r/g/b — float 0.0..1.0
	uint32_t color = rgb(
	    (int)(obj->color_r * 255.0f),
	    (int)(obj->color_g * 255.0f),
	    (int)(obj->color_b * 255.0f));

	for (size_t i = 0; i < obj->count; i++)
	{
		size_t j = (i + 1) % obj->count;
		int x1 = cx + (int)obj->points[i].x;
		int y1 = cy - (int)obj->points[i].y;
		int x2 = cx + (int)obj->points[j].x;
		int y2 = cy - (int)obj->points[j].y;
		canvas_draw_line(cnv, x1, y1, x2, y2, color);
	}
}

static void graph_canvas_draw(element_t *el)
{
	canvas_t *cnv = (canvas_t *)el;

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

	if (current_object)
		draw_transform_object(cnv, current_object);

	if (!current_formula)
		return;

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

static void on_translate()
{
	float dx = atof(g_inp_dx->text);
	float dy = atof(g_inp_dy->text);
	transform_translate_homogeneous(current_object, dx, dy);
	g_graph_dirty = 1;
}

static void on_scale()
{
	float s = atof(g_inp_scale->text);
	transform_scale_homogeneous(current_object, s, s);
	g_graph_dirty = 1;
}

static void on_rotate()
{
	float a = atof(g_inp_angle->text);
	transform_rotate_homogeneous(current_object, a);
	g_graph_dirty = 1;
}

void graph_app_init(void)
{
	window_t *win = window_create(100, 100, 800, 600);

	canvas_t *cnv = canvas_create(50, 50, 600, 400);
	cnv->draw = graph_canvas_draw;
	window_addElement(win, cnv);

	button_t *btn_parabola = button_create(700, 100, 80, 30, "Parabola");
	btn_parabola->on_click = on_parabola;
	btn_parabola->style_set->normal->background_color = rgb(255, 0, 0);
	btn_parabola->style_set->normal->border_radius = 10;
	btn_parabola->style_set->hover->background_color = rgba(255, 0, 0, 0.69);
	window_addElement(win, btn_parabola);

	button_t *btn_line = button_create(700, 150, 80, 30, "Line");
	btn_line->on_click = on_line;
	window_addElement(win, btn_line);

	g_inp_dx = input_create(700, 200, 80, 25, "0");
	g_inp_dy = input_create(700, 230, 80, 25, "0");
	g_inp_scale = input_create(700, 260, 80, 25, "1");
	g_inp_angle = input_create(700, 290, 80, 25, "0");
	window_addElement(win, g_inp_dx);
	window_addElement(win, g_inp_dy);
	window_addElement(win, g_inp_scale);
	window_addElement(win, g_inp_angle);

	button_t *btn_translate = button_create(700, 320, 80, 25, "Translate");
	button_t *btn_scale_btn = button_create(700, 350, 80, 25, "Scale");
	button_t *btn_rotate = button_create(700, 380, 80, 25, "Rotate");
	btn_translate->on_click = on_translate;
	btn_scale_btn->on_click = on_scale;
	btn_rotate->on_click = on_rotate;
	window_addElement(win, btn_translate);
	window_addElement(win, btn_scale_btn);
	window_addElement(win, btn_rotate);

	// Квадрат 100x100 в центрі
	point_t pts[] = {
	    {-50.0f, -50.0f},
	    {50.0f, -50.0f},
	    {50.0f, 50.0f},
	    {-50.0f, 50.0f},
	};
	current_object = transform_object_create(pts, 4);
	current_object->color_r = 0.2f;
	current_object->color_g = 0.6f;
	current_object->color_b = 1.0f;

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
