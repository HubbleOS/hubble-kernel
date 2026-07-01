#include "surface.h"
#include "tasks.h"

#include <libgui/core.h>
#include <libgui/ui.h>
#include <libgui/utils.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
   Global state
 */
static canvas_t *g_cnv = NULL;
static window_t *g_win = NULL;
static surface_t *g_surf = NULL;
static proj_t g_proj;
static int g_dirty = 1;

/* UI inputs */
static input_t *g_inp_x0, *g_inp_x1; /* domain X          */
static input_t *g_inp_y0, *g_inp_y1; /* domain Y          */
static input_t *g_inp_nx, *g_inp_ny; /* grid resolution   */
static input_t *g_inp_scale;         /* world→screen scale*/
static input_t *g_inp_k;             /* oblique factor    */
static input_t *g_inp_alpha;         /* oblique angle     */
static input_t *g_inp_ox, *g_inp_oy; /* screen origin     */

/*
   Callbacks for surface_draw  (bridge to your canvas API)
 */
static void cb_draw_line(void *ud, int x0, int y0, int x1, int y1,
                         unsigned color) {
  canvas_t *c = (canvas_t *)ud;
  canvas_draw_line(c, x0, y0, x1, y1, color);
}

static void cb_fill_quad(void *ud, int x[4], int y[4], unsigned color) {
  canvas_t *c = (canvas_t *)ud;
  /* Most minimal fill: scan-line between top and bottom of the quad.
     If your framework exposes canvas_fill_polygon use that instead. */
  // canvas_fill_polygon(c, x, y, 4, color);
}

/*
   Read proj params from UI
 */
static void read_proj(void) {
  g_proj.scale = (float)atof(g_inp_scale->text);
  g_proj.k = (float)atof(g_inp_k->text);
  g_proj.alpha_deg = (float)atof(g_inp_alpha->text);
  g_proj.ox = atoi(g_inp_ox->text);
  g_proj.oy = atoi(g_inp_oy->text);

  if (g_proj.scale == 0.0f)
    g_proj.scale = 1.0f;
  if (g_proj.k == 0.0f)
    g_proj.k = 0.5f;
}

/*
   Rebuild surface from UI parameters
 */
static void rebuild(void) {
  float x0 = (float)atof(g_inp_x0->text);
  float x1 = (float)atof(g_inp_x1->text);
  float y0 = (float)atof(g_inp_y0->text);
  float y1 = (float)atof(g_inp_y1->text);
  int nx = atoi(g_inp_nx->text);
  int ny = atoi(g_inp_ny->text);

  if (x1 <= x0)
    x1 = x0 + 1.0f;
  if (y1 <= y0)
    y1 = y0 + 1.0f;
  if (nx < 2)
    nx = 2;
  if (ny < 2)
    ny = 2;

  surface_destroy(g_surf);
  g_surf = surface_create(nx, ny, x0, x1, y0, y1);
  if (!g_surf)
    return;

  surface_eval(g_surf);

  read_proj();
  surface_project(g_surf, &g_proj);

  g_dirty = 1;
}

/*
   Redraw canvas
 */
static void redraw(void) {
  if (!g_cnv)
    return;

  canvas_clear(g_cnv, rgb(18, 18, 22));

  /* frame */
  canvas_draw_line(g_cnv, 0, 0, g_cnv->width - 1, 0, rgb(60, 60, 70));
  canvas_draw_line(g_cnv, 0, g_cnv->height - 1, g_cnv->width - 1,
                   g_cnv->height - 1, rgb(60, 60, 70));
  canvas_draw_line(g_cnv, 0, 0, 0, g_cnv->height - 1, rgb(60, 60, 70));
  canvas_draw_line(g_cnv, g_cnv->width - 1, 0, g_cnv->width - 1,
                   g_cnv->height - 1, rgb(60, 60, 70));

  /* axes – white */
  surface_draw_axes(&g_proj, 60.0f, cb_draw_line, g_cnv, rgb(220, 220, 220));

  if (g_surf) {
    /* fill colour: dark teal; wire colour: cyan */
    surface_draw(g_surf, cb_draw_line, cb_fill_quad, g_cnv,
                 rgb(80, 220, 255), /* wireframe */
                 rgb(20, 60, 80),   /* cell fill  */
                 g_cnv->width);
  }

  if (g_win)
    object_flush(g_win->surface);
}

/*
   Button callbacks
 */
static void on_draw(void) {
  rebuild();
  redraw();
}

