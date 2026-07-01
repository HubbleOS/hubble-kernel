#include "tasks.h"

#include <libgui/core.h>
#include <libgui/ui.h>
#include <libgui/utils.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "polyhedron3d.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/*
   CUBE  8 verts, 12 edges, 12 tri-faces
 */
static const point3d_t CUBE_VERTS[] = {
    {-60, -60, -60}, {60, -60, -60}, {60, 60, -60}, {-60, 60, -60},
    {-60, -60, 60},  {60, -60, 60},  {60, 60, 60},  {-60, 60, 60}};
static const edge_t CUBE_EDGES[] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                    {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                    {0, 4}, {1, 5}, {2, 6}, {3, 7}};
/* 6 quads → 12 triangles (CCW when viewed from outside) */
static const face_t CUBE_FACES[] = {
    /* bottom  (-Z) */ {0, 3, 2}, {0, 2, 1},
    /* top     (+Z) */ {4, 5, 6}, {4, 6, 7},
    /* front   (-Y) */ {0, 1, 5}, {0, 5, 4},
    /* back    (+Y) */ {2, 3, 7}, {2, 7, 6},
    /* left    (-X) */ {0, 4, 7}, {0, 7, 3},
    /* right   (+X) */ {1, 2, 6}, {1, 6, 5}};

/*
   TETRAHEDRON  4 verts, 6 edges, 4 tri-faces
 */
static const point3d_t TETRA_VERTS[] = {{46.188f, 0.0f, 0.0f},
                                        {-23.094f, 40.0f, 0.0f},
                                        {-23.094f, -40.0f, 0.0f},
                                        {0.0f, 0.0f, 65.28f}};
static const edge_t TETRA_EDGES[] = {{0, 1}, {0, 2}, {0, 3},
                                     {1, 2}, {1, 3}, {2, 3}};
static const face_t TETRA_FACES[] = {
    {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}};

/*
   OCTAHEDRON  6 verts, 12 edges, 8 tri-faces
 */
static const point3d_t OCTA_VERTS[] = {{0, 70, 0},  {70, 0, 0},  {0, 0, 70},
                                       {-70, 0, 0}, {0, 0, -70}, {0, -70, 0}};
static const edge_t OCTA_EDGES[] = {{0, 1}, {0, 2}, {0, 3}, {0, 4},
                                    {5, 1}, {5, 2}, {5, 3}, {5, 4},
                                    {1, 2}, {2, 3}, {3, 4}, {4, 1}};
static const face_t OCTA_FACES[] = {
    /* top pyramid */ {0, 1, 2},
    {0, 2, 3},
    {0, 3, 4},
    {0, 4, 1},
    /* bottom pyramid */ {5, 2, 1},
    {5, 3, 2},
    {5, 4, 3},
    {5, 1, 4}};

/*
   STAR  14 verts – approximated as centre+spike tri-fans
   (cube faces + 6 spike pyramids → 12+24 = 36 tri-faces)
 */
static const point3d_t STAR_VERTS[] = {
    {-50, -50, -50}, {50, -50, -50}, {50, 50, -50}, {-50, 50, -50},
    {-50, -50, 50},  {50, -50, 50},  {50, 50, 50},  {-50, 50, 50},
    {0, 0, -120},    {0, 0, 120},    {0, 120, 0},   {0, -120, 0},
    {120, 0, 0},     {-120, 0, 0}};
static const edge_t STAR_EDGES[] = {
    {0, 1},  {1, 2},  {2, 3},  {3, 0},  {4, 5},  {5, 6},  {6, 7},  {7, 4},
    {0, 4},  {1, 5},  {2, 6},  {3, 7},  {8, 0},  {8, 1},  {8, 2},  {8, 3},
    {9, 4},  {9, 5},  {9, 6},  {9, 7},  {10, 2}, {10, 3}, {10, 6}, {10, 7},
    {11, 0}, {11, 1}, {11, 4}, {11, 5}, {12, 1}, {12, 2}, {12, 5}, {12, 6},
    {13, 0}, {13, 3}, {13, 4}, {13, 7}};
