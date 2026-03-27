#include "tasks.h"

#include <libgui/core.h>
#include <libgui/ui.h>
#include <libgui/utils.h>

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "polyhedron3d.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

// Built-in shapes (cube + tetrahedron + octahedron)

// Cube: 8 vertices, 12 edges
static const point3d_t CUBE_VERTS[] = {
    {-60, -60, -60}, // 0
    {60, -60, -60},  // 1
    {60, 60, -60},   // 2
    {-60, 60, -60},  // 3
    {-60, -60, 60},  // 4
    {60, -60, 60},   // 5
    {60, 60, 60},    // 6
    {-60, 60, 60}    // 7
};

static const edge_t CUBE_EDGES[] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4}, // top
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7} // vertical
};

// Tetrahedron: 4 vertices, 6 edges
static const point3d_t TETRA_VERTS[] = {
    {46.188f, 0.0f, 0.0f},
    {-23.094f, 40.0f, 0.0f},
    {-23.094f, -40.0f, 0.0f},
    {0.0f, 0.0f, 65.28f}};

static const edge_t TETRA_EDGES[] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};

// Octahedron: 6 vertices, 12 edges
static const point3d_t OCTA_VERTS[] = {
    {0, 70, 0},
    {70, 0, 0},
    {0, 0, 70},
    {-70, 0, 0},
    {0, 0, -70},
    {0, -70, 0}};

static const edge_t OCTA_EDGES[] = {
    {0, 1}, {0, 2}, {0, 3}, {0, 4}, {5, 1}, {5, 2}, {5, 3}, {5, 4}, {1, 2}, {2, 3}, {3, 4}, {4, 1} // equator edges
};

// Star: 14 vertices, 36 edges
static const point3d_t STAR_VERTS[] = {
    // cube (8)
    {-50, -50, -50},
    {50, -50, -50},
    {50, 50, -50},
    {-50, 50, -50},
    {-50, -50, 50},
    {50, -50, 50},
    {50, 50, 50},
    {-50, 50, 50},
    // spikes (6)
    {0, 0, -120},
    {0, 0, 120},
    {0, 120, 0},
    {0, -120, 0},
    {120, 0, 0},
    {-120, 0, 0}};

static const edge_t STAR_EDGES[] = {
    // cube
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
    // front spike
    {8, 0},
    {8, 1},
    {8, 2},
    {8, 3},
    // back spike
    {9, 4},
    {9, 5},
    {9, 6},
    {9, 7},
    // top spike
    {10, 2},
    {10, 3},
    {10, 6},
    {10, 7},
    // bottom spike
    {11, 0},
    {11, 1},
    {11, 4},
    {11, 5},
    // right spike
    {12, 1},
    {12, 2},
    {12, 5},
    {12, 6},
    // left spike
    {13, 0},
    {13, 3},
    {13, 4},
    {13, 7}};

static const point3d_t HUMAN_VERTS[] = {
    // ТАЗ (Центр 0,0,0)
    {-10, -5, 0}, // 0  Лево-перед
    {10, -5, 0},  // 1  Право-перед
    {-10, 5, 0},  // 2  Лево-зад
    {10, 5, 0},	  // 3  Право-зад

    // ГРУДНАЯ КЛЕТКА (Z=60)
    {-15, -8, 60}, // 4  Левое плечо перед
    {15, -8, 60},  // 5  Правое плечо перед
    {-15, 8, 60},  // 6  Левое плечо зад
    {15, 8, 60},   // 7  Правое плечо зад

    // ГОЛОВА (Z=85...115)
    {-7, -7, 85}, // 8  Основание лево-перед
    {7, -7, 85},  // 9  Основание право-перед
    {-7, 7, 85},  // 10 Основание лево-зад
    {7, 7, 85},	  // 11 Основание право-зад
    {0, 0, 110},  // 12 Макушка

    // РУКИ (Левая)
    {-25, -5, 35}, // 13 Локоть
    {-30, 0, 10},  // 14 Кисть
    // РУКИ (Правая)
    {25, -5, 35}, // 15 Локоть
    {30, 0, 10},  // 16 Кисть

    // НОГИ (Левая)
    {-8, -3, -40}, // 17 Колено
    {-8, 0, -75},  // 18 Стопа
    // НОГИ (Правая)
    {8, -3, -40}, // 19 Колено
    {8, 0, -75},  // 20 Стопа

    // ДОПОЛНИТЕЛЬНО: Шея и Живот (для связки)
    {0, 0, 30}, // 21 Центр живота
    {0, 0, 75}, // 22 Шея
};