static void on_reset(void) {
  /* restore input defaults */
  element_set_text(g_inp_x0, "-2");
  element_set_text(g_inp_x1, "2");
  element_set_text(g_inp_y0, "-2");
  element_set_text(g_inp_y1, "2");
  element_set_text(g_inp_nx, "20");
  element_set_text(g_inp_ny, "20");
  element_set_text(g_inp_scale, "40");
  element_set_text(g_inp_k, "0.5");
  element_set_text(g_inp_alpha, "45");
  element_set_text(g_inp_ox, "290");
  element_set_text(g_inp_oy, "400");
  rebuild();
  redraw();
}

/*
   Init
 */
void surface_app_init(void) {
  g_win = window_create(60, 40, 820, 720);

  /* canvas 580×480 */
  g_cnv = canvas_create(10, 10, 580, 480);
  window_addElement(g_win, g_cnv);

  /* ---- Right panel ---- */
  int px = 610, py = 20;

  /* Domain X */
  label_t *lbl_dom = label_create(px, py, 180, 18, "Domain");
  window_addElement(g_win, lbl_dom);
  py += 22;

  label_t *lbl_x0 = label_create(px, py, 30, 18, "x0:");
  g_inp_x0 = input_create(px + 32, py, 60, 24, "-2");
  label_t *lbl_x1 = label_create(px + 100, py, 30, 18, "x1:");
  g_inp_x1 = input_create(px + 132, py, 60, 24, "2");
  window_addElement(g_win, lbl_x0);
  window_addElement(g_win, g_inp_x0);
  window_addElement(g_win, lbl_x1);
  window_addElement(g_win, g_inp_x1);
  py += 30;

  label_t *lbl_y0 = label_create(px, py, 30, 18, "y0:");
  g_inp_y0 = input_create(px + 32, py, 60, 24, "-2");
  label_t *lbl_y1 = label_create(px + 100, py, 30, 18, "y1:");
  g_inp_y1 = input_create(px + 132, py, 60, 24, "2");
  window_addElement(g_win, lbl_y0);
  window_addElement(g_win, g_inp_y0);
  window_addElement(g_win, lbl_y1);
  window_addElement(g_win, g_inp_y1);
  py += 36;

  /* Grid */
  label_t *lbl_grid = label_create(px, py, 180, 18, "Grid resolution");
  window_addElement(g_win, lbl_grid);
  py += 22;

  label_t *lbl_nx = label_create(px, py, 30, 18, "nx:");
  g_inp_nx = input_create(px + 32, py, 60, 24, "20");
  label_t *lbl_ny = label_create(px + 100, py, 30, 18, "ny:");
  g_inp_ny = input_create(px + 132, py, 60, 24, "20");
  window_addElement(g_win, lbl_nx);
  window_addElement(g_win, g_inp_nx);
  window_addElement(g_win, lbl_ny);
  window_addElement(g_win, g_inp_ny);
  py += 36;

  /* Projection */
  label_t *lbl_proj = label_create(px, py, 180, 18, "Projection");
  window_addElement(g_win, lbl_proj);
  py += 22;

  label_t *lbl_sc = label_create(px, py, 45, 18, "scale:");
  g_inp_scale = input_create(px + 50, py, 60, 24, "40");
  window_addElement(g_win, lbl_sc);
  window_addElement(g_win, g_inp_scale);
  py += 30;

  label_t *lbl_k = label_create(px, py, 15, 18, "k:");
  g_inp_k = input_create(px + 20, py, 55, 24, "0.5");
  label_t *lbl_al = label_create(px + 85, py, 40, 18, "ang:");
  g_inp_alpha = input_create(px + 128, py, 55, 24, "45");
  // window_addElement(g_win, lbl_k);
  // window_addElement(g_win, g_inp_k);
  // window_addElement(g_win, lbl_al);
  // window_addElement(g_win, g_inp_alpha);
  // py += 30;

  label_t *lbl_ox = label_create(px, py, 30, 18, "ox:");
  g_inp_ox = input_create(px + 32, py, 55, 24, "290");
  label_t *lbl_oy = label_create(px + 95, py, 30, 18, "oy:");
  g_inp_oy = input_create(px + 128, py, 55, 24, "400");
  window_addElement(g_win, lbl_ox);
  window_addElement(g_win, g_inp_ox);
  window_addElement(g_win, lbl_oy);
  window_addElement(g_win, g_inp_oy);
  py += 42;

  /* Buttons */
  button_t *btn_draw = button_create(px, py, 90, 30, "Draw");
  button_t *btn_reset = button_create(px + 100, py, 90, 30, "Reset");
  btn_draw->on_click = on_draw;
  btn_reset->on_click = on_reset;
  window_addElement(g_win, btn_draw);
  window_addElement(g_win, btn_reset);

  /* initial draw */
  on_reset();
}

/*
   Render loop (called every frame)
 */
void surface_app_render(void) {
  if (!g_dirty)
    return;
  g_dirty = 0;
  redraw();
}