static const face_t STAR_FACES[] = {
    /* cube quads → tris */
    {0, 3, 2},
    {0, 2, 1},
    {4, 5, 6},
    {4, 6, 7},
    {0, 1, 5},
    {0, 5, 4},
    {2, 3, 7},
    {2, 7, 6},
    {0, 4, 7},
    {0, 7, 3},
    {1, 2, 6},
    {1, 6, 5},
    /* -Z spike */ {8, 1, 0},
    {8, 2, 1},
    {8, 3, 2},
    {8, 0, 3},
    /* +Z spike */ {9, 4, 5},
    {9, 5, 6},
    {9, 6, 7},
    {9, 7, 4},
    /* +Y spike */ {10, 3, 7},
    {10, 7, 6},
    {10, 6, 2},
    {10, 2, 3},
    /* -Y spike */ {11, 0, 1},
    {11, 5, 4},
    {11, 1, 5},
    {11, 4, 0},
    /* +X spike */ {12, 1, 2},
    {12, 6, 5},
    {12, 2, 6},
    {12, 5, 1},
    /* -X spike */ {13, 3, 0},
    {13, 4, 7},
    {13, 0, 4},
    {13, 7, 3}};

/*
   HUMAN – no closed faces; kept as wire-only (face_count = 0)
 */
static const point3d_t HUMAN_VERTS[] = {
    {-10, -5, 0}, {10, -5, 0},  {-10, 5, 0},   {10, 5, 0},    {-15, -8, 60},
    {15, -8, 60}, {-15, 8, 60}, {15, 8, 60},   {-7, -7, 85},  {7, -7, 85},
    {-7, 7, 85},  {7, 7, 85},   {0, 0, 110},   {-25, -5, 35}, {-30, 0, 10},
    {25, -5, 35}, {30, 0, 10},  {-8, -3, -40}, {-8, 0, -75},  {8, -3, -40},
    {8, 0, -75},  {0, 0, 30},   {0, 0, 75}};
static const edge_t HUMAN_EDGES[] = {
    {0, 1},   {1, 3},   {3, 2},   {2, 0},  {4, 5},   {5, 7},   {7, 6},
    {6, 4},   {0, 4},   {1, 5},   {2, 6},  {3, 7},   {8, 9},   {9, 11},
    {11, 10}, {10, 8},  {8, 12},  {9, 12}, {10, 12}, {11, 12}, {22, 8},
    {22, 9},  {22, 10}, {22, 11}, {4, 13}, {6, 13},  {13, 14}, {5, 15},
    {7, 15},  {15, 16}, {0, 17},  {2, 17}, {17, 18}, {1, 19},  {3, 19},
    {19, 20}, {21, 22}, {21, 0}};

/*
   Observer (same as before)
 */
#define CAM_X (-400.0f)
#define CAM_Y 0.0f
#define CAM_Z 0.0f
#define FOCAL 400.0f

typedef struct {
  int x, y;
} ipoint_t;

static ipoint_t project(const point3d_t *p) {
  float dx = p->x - CAM_X;
  float dy = p->y - CAM_Y;
  float dz = p->z - CAM_Z;
  float t = (dx == 0.0f) ? 1.0f : FOCAL / dx;
  return (ipoint_t){(int)(dy * t), (int)(-dz * t)};
}

/*
   Back-face culling helpers
 */

/* Cross product of (b-a) × (c-a) */
static point3d_t cross(point3d_t a, point3d_t b, point3d_t c) {
  float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
  float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
  return (point3d_t){uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx};
}