static const edge_t HUMAN_EDGES[] = {
    // Таз (кольцо)
    {0, 1},
    {1, 3},
    {3, 2},
    {2, 0},

    // Грудь (кольцо)
    {4, 5},
    {5, 7},
    {7, 6},
    {6, 4},

    // Соединение таза и груди (туловище)
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},

    // Голова
    {8, 9},
    {9, 11},
    {11, 10},
    {10, 8}, // Основание
    {8, 12},
    {9, 12},
    {10, 12},
    {11, 12}, // К макушке
    {22, 8},
    {22, 9},
    {22, 10},
    {22, 11}, // К шее

    // Левая рука
    {4, 13},
    {6, 13},  // Плечо к локтю
    {13, 14}, // Локоть к кисти

    // Правая рука
    {5, 15},
    {7, 15},  // Плечо к локтю
    {15, 16}, // Локоть к кисти

    // Левая нога
    {0, 17},
    {2, 17},  // Таз к колену
    {17, 18}, // Колено к стопе

    // Правая нога
    {1, 19},
    {3, 19},  // Таз к колену
    {19, 20}, // Колено к стопе

    // Позвоночник (внутренний для жесткости)
    {21, 22},
    {21, 0}};

// Global state
static canvas_t *g_cnv = NULL;
static window_t *g_win = NULL;
static polyhedron3d_t *g_poly = NULL;

static int g_origin_x = 290;
static int g_origin_y = 240;
static int g_dirty = 1;

static input_t *g_inp_sx = NULL;
static input_t *g_inp_sy = NULL;
static input_t *g_inp_sz = NULL;
static input_t *g_inp_ax = NULL;
static input_t *g_inp_ay = NULL;
static input_t *g_inp_az = NULL;
static input_t *g_inp_speed = NULL;

// animation
static int g_animating = 0;
static button_t *g_btn_playpause = NULL;

// Orthographic projection with isometric-like tilt

typedef struct
{
	int x, y;
} ipoint_t;

static ipoint_t project(const point3d_t *p)
{
	ipoint_t out;

	// Y -> right
	// Z -> up
	// X -> forward

	const float k = 0.5f;

	out.x = (int)(p->y - p->x * k);
	out.y = (int)(-p->z + p->x * k);

	return out;
}

// Drawing
static void poly_draw(canvas_t *cnv, const polyhedron3d_t *poly, int ox, int oy)
{
	if (!cnv || !poly)
		return;

	uint32_t edge_color = rgb(
	    (int)(poly->color_r * 255.0f),
	    (int)(poly->color_g * 255.0f),
	    (int)(poly->color_b * 255.0f));

	uint32_t vertex_color = rgb(255, 255, 100);

	for (size_t i = 0; i < poly->count; i++)
	{
		ipoint_t pi = project(&poly->transformed[i]);

		for (size_t i = 0; i < poly->edge_count; i++)
		{
			int a = poly->edges[i].a;
			int b = poly->edges[i].b;

			ipoint_t pa = project(&poly->transformed[a]);
			ipoint_t pb = project(&poly->transformed[b]);

			canvas_draw_line(cnv,
					 ox + pa.x, oy + pa.y,
					 ox + pb.x, oy + pb.y,
					 edge_color);
		}

		// small cross at each vertex
		canvas_draw_line(cnv, ox + pi.x - 2, oy + pi.y, ox + pi.x + 2, oy + pi.y, vertex_color);
		canvas_draw_line(cnv, ox + pi.x, oy + pi.y - 2, ox + pi.x, oy + pi.y + 2, vertex_color);
	}
}

static void draw_axes(canvas_t *cnv, int ox, int oy)
{
	point3d_t origin = {0, 0, 0};

	point3d_t x_axis = {80, 0, 0};
	point3d_t y_axis = {0, 80, 0};
	point3d_t z_axis = {0, 0, 80};

	ipoint_t o = project(&origin);
	ipoint_t px = project(&x_axis);
	ipoint_t py = project(&y_axis);
	ipoint_t pz = project(&z_axis);

	// X (red)
	canvas_draw_line(cnv,
			 ox + o.x, oy + o.y,
			 ox + px.x, oy + px.y,
			 rgb(255, 80, 80));

	// Y (green)
	canvas_draw_line(cnv,
			 ox + o.x, oy + o.y,
			 ox + py.x, oy + py.y,
			 rgb(80, 255, 80));

	// Z (blue)
	canvas_draw_line(cnv,
			 ox + o.x, oy + o.y,
			 ox + pz.x, oy + pz.y,
			 rgb(80, 120, 255));
}

