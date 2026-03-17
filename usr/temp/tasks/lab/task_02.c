#include "tasks.h"

#include <gui/core/compositor/compositor.h>
#include <gui/core/object/object.h>

#include <gui/ui/button/button.h>
#include <gui/ui/input/input.h>
#include <gui/ui/label/label.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/window/window.h>
#include <gui/ui/background/background.h>

#include <gui/utils/color/color.h>

#include <stdlib.h>
#include <math.h>

#include "transform_object.h"

// figures
static const point_t TRIANGLE_PTS[] = {{0, -60}, {60, 40}, {-60, 40}};
static const point_t SQUARE_PTS[] = {{-50, -50}, {50, -50}, {50, 50}, {-50, 50}};
static const point_t HEXAGON_PTS[] = {{0, -50}, {43, -25}, {43, 25}, {0, 50}, {-43, 25}, {-43, -25}};

// global state
static canvas_t *g_cnv = NULL;
static window_t *g_win = NULL;
static transform_object_t *g_obj = NULL;
static int g_origin_x = 300;
static int g_origin_y = 240;
static int g_geo_dirty = 1;

static input_t *g_inp_dx = NULL;
static input_t *g_inp_dy = NULL;
static input_t *g_inp_scale = NULL;
static input_t *g_inp_angle = NULL;

// drawing
static void geo_draw(canvas_t *cnv, transform_object_t *obj, int ox, int oy)
{
	if (!obj || !cnv)
		return;

	uint32_t color = rgb(
	    (int)(obj->color_r * 255.0f),
	    (int)(obj->color_g * 255.0f),
	    (int)(obj->color_b * 255.0f));

	for (size_t i = 0; i < obj->count; i++)
	{
		size_t j = (i + 1) % obj->count;

		int x1 = ox + (int)obj->points[i].x;
		int y1 = oy + (int)obj->points[i].y;
		int x2 = ox + (int)obj->points[j].x;
		int y2 = oy + (int)obj->points[j].y;

		canvas_draw_line(cnv, x1, y1, x2, y2, color);
	}
}

static void redraw(void)
{
	if (!g_cnv)
		return;

	canvas_clear(g_cnv, rgb(20, 20, 20));

	// frame
	canvas_draw_line(g_cnv, 0, 0, g_cnv->width - 1, 0, rgb(80, 80, 80));
	canvas_draw_line(g_cnv, 0, g_cnv->height - 1, g_cnv->width - 1, g_cnv->height - 1, rgb(80, 80, 80));
	canvas_draw_line(g_cnv, 0, 0, 0, g_cnv->height - 1, rgb(80, 80, 80));
	canvas_draw_line(g_cnv, g_cnv->width - 1, 0, g_cnv->width - 1, g_cnv->height - 1, rgb(80, 80, 80));

	geo_draw(g_cnv, g_obj, g_origin_x, g_origin_y);

	if (g_win)
		object_flush(g_win->surface);
}

// helper: create a new object
static void set_shape(const point_t *pts, size_t count,
		      float r, float g, float b)
{
	if (g_obj)
		transform_object_destroy(g_obj);

	g_obj = transform_object_create((point_t *)pts, count);
	if (!g_obj)
		return;

	g_obj->color_r = r;
	g_obj->color_g = g;
	g_obj->color_b = b;

	g_geo_dirty = 1;
}

// callbacks
static void on_triangle()
{
	set_shape(TRIANGLE_PTS, 3, 0.0f, 0.8f, 1.0f);
}

static void on_square()
{
	set_shape(SQUARE_PTS, 4, 1.0f, 0.8f, 0.0f);
}

static void on_hexagon()
{
	set_shape(HEXAGON_PTS, 6, 0.8f, 0.0f, 1.0f);
}

static void on_destroy()
{
	if (g_obj)
	{
		transform_object_destroy(g_obj);
		g_obj = NULL;
	}
	g_geo_dirty = 1;
}

// transform
static void on_translate()
{
	if (!g_obj)
		return;
	float dx = atof(g_inp_dx->text);
	float dy = atof(g_inp_dy->text);
	transform_translate_homogeneous(g_obj, dx, dy);
	g_geo_dirty = 1;
}

static void on_scale()
{
	if (!g_obj)
		return;
	float s = atof(g_inp_scale->text);
	transform_scale_homogeneous(g_obj, s, s);
	g_geo_dirty = 1;
}

static void on_rotate()
{
	if (!g_obj)
		return;
	float a = atof(g_inp_angle->text);
	transform_rotate_homogeneous(g_obj, a);
	g_geo_dirty = 1;
}

// initialize

void geometry_app_init(void)
{
	g_win = window_create(80, 60, 780, 560);

	g_cnv = canvas_create(10, 10, 580, 480);
	window_addElement(g_win, g_cnv);

	button_t *btn_tri = button_create(610, 20, 100, 30, "Triangle");
	btn_tri->on_click = on_triangle;
	window_addElement(g_win, btn_tri);

	button_t *btn_sq = button_create(610, 60, 100, 30, "Square");
	btn_sq->on_click = on_square;
	window_addElement(g_win, btn_sq);

	button_t *btn_hex = button_create(610, 100, 100, 30, "Hexagon");
	btn_hex->on_click = on_hexagon;
	window_addElement(g_win, btn_hex);

	button_t *btn_del = button_create(610, 140, 100, 30, "Destroy");
	btn_del->on_click = on_destroy;
	window_addElement(g_win, btn_del);

	label_t *lbl_dx = label_create(605, 193, 40, 18, "dx:");
	label_t *lbl_dy = label_create(605, 223, 40, 18, "dy:");
	label_t *lbl_scale = label_create(598, 253, 50, 18, "scale:");
	label_t *lbl_angle = label_create(598, 283, 50, 18, "angle:");
	window_addElement(g_win, lbl_dx);
	window_addElement(g_win, lbl_dy);
	window_addElement(g_win, lbl_scale);
	window_addElement(g_win, lbl_angle);

	g_inp_dx = input_create(650, 190, 80, 25, "0");
	g_inp_dy = input_create(650, 220, 80, 25, "0");
	g_inp_scale = input_create(650, 250, 80, 25, "1");
	g_inp_angle = input_create(650, 280, 80, 25, "0");
	window_addElement(g_win, g_inp_dx);
	window_addElement(g_win, g_inp_dy);
	window_addElement(g_win, g_inp_scale);
	window_addElement(g_win, g_inp_angle);

	button_t *btn_translate = button_create(650, 315, 80, 25, "Translate");
	button_t *btn_scale_btn = button_create(650, 345, 80, 25, "Scale");
	button_t *btn_rotate = button_create(650, 375, 80, 25, "Rotate");
	btn_translate->on_click = on_translate;
	btn_scale_btn->on_click = on_scale;
	btn_rotate->on_click = on_rotate;
	window_addElement(g_win, btn_translate);
	window_addElement(g_win, btn_scale_btn);
	window_addElement(g_win, btn_rotate);

	// set a default shape
	set_shape(TRIANGLE_PTS, 3, 0.0f, 0.8f, 1.0f);
}

void geometry_app_render(void)
{
	if (!g_geo_dirty)
		return;
	g_geo_dirty = 0;
	redraw();
}