/* Dot product */
static float dot(point3d_t a, point3d_t b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

/*
 * face_visible() — returns 1 if the face normal points toward the observer.
 *
 * Normal N = (B-A) × (C-A)   (vertices in CCW order from outside)
 * View ray  V = observer - face_centroid
 * Visible  ⟺  dot(N, V) > 0
 */
static int face_visible(const point3d_t *t, const face_t *f) {
  point3d_t a = t[f->a], b = t[f->b], c = t[f->c];

  point3d_t n = cross(a, b, c);

  /* centroid of the face */
  point3d_t ctr = {(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f,
                   (a.z + b.z + c.z) / 3.0f};

  point3d_t view = {CAM_X - ctr.x, CAM_Y - ctr.y, CAM_Z - ctr.z};

  return dot(n, view) > 0.0f;
}

/*
 * Build a boolean mask: visible_edge[i] = 1 if edge i belongs to at
 * least one front-facing face.
 * Edges that belong to NO face (wire-only shapes like Human) are always
 * visible.
 */
static void build_edge_mask(const polyhedron3d_t *poly, int *mask) {
  /* mark all edges "not yet claimed by any face" */
  int *claimed = calloc(poly->edge_count, sizeof(int));

  for (size_t i = 0; i < poly->edge_count; i++)
    mask[i] = 0;

  for (size_t fi = 0; fi < poly->face_count; fi++) {
    const face_t *f = &poly->faces[fi];
    int vis = face_visible(poly->transformed, f);

    /* For each edge of this face, find its index in poly->edges */
    int tri[3][2] = {{f->a, f->b}, {f->b, f->c}, {f->c, f->a}};
    for (int t = 0; t < 3; t++) {
      int pa = tri[t][0], pb = tri[t][1];
      for (size_t ei = 0; ei < poly->edge_count; ei++) {
        int ea = poly->edges[ei].a, eb = poly->edges[ei].b;
        if ((ea == pa && eb == pb) || (ea == pb && eb == pa)) {
          claimed[ei] = 1;
          if (vis)
            mask[ei] = 1;
          break;
        }
      }
    }
  }

  /* unclaimed edges (wire-only) are always drawn */
  for (size_t i = 0; i < poly->edge_count; i++)
    if (!claimed[i])
      mask[i] = 1;

  free(claimed);
}

/*
   Drawing
 */
static void poly_draw(canvas_t *cnv, const polyhedron3d_t *poly, int ox,
                      int oy) {
  if (!cnv || !poly)
    return;

  uint32_t edge_color =
      rgb((int)(poly->color_r * 255.0f), (int)(poly->color_g * 255.0f),
          (int)(poly->color_b * 255.0f));
  // uint32_t vertex_color = rgb(255, 255, 100);

  /* build visibility mask */
  int *mask = malloc(poly->edge_count * sizeof(int));
  if (!mask)
    return;
  build_edge_mask(poly, mask);

  /* draw visible edges */
  for (size_t i = 0; i < poly->edge_count; i++) {
    if (!mask[i])
      continue;

    int a = poly->edges[i].a, b = poly->edges[i].b;
    ipoint_t pa = project(&poly->transformed[a]);
    ipoint_t pb = project(&poly->transformed[b]);
    canvas_draw_line(cnv, ox + pa.x, oy + pa.y, ox + pb.x, oy + pb.y,
                     edge_color);
  }
  free(mask);

  /* vertices (always) */
  // for (size_t i = 0; i < poly->count; i++)
  // {
  // 	ipoint_t pi = project(&poly->transformed[i]);
  // 	canvas_draw_line(cnv, ox + pi.x - 2, oy + pi.y,
  // 			 ox + pi.x + 2, oy + pi.y, vertex_color);
  // 	canvas_draw_line(cnv, ox + pi.x, oy + pi.y - 2,
  // 			 ox + pi.x, oy + pi.y + 2, vertex_color);
  // }
}

/*
   Shape factory — now passes face_count to create()
 */
static polyhedron3d_t *make_poly(const point3d_t *verts, size_t nv,
                                 const edge_t *edges, size_t ne,
                                 const face_t *faces, size_t nf, float r,
                                 float g, float b) {
  polyhedron3d_t *poly = polyhedron3d_create(nv, ne, nf);
  if (!poly)
    return NULL;

  memcpy(poly->vertices, verts, nv * sizeof(point3d_t));
  memcpy(poly->edges, edges, ne * sizeof(edge_t));
  if (nf && faces)
    memcpy(poly->faces, faces, nf * sizeof(face_t));

  poly->color_r = r;
  poly->color_g = g;
  poly->color_b = b;
  polyhedron3d_reset(poly);
  return poly;
}

/*
   Global state (unchanged from original)
 */
static canvas_t *g_cnv = NULL;
static window_t *g_win = NULL;
static polyhedron3d_t *g_poly = NULL;

static int g_origin_x = 290, g_origin_y = 240, g_dirty = 1;

static input_t *g_inp_sx, *g_inp_sy, *g_inp_sz;
static input_t *g_inp_ax, *g_inp_ay, *g_inp_az;
static input_t *g_inp_speed;
static input_t *g_inp_tx, *g_inp_ty, *g_inp_tz;

static int g_animating = 0;
static button_t *g_btn_playpause = NULL;

static void set_poly(polyhedron3d_t *next) {
  if (g_poly)
    polyhedron3d_destroy(g_poly);
  g_poly = next;
  g_dirty = 1;
}

/*
   Shape callbacks
 */
static void on_cube(void) {
  set_poly(make_poly(CUBE_VERTS, ARRAY_SIZE(CUBE_VERTS), CUBE_EDGES,
                     ARRAY_SIZE(CUBE_EDGES), CUBE_FACES, ARRAY_SIZE(CUBE_FACES),
                     0.2f, 0.8f, 1.0f));
}
static void on_tetrahedron(void) {
  set_poly(make_poly(TETRA_VERTS, ARRAY_SIZE(TETRA_VERTS), TETRA_EDGES,
                     ARRAY_SIZE(TETRA_EDGES), TETRA_FACES,
                     ARRAY_SIZE(TETRA_FACES), 1.0f, 0.2f, 0.8f));
}
static void on_octahedron(void) {
  set_poly(make_poly(OCTA_VERTS, ARRAY_SIZE(OCTA_VERTS), OCTA_EDGES,
                     ARRAY_SIZE(OCTA_EDGES), OCTA_FACES, ARRAY_SIZE(OCTA_FACES),
                     0.8f, 1.0f, 0.2f));
}
static void on_star(void) {
  set_poly(make_poly(STAR_VERTS, ARRAY_SIZE(STAR_VERTS), STAR_EDGES,
                     ARRAY_SIZE(STAR_EDGES), STAR_FACES, ARRAY_SIZE(STAR_FACES),
                     1.0f, 0.9f, 0.2f));
}
static void on_human(void) {
  set_poly(make_poly(HUMAN_VERTS, ARRAY_SIZE(HUMAN_VERTS), HUMAN_EDGES,
                     ARRAY_SIZE(HUMAN_EDGES), NULL, 0, 0.9f, 0.2f, 1.0f));
}
static void on_destroy(void) { set_poly(NULL); }
static void on_reset(void) {
  if (g_poly) {
    polyhedron3d_reset(g_poly);
    g_dirty = 1;
  }
}
static void on_playpause(void) {
  g_animating = !g_animating;
  if (g_btn_playpause)
    element_set_text(g_btn_playpause, g_animating ? "Pause" : "Play");
}

/* Transform callbacks (unchanged) */
static void on_scale(void) {
  if (!g_poly)
    return;
  float sx = atof(g_inp_sx->text);
  if (!sx)
    sx = 1;
  float sy = atof(g_inp_sy->text);
  if (!sy)
    sy = 1;
  float sz = atof(g_inp_sz->text);
  if (!sz)
    sz = 1;
  polyhedron3d_scale(g_poly, sx, sy, sz);
  g_dirty = 1;
}
static void on_rotate(void) {
  if (!g_poly)
    return;
  polyhedron3d_rotate_x(g_poly, atof(g_inp_ax->text));
  polyhedron3d_rotate_y(g_poly, atof(g_inp_ay->text));
  polyhedron3d_rotate_z(g_poly, atof(g_inp_az->text));
  g_dirty = 1;
}
static void on_translate(void) {
  if (!g_poly)
    return;
  polyhedron3d_translate(g_poly, atof(g_inp_tx->text), atof(g_inp_ty->text),
                         atof(g_inp_tz->text));
  g_dirty = 1;
}

/*
   Redraw
 */
static void redraw(void) {
  if (!g_cnv)
    return;
  canvas_clear(g_cnv, rgb(18, 18, 22));
  canvas_draw_line(g_cnv, 0, 0, g_cnv->width - 1, 0, rgb(60, 60, 70));
  canvas_draw_line(g_cnv, 0, g_cnv->height - 1, g_cnv->width - 1,
                   g_cnv->height - 1, rgb(60, 60, 70));
  canvas_draw_line(g_cnv, 0, 0, 0, g_cnv->height - 1, rgb(60, 60, 70));
  canvas_draw_line(g_cnv, g_cnv->width - 1, 0, g_cnv->width - 1,
                   g_cnv->height - 1, rgb(60, 60, 70));

  poly_draw(g_cnv, g_poly, g_origin_x, g_origin_y);

  if (g_win)
    object_flush(g_win->surface);
}

/*
   Init / Render (UI layout unchanged from original)
 */
void geometry3d_app_init(void) {
  g_win = window_create(60, 40, 820, 720);
  g_cnv = canvas_create(10, 10, 580, 480);
  window_addElement(g_win, g_cnv);

  int bx = 610, by = 20;
  button_t *btn_cube = button_create(bx, by, 110, 28, "Cube");
  button_t *btn_tet = button_create(bx, by + 35, 110, 28, "Tetrahedron");
  button_t *btn_oct = button_create(bx, by + 70, 110, 28, "Octahedron");
  button_t *btn_star = button_create(bx, by + 105, 110, 28, "Star");
  button_t *btn_hum = button_create(bx, by + 140, 110, 28, "Human");
  btn_cube->on_click = on_cube;
  btn_tet->on_click = on_tetrahedron;
  btn_oct->on_click = on_octahedron;
  btn_star->on_click = on_star;
  btn_hum->on_click = on_human;
  window_addElement(g_win, btn_cube);
  window_addElement(g_win, btn_tet);
  window_addElement(g_win, btn_oct);
  window_addElement(g_win, btn_star);
  window_addElement(g_win, btn_hum);

  button_t *btn_del = button_create(610, 206, 80, 26, "Destroy");
  button_t *btn_reset = button_create(700, 206, 70, 26, "Reset");
  btn_del->on_click = on_destroy;
  btn_reset->on_click = on_reset;
  window_addElement(g_win, btn_del);
  window_addElement(g_win, btn_reset);

  g_btn_playpause = button_create(610, 240, 80, 28, "Play");
  g_btn_playpause->on_click = on_playpause;
  window_addElement(g_win, g_btn_playpause);

  label_t *lbl_spd = label_create(700, 244, 40, 18, "spd:");
  g_inp_speed = input_create(725, 240, 45, 28, "1");
  window_addElement(g_win, lbl_spd);
  window_addElement(g_win, g_inp_speed);

  int iy = 290;
  window_addElement(g_win, label_create(610, iy, 100, 18, "Scale"));
  iy += 20;
  g_inp_sx = input_create(645, iy, 80, 24, "1");
  g_inp_sy = input_create(645, iy + 28, 80, 24, "1");
  g_inp_sz = input_create(645, iy + 56, 80, 24, "1");
  window_addElement(g_win, label_create(610, iy, 30, 18, "sx:"));
  window_addElement(g_win, g_inp_sx);
  window_addElement(g_win, label_create(610, iy + 28, 30, 18, "sy:"));
  window_addElement(g_win, g_inp_sy);
  window_addElement(g_win, label_create(610, iy + 56, 30, 18, "sz:"));
  window_addElement(g_win, g_inp_sz);
  button_t *btn_scale = button_create(645, iy + 84, 80, 26, "Scale");
  btn_scale->on_click = on_scale;
  window_addElement(g_win, btn_scale);

  iy += 120;
  window_addElement(g_win, label_create(610, iy, 100, 18, "Rotate"));
  iy += 20;
  g_inp_ax = input_create(648, iy, 77, 24, "0");
  g_inp_ay = input_create(648, iy + 28, 77, 24, "0");
  g_inp_az = input_create(648, iy + 56, 77, 24, "0");
  window_addElement(g_win, label_create(610, iy, 35, 18, "X:"));
  window_addElement(g_win, g_inp_ax);
  window_addElement(g_win, label_create(610, iy + 28, 35, 18, "Y:"));
  window_addElement(g_win, g_inp_ay);
  window_addElement(g_win, label_create(610, iy + 56, 35, 18, "Z:"));
  window_addElement(g_win, g_inp_az);
  button_t *btn_rotate = button_create(648, iy + 84, 77, 26, "Rotate");
  btn_rotate->on_click = on_rotate;
  window_addElement(g_win, btn_rotate);

  iy += 120;
  window_addElement(g_win, label_create(610, iy, 100, 18, "Translate"));
  iy += 20;
  g_inp_tx = input_create(648, iy, 77, 24, "0");
  g_inp_ty = input_create(648, iy + 28, 77, 24, "0");
  g_inp_tz = input_create(648, iy + 56, 77, 24, "0");
  window_addElement(g_win, label_create(610, iy, 35, 18, "X:"));
  window_addElement(g_win, g_inp_tx);
  window_addElement(g_win, label_create(610, iy + 28, 35, 18, "Y:"));
  window_addElement(g_win, g_inp_ty);
  window_addElement(g_win, label_create(610, iy + 56, 35, 18, "Z:"));
  window_addElement(g_win, g_inp_tz);
  button_t *btn_tr = button_create(648, iy + 84, 77, 26, "Translate");
  btn_tr->on_click = on_translate;
  window_addElement(g_win, btn_tr);

  on_cube();
}

void geometry3d_app_render(void) {
  if (g_animating && g_poly) {
    float speed = 1.0f;
    if (g_inp_speed) {
      float v = atof(g_inp_speed->text);
      if (v != 0.0f)
        speed = v;
    }
    polyhedron3d_rotate_z(g_poly, speed);
    g_dirty = 1;
  }
  if (!g_dirty)
    return;
  g_dirty = 0;
  redraw();
}