static void redraw(void)
{
	if (!g_cnv)
		return;

	canvas_clear(g_cnv, rgb(18, 18, 22));

	// frame
	canvas_draw_line(g_cnv, 0, 0, g_cnv->width - 1, 0, rgb(60, 60, 70));
	canvas_draw_line(g_cnv, 0, g_cnv->height - 1, g_cnv->width - 1, g_cnv->height - 1, rgb(60, 60, 70));
	canvas_draw_line(g_cnv, 0, 0, 0, g_cnv->height - 1, rgb(60, 60, 70));
	canvas_draw_line(g_cnv, g_cnv->width - 1, 0, g_cnv->width - 1, g_cnv->height - 1, rgb(60, 60, 70));

	// draw_axes(g_cnv, g_origin_x, g_origin_y);

	poly_draw(g_cnv, g_poly, g_origin_x, g_origin_y);

	if (g_win)
		object_flush(g_win->surface);
}

// Shape factory
static polyhedron3d_t *make_poly(
    const point3d_t *verts, size_t n,
    const edge_t *edges, size_t edge_count,
    float r, float g, float b)
{
	polyhedron3d_t *poly = polyhedron3d_create(n, edge_count);
	if (!poly)
		return NULL;

	memcpy(poly->vertices, verts, n * sizeof(point3d_t));
	memcpy(poly->edges, edges, edge_count * sizeof(edge_t));

	poly->color_r = r;
	poly->color_g = g;
	poly->color_b = b;

	polyhedron3d_reset(poly);
	return poly;
}

static void set_poly(polyhedron3d_t *next)
{
	if (g_poly)
		polyhedron3d_destroy(g_poly);
	g_poly = next;
	g_dirty = 1;
}

// Callbacks – shape selection

static void on_cube(void)
{
	set_poly(make_poly(CUBE_VERTS, ARRAY_SIZE(CUBE_VERTS),
			   CUBE_EDGES, ARRAY_SIZE(CUBE_EDGES),
			   0.2f, 0.8f, 1.0f));
}

static void on_tetrahedron(void)
{
	set_poly(make_poly(TETRA_VERTS, ARRAY_SIZE(TETRA_VERTS),
			   &TETRA_EDGES, ARRAY_SIZE(TETRA_EDGES),
			   1.0f, 0.2f, 0.8f));
}

static void on_octahedron(void)
{
	set_poly(make_poly(OCTA_VERTS, ARRAY_SIZE(OCTA_VERTS),
			   &OCTA_EDGES, ARRAY_SIZE(OCTA_EDGES),
			   0.8f, 1.0f, 0.2f));
}

static void on_star(void)
{
	set_poly(make_poly(STAR_VERTS, ARRAY_SIZE(STAR_VERTS),
			   STAR_EDGES, ARRAY_SIZE(STAR_EDGES),
			   1.0f, 0.9f, 0.2f));
}

static void on_human(void)
{
	set_poly(make_poly(HUMAN_VERTS, ARRAY_SIZE(HUMAN_VERTS),
			   HUMAN_EDGES, ARRAY_SIZE(HUMAN_EDGES),
			   0.9f, 0.2f, 1.0f));
}

static void on_destroy(void)
{
	set_poly(NULL);
}

static void on_reset(void)
{
	if (g_poly)
	{
		polyhedron3d_reset(g_poly);
		g_dirty = 1;
	}
}

static void on_playpause(void)
{
	g_animating = !g_animating;
	if (g_btn_playpause)
		element_set_text(g_btn_playpause, g_animating ? "Pause" : "Play");
}

// Callbacks – transforms
static void on_scale(void)
{
	if (!g_poly)
		return;
	float sx = atof(g_inp_sx->text);
	float sy = atof(g_inp_sy->text);
	float sz = atof(g_inp_sz->text);
	if (sx == 0.0f)
		sx = 1.0f;
	if (sy == 0.0f)
		sy = 1.0f;
	if (sz == 0.0f)
		sz = 1.0f;
	polyhedron3d_scale(g_poly, sx, sy, sz);
	g_dirty = 1;
}

static void on_rotate(void)
{
	if (!g_poly)
		return;
	float ax = atof(g_inp_ax->text);
	float ay = atof(g_inp_ay->text);
	float az = atof(g_inp_az->text);
	polyhedron3d_rotate_x(g_poly, ax);
	polyhedron3d_rotate_y(g_poly, ay);
	polyhedron3d_rotate_z(g_poly, az);
	g_dirty = 1;
}

