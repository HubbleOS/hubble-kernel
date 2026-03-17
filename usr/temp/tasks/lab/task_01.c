#include "tasks.h"

#include <math.h>

#include <gui/core/compositor/compositor.h>
#include <gui/core/object/object.h>

#include <gui/ui/button/button.h>
#include <gui/ui/input/input.h>
#include <gui/ui/label/label.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/window/window.h>
#include <gui/ui/background/background.h>

#include <gui/dev/mouse/mouse.h>
#include <gui/utils/color/color.h>
#include <stdlib.h>

#include "transform_object.h"

static canvas_t *current_canvas = NULL;
static window_t *current_window = NULL;
static int g_graph_dirty = 1;

static transform_object_t *g_graph = NULL;
static uint32_t g_graph_color = 0;

static input_t *g_inp_dx = NULL;
static input_t *g_inp_dy = NULL;
static input_t *g_inp_scale = NULL;
static input_t *g_inp_angle = NULL;

// functions
static int formula_parabola(int x) { return x * x / 100; }
static int formula_line(int x) { return x; }

// sampling function in transform_object_t
static transform_object_t *sample_formula(int (*formula)(int x),
					  int cx, int width)
{
	// we count how many points will actually get into the canvas
	int count = 0;
	for (int x = 0; x < width; x++)
	{
		int fx = x - cx;
		(void)formula(fx);
		count++;
	}

	point_t *pts = malloc(sizeof(point_t) * count);
	if (!pts)
		return NULL;

	int idx = 0;
	for (int x = 0; x < width; x++)
	{
		int fx = x - cx;
		int fy = formula(fx);
		pts[idx].x = (float)fx;
		pts[idx].y = (float)fy;
		idx++;
	}

	transform_object_t *obj = transform_object_create(pts, count);
	free(pts);
	return obj;
}

// set a new graph
static void set_formula(int (*formula)(int x), uint32_t color)
{
	if (g_graph)
	{
		transform_object_destroy(g_graph);
		g_graph = NULL;
	}

	if (!current_canvas)
		return;

	int cx = current_canvas->width / 2;
	g_graph = sample_formula(formula, cx, current_canvas->width);
	if (!g_graph)
		return;

	// color with uint32_t → float 0..1
	g_graph->color_r = ((color >> 16) & 0xFF) / 255.0f;
	g_graph->color_g = ((color >> 8) & 0xFF) / 255.0f;
	g_graph->color_b = ((color) & 0xFF) / 255.0f;

	g_graph_color = color;
	g_graph_dirty = 1;
}

static void on_parabola() { set_formula(formula_parabola, rgb(0, 255, 0)); }
static void on_line() { set_formula(formula_line, rgb(255, 0, 0)); }

// canvas draw
static void graph_canvas_draw(element_t *el)
{
	canvas_t *cnv = (canvas_t *)el;

	int cx = cnv->width / 2;
	int cy = cnv->height / 2;

	canvas_clear(cnv, rgb(20, 20, 20));

	// axis
	canvas_draw_line(cnv, cx, 0, cx, cnv->height, rgb(255, 255, 255));
	canvas_draw_line(cnv, 0, cy, cnv->width, cy, rgb(255, 255, 255));

	// net
	int step = 40;
	for (int x = cx; x < cnv->width; x += step)
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
	for (int x = cx - step; x > 0; x -= step)
		canvas_draw_line(cnv, x, cy - 3, x, cy + 3, rgb(200, 200, 200));
	for (int y = cy; y < cnv->height; y += step)
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));
	for (int y = cy - step; y > 0; y -= step)
		canvas_draw_line(cnv, cx - 3, y, cx + 3, y, rgb(200, 200, 200));

	if (!g_graph)
		return;

	uint32_t color = rgb(
	    (int)(g_graph->color_r * 255.0f),
	    (int)(g_graph->color_g * 255.0f),
	    (int)(g_graph->color_b * 255.0f));

	// we draw lines between adjacent points
	int first = 1;
	int prev_px = 0, prev_py = 0;

	for (size_t i = 0; i < g_graph->count; i++)
	{
		int px = cx + (int)g_graph->points[i].x;
		int py = cy - (int)g_graph->points[i].y;

		if (py < 0 || py >= cnv->height || px < 0 || px >= cnv->width)
		{
			first = 1;
			continue;
		}

		if (!first)
			canvas_draw_line(cnv, prev_px, prev_py, px, py, color);

		prev_px = px;
		prev_py = py;
		first = 0;
	}
}

// transformation

static void on_translate()
{
	if (!g_graph)
		return;
	float dx = atof(g_inp_dx->text);
	float dy = atof(g_inp_dy->text);
	transform_translate_homogeneous(g_graph, dx, dy);
	g_graph_dirty = 1;
}

static void on_scale()
{
	if (!g_graph)
		return;
	float s = atof(g_inp_scale->text);
	transform_scale_homogeneous(g_graph, s, s);
	g_graph_dirty = 1;
}

static void on_rotate()
{
	if (!g_graph)
		return;
	float a = atof(g_inp_angle->text);
	transform_rotate_homogeneous(g_graph, a);
	g_graph_dirty = 1;
}

// initialization

void graph_app_init(void)
{
	window_t *win = window_create(100, 100, 800, 600);

	canvas_t *cnv = canvas_create(50, 50, 600, 400);
	cnv->draw = graph_canvas_draw;
	window_addElement(win, cnv);

	current_canvas = cnv;
	current_window = win;

	button_t *btn_parabola = button_create(700, 100, 80, 30, "Parabola");
	btn_parabola->on_click = on_parabola;
	btn_parabola->style_set->normal->background_color = rgb(255, 0, 0);
	btn_parabola->style_set->normal->border_radius = 10;
	btn_parabola->style_set->hover->background_color = rgba(255, 0, 0, 0.69);
	window_addElement(win, btn_parabola);

	button_t *btn_line = button_create(700, 150, 80, 30, "Line");
	btn_line->on_click = on_line;
	window_addElement(win, btn_line);

	label_t *lbl_dx = label_create(655, 203, 40, 18, "dx:");
	label_t *lbl_dy = label_create(655, 233, 40, 18, "dy:");
	label_t *lbl_scale = label_create(645, 263, 50, 18, "scale:");
	label_t *lbl_angle = label_create(645, 293, 50, 18, "angle:");
	window_addElement(win, lbl_dx);
	window_addElement(win, lbl_dy);
	window_addElement(win, lbl_scale);
	window_addElement(win, lbl_angle);

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

	// set default formula
	set_formula(formula_parabola, rgb(0, 255, 0));
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
