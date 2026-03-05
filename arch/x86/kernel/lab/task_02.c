#include "tasks.h"

#include <gui/core/compositor/compositor.h>
#include <gui/core/object/object.h>
#include <gui/ui/button/button.h>
#include <gui/ui/canvas/canvas.h>
#include <gui/ui/сursor/cursor.h>
#include <gui/ui/mouse/mouse.h>
#include <gui/ui/window/window.h>
#include <gui/utils/color/color.h>
#include <hpet/hpet.h>
#include <dev/mouse.h>
#include "gui/background.h"
#include <stdlib.h>
#include <string.h>

#define MAX_POINTS 32

typedef struct
{

	int points_x[MAX_POINTS];
	int points_y[MAX_POINTS];
	int point_count;

	float scale;
	int origin_x;
	int origin_y;

	uint32_t color;

	int alive;
} geo_object_t;

static const int TRIANGLE_X[] = {0, 60, -60};
static const int TRIANGLE_Y[] = {-60, 40, 40};
static const int TRIANGLE_N = 3;

static const int SQUARE_X[] = {-50, 50, 50, -50};
static const int SQUARE_Y[] = {-50, -50, 50, 50};
static const int SQUARE_N = 4;

static const int HEXAGON_X[] = {0, 43, 43, 0, -43, -43};
static const int HEXAGON_Y[] = {-50, -25, 25, 50, 25, -25};
static const int HEXAGON_N = 6;

static void geo_init(geo_object_t *obj,
		     const int *xs, const int *ys, int n,
		     int ox, int oy, float scale, uint32_t color)
{
	if (!obj || n > MAX_POINTS)
		return;

	for (int i = 0; i < n; i++)
	{
		obj->points_x[i] = xs[i];
		obj->points_y[i] = ys[i];
	}
	obj->point_count = n;
	obj->origin_x = ox;
	obj->origin_y = oy;
	obj->scale = scale;
	obj->color = color;
	obj->alive = 1;
}

static void geo_destroy(geo_object_t *obj)
{
	if (!obj)
		return;
	obj->alive = 0;
}

static void geo_draw(geo_object_t *obj, canvas_t *cnv)
{
	if (!obj || !cnv || !obj->alive || obj->point_count < 2)
		return;

	for (int i = 0; i < obj->point_count; i++)
	{
		int j = (i + 1) % obj->point_count;

		int x1 = obj->origin_x + (int)(obj->points_x[i] * obj->scale);
		int y1 = obj->origin_y + (int)(obj->points_y[i] * obj->scale);
		int x2 = obj->origin_x + (int)(obj->points_x[j] * obj->scale);
		int y2 = obj->origin_y + (int)(obj->points_y[j] * obj->scale);

		canvas_draw_line(cnv, x1, y1, x2, y2, obj->color);
	}
}

static canvas_t *g_cnv = NULL;
static window_t *g_win = NULL;
static geo_object_t g_obj;

static void redraw(void)
{
	if (!g_cnv)
		return;

	canvas_clear(g_cnv, rgb(20, 20, 20));

	canvas_draw_line(g_cnv, 0, 0, g_cnv->base.width - 1, 0, rgb(80, 80, 80));
	canvas_draw_line(g_cnv, 0, g_cnv->base.height - 1,
			 g_cnv->base.width - 1, g_cnv->base.height - 1, rgb(80, 80, 80));
	canvas_draw_line(g_cnv, 0, 0, 0, g_cnv->base.height - 1, rgb(80, 80, 80));
	canvas_draw_line(g_cnv, g_cnv->base.width - 1, 0,
			 g_cnv->base.width - 1, g_cnv->base.height - 1, rgb(80, 80, 80));

	geo_draw(&g_obj, g_cnv);

	if (g_win)
	{
		object_t *obj = g_win->surface;
		object_redraw_elements(obj);
		compositor_add_damage(obj->x + g_cnv->base.x,
				      obj->y + g_cnv->base.y,
				      g_cnv->base.width,
				      g_cnv->base.height);
	}
}

static int g_geo_dirty = 1;

static void on_triangle(void *unused)
{
	geo_init(&g_obj, TRIANGLE_X, TRIANGLE_Y, TRIANGLE_N,
		 300, 200, 1.0f, rgb(0, 200, 255));
	redraw();
	g_geo_dirty = 1;
}

static void on_square(void *unused)
{
	geo_init(&g_obj, SQUARE_X, SQUARE_Y, SQUARE_N,
		 300, 200, 1.0f, rgb(255, 200, 0));
	redraw();
	g_geo_dirty = 1;
}

static void on_hexagon(void *unused)
{
	geo_init(&g_obj, HEXAGON_X, HEXAGON_Y, HEXAGON_N,
		 300, 200, 1.0f, rgb(200, 0, 255));
	redraw();
	g_geo_dirty = 1;
}

static void on_scale_up(void *unused)
{
	if (!g_obj.alive)
		return;
	g_obj.scale *= 1.2f;
	redraw();
	g_geo_dirty = 1;
}

static void on_scale_down(void *unused)
{
	if (!g_obj.alive)
		return;
	g_obj.scale /= 1.2f;
	redraw();
	g_geo_dirty = 1;
}

static void on_destroy(void *unused)
{
	geo_destroy(&g_obj);
	redraw();
	g_geo_dirty = 1;
}

void geometry_app_init(void)
{
	g_win = window_create(80, 60, 780, 560);

	g_cnv = canvas_create(10, 10, 580, 480);
	window_addElement(g_win, &g_cnv->base);

	button_t *btn_tri = button_create(610, 20, 100, 30, "Triangle");
	btn_tri->on_click = on_triangle;
	window_addElement(g_win, &btn_tri->base);

	button_t *btn_sq = button_create(610, 60, 100, 30, "Square");
	btn_sq->on_click = on_square;
	window_addElement(g_win, &btn_sq->base);

	button_t *btn_hex = button_create(610, 100, 100, 30, "Hexagon");
	btn_hex->on_click = on_hexagon;
	window_addElement(g_win, &btn_hex->base);

	button_t *btn_up = button_create(610, 160, 100, 30, "Scale +");
	btn_up->on_click = on_scale_up;
	window_addElement(g_win, &btn_up->base);

	button_t *btn_dn = button_create(610, 200, 100, 30, "Scale -");
	btn_dn->on_click = on_scale_down;
	window_addElement(g_win, &btn_dn->base);

	button_t *btn_del = button_create(610, 260, 100, 30, "Destroy");
	btn_del->on_click = on_destroy;
	window_addElement(g_win, &btn_del->base);

	geo_init(&g_obj, TRIANGLE_X, TRIANGLE_Y, TRIANGLE_N,
		 300, 240, 1.0f, rgb(0, 200, 255));
}

void geometry_app_update(void)
{
	if (!g_geo_dirty)
		return;
	g_geo_dirty = 0;

	redraw();
	compositor_add_damage(
	    g_win->surface->x + g_cnv->base.x,
	    g_win->surface->y + g_cnv->base.y,
	    g_cnv->base.width,
	    g_cnv->base.height);
}