// Init
void geometry3d_app_init(void)
{
	g_win = window_create(60, 40, 820, 580);

	// canvas: 580×480
	g_cnv = canvas_create(10, 10, 580, 480);
	window_addElement(g_win, g_cnv);

	// Shape buttons
	int bx = 610, by = 20;

	button_t *btn_cube = button_create(bx, by, 110, 28, "Cube");
	button_t *btn_tet = button_create(bx, by + 35, 110, 28, "Tetrahedron");
	button_t *btn_oct = button_create(bx, by + 70, 110, 28, "Octahedron");
	btn_cube->on_click = on_cube;
	btn_tet->on_click = on_tetrahedron;
	btn_oct->on_click = on_octahedron;
	window_addElement(g_win, btn_cube);
	window_addElement(g_win, btn_tet);
	window_addElement(g_win, btn_oct);

	button_t *btn_star = button_create(bx, by + 105, 110, 28, "Star");
	btn_star->on_click = on_star;
	window_addElement(g_win, btn_star);

	button_t *btn_human = button_create(bx, by + 140, 110, 28, "Human");
	btn_human->on_click = on_human;
	window_addElement(g_win, btn_human);

	// Destroy / Reset
	button_t *btn_del = button_create(610, 206, 80, 26, "Destroy");
	button_t *btn_reset = button_create(700, 206, 70, 26, "Reset");
	btn_del->on_click = on_destroy;
	btn_reset->on_click = on_reset;
	window_addElement(g_win, btn_del);
	window_addElement(g_win, btn_reset);

	// Animation
	g_btn_playpause = button_create(610, 240, 80, 28, "Play");
	g_btn_playpause->on_click = on_playpause;
	window_addElement(g_win, g_btn_playpause);

	label_t *lbl_spd = label_create(700, 244, 40, 18, "spd:");
	g_inp_speed = input_create(725, 240, 45, 28, "1");
	window_addElement(g_win, lbl_spd);
	window_addElement(g_win, g_inp_speed);

	// Scale inputs
	int iy = 290;
	label_t *lbl_sc = label_create(610, iy, 100, 18, "Scale");
	window_addElement(g_win, lbl_sc);
	iy += 20;

	label_t *lbl_sx = label_create(610, iy, 30, 18, "sx:");
	label_t *lbl_sy = label_create(610, iy + 28, 30, 18, "sy:");
	label_t *lbl_sz = label_create(610, iy + 56, 30, 18, "sz:");
	g_inp_sx = input_create(645, iy, 80, 24, "1");
	g_inp_sy = input_create(645, iy + 28, 80, 24, "1");
	g_inp_sz = input_create(645, iy + 56, 80, 24, "1");
	window_addElement(g_win, lbl_sx);
	window_addElement(g_win, g_inp_sx);
	window_addElement(g_win, lbl_sy);
	window_addElement(g_win, g_inp_sy);
	window_addElement(g_win, lbl_sz);
	window_addElement(g_win, g_inp_sz);

	button_t *btn_scale = button_create(645, iy + 84, 80, 26, "Scale");
	btn_scale->on_click = on_scale;
	window_addElement(g_win, btn_scale);

	// Rotation inputs
	iy += 120;
	label_t *lbl_rot = label_create(610, iy, 100, 18, "Rotate");
	window_addElement(g_win, lbl_rot);
	iy += 20;

	label_t *lbl_ax = label_create(610, iy, 35, 18, "X:");
	label_t *lbl_ay = label_create(610, iy + 28, 35, 18, "Y:");
	label_t *lbl_az = label_create(610, iy + 56, 35, 18, "Z:");
	g_inp_ax = input_create(648, iy, 77, 24, "0");
	g_inp_ay = input_create(648, iy + 28, 77, 24, "0");
	g_inp_az = input_create(648, iy + 56, 77, 24, "0");
	window_addElement(g_win, lbl_ax);
	window_addElement(g_win, g_inp_ax);
	window_addElement(g_win, lbl_ay);
	window_addElement(g_win, g_inp_ay);
	window_addElement(g_win, lbl_az);
	window_addElement(g_win, g_inp_az);

	button_t *btn_rotate = button_create(648, iy + 84, 77, 26, "Rotate");
	btn_rotate->on_click = on_rotate;
	window_addElement(g_win, btn_rotate);

	// default shape
	on_cube();
}

void geometry3d_app_render(void)
{
	if (g_animating && g_poly)
	{
		float speed = 1.0f;
		if (g_inp_speed)
		{
			float v = atof(g_inp_speed->text);
			if (v != 0.0f)
				speed = v;
		}
		float ax = atof(g_inp_ax->text);
		float ay = atof(g_inp_ay->text);
		float az = atof(g_inp_az->text);

		polyhedron3d_rotate_z(g_poly, speed);
		g_dirty = 1;
	}

	if (!g_dirty)
		return;
	g_dirty = 0;
	redraw();
}
