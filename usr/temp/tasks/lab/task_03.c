#include "tasks.h"

#include <libgui/core.h>
#include <libgui/ui.h>
#include <libgui/utils.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Colours
#define COL_BG rgb(18, 18, 28)
#define COL_GRID rgb(35, 35, 55)
#define COL_FRAME rgb(70, 70, 110)
#define COL_AXIS rgb(80, 120, 200)
#define COL_POINT_SRC rgb(80, 200, 120)
#define COL_POINT_DST rgb(240, 80, 80)
#define COL_CENTRE rgb(240, 180, 40)
#define COL_POLYGON rgb(100, 180, 255)
#define COL_HULL rgb(255, 140, 50)
#define COL_HULL_PT rgb(255, 220, 80)
#define COL_INNER_PT rgb(160, 160, 160)
#define COL_WHITE rgb(255, 255, 255)
#define COL_PANEL rgb(28, 28, 45)

// Globals
window_t *g_win_ = NULL;
canvas_t *g_cnv_ = NULL;
static int g_task = 1; // active task 1-4
static int g_dirty = 1;

// Canvas geometry
#define CNV_X 10
#define CNV_Y 10
#define CNV_W 565
#define CNV_H 480
#define CNV_CX (CNV_W / 2)
#define CNV_CY (CNV_H / 2)

static input_t *t1_px, *t1_py; // source point
static input_t *t1_cx, *t1_cy; // centre

static input_t *t2_px, *t2_py; // source point
static input_t *t2_ax, *t2_ay; // axis direction

static input_t *t3_pts_input; // "x1,y1;x2,y2;…"
static label_t *t3_result_lbl;

static input_t *t4_pts_input; // "x1,y1;x2,y2;…"

// Filled circle (radius r)
static void draw_circle(canvas_t *c, int cx, int cy, int r, uint32_t col) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r)
        canvas_set_pixel(c, cx + dx, cy + dy, col);
}

// Main axes
static void draw_axes(canvas_t *c) {
  canvas_draw_line(c, CNV_CX, 0, CNV_CX, CNV_H - 1, COL_AXIS);
  canvas_draw_line(c, 0, CNV_CY, CNV_W - 1, CNV_CY, COL_AXIS);
}

//  Arrow tip at (x2,y2) coming from direction (x1,y1)
static void draw_arrow(canvas_t *c, int x1, int y1, int x2, int y2,
                       uint32_t col) {
  canvas_draw_line(c, x1, y1, x2, y2, col);
  float ang = atan2f((float)(y2 - y1), (float)(x2 - x1));
  int al = 10;
  float a1 = ang + 2.5f, a2 = ang - 2.5f;
  canvas_draw_line(c, x2, y2, x2 - (int)(al * cosf(a1)),
                   y2 - (int)(al * sinf(a1)), col);
  canvas_draw_line(c, x2, y2, x2 - (int)(al * cosf(a2)),
                   y2 - (int)(al * sinf(a2)), col);
}

// World->canvas coordinate mapping (canvas coords, y flipped)
static int wx(float world) { return CNV_CX + (int)(world); }
static int wy(float world) { return CNV_CY - (int)(world); }

// Point parsing helpers
typedef struct {
  float x, y;
} pt2_t;

// Parse "x1,y1;x2,y2;…" -> array, returns count
static int parse_points(const char *s, pt2_t *out, int max_pts) {
  int n = 0;
  char buf[512];
  strncpy(buf, s, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *tok = strtok(buf, ";");
  while (tok && n < max_pts) {
    float x = 0, y = 0;
    if (sscanf(tok, "%f,%f", &x, &y) == 2) {
      out[n].x = x;
      out[n].y = y;
      n++;
    }
    tok = strtok(NULL, ";");
  }
  return n;
}

static void task1_draw(void) {
  float px = atof(t1_px->text), py = atof(t1_py->text);
  float cx = atof(t1_cx->text), cy = atof(t1_cy->text);

  // Symmetric point
  float rx = 2.0f * cx - px;
  float ry = 2.0f * cy - py;

  draw_axes(g_cnv_);

  canvas_draw_line(g_cnv_, wx(px), wy(py), wx(rx), wy(ry), rgb(120, 120, 180));

  // Centre dot
  int ccx = wx(cx), ccy = wy(cy);
  draw_circle(g_cnv_, ccx, ccy, 5, COL_CENTRE);

  // Source point P
  draw_circle(g_cnv_, wx(px), wy(py), 6, COL_POINT_SRC);

  // Result point P'
  draw_circle(g_cnv_, wx(rx), wy(ry), 6, COL_POINT_DST);
}

static void task2_draw(void) {
  float px = atof(t2_px->text), py = atof(t2_py->text);
  float ax = atof(t2_ax->text), ay = atof(t2_ay->text);

  // Normal to the axis direction (ax,ay): n = (-ay, ax)
  float A = -ay, B = ax;
  float denom = A * A + B * B;

  float t = (A * px + B * py) / denom;
  float rx = px - 2.0f * t * A;
  float ry = py - 2.0f * t * B;

  // Foot of perpendicular (= midpoint)
  float fx = (px + rx) * 0.5f;
  float fy = (py + ry) * 0.5f;

  draw_axes(g_cnv_);

  // Draw axis line across canvas
  // Parametric: (x,y) = foot + s*(ax,ay) for s in range
  float len = 3000.0f;
  float nrm = sqrtf(ax * ax + ay * ay);
  float ux = ax / nrm, uy = ay / nrm;
  int ax1 = wx(fx - ux * len), ay1 = wy(fy - uy * len);
  int ax2 = wx(fx + ux * len), ay2 = wy(fy + uy * len);
  canvas_draw_line(g_cnv_, ax1, ay1, ax2, ay2, COL_AXIS);

  canvas_draw_line(g_cnv_, wx(px), wy(py), wx(rx), wy(ry), rgb(120, 120, 180));

  // Foot of perpendicular
  draw_circle(g_cnv_, wx(fx), wy(fy), 4, COL_CENTRE);

  // Source point P
  draw_circle(g_cnv_, wx(px), wy(py), 6, COL_POINT_SRC);

  // Result point P'
  draw_circle(g_cnv_, wx(rx), wy(ry), 6, COL_POINT_DST);
}

static void task3_draw(void) {
  pt2_t pts[32];
  int n = parse_points(t3_pts_input->text, pts, 32);

  draw_axes(g_cnv_);

  if (n < 3) {
    element_set_text(t3_result_lbl, "Need >= 3 points");
    return;
  }

  // Draw polygon edges
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    canvas_draw_line(g_cnv_, wx(pts[i].x), wy(pts[i].y), wx(pts[j].x),
                     wy(pts[j].y), COL_POLYGON);
  }

  // Draw vertices
  for (int i = 0; i < n; i++) {
    uint32_t col = (i == 0) ? COL_CENTRE : COL_POLYGON;
    draw_circle(g_cnv_, wx(pts[i].x), wy(pts[i].y), 5, col);
  }

  // Draw direction arrows on each edge
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    int mx = (wx(pts[i].x) + wx(pts[j].x)) / 2;
    int my = (wy(pts[i].y) + wy(pts[j].y)) / 2;
    draw_arrow(g_cnv_, wx(pts[i].x), wy(pts[i].y), mx, my, COL_POLYGON);
  }

  // Convexity test
  // (pos - left, neg - right)
  int pos = 0, neg = 0;
  for (int i = 0; i < n; i++) {
    int a = i, b = (i + 1) % n, c = (i + 2) % n;
    float cross = (pts[b].x - pts[a].x) * (pts[c].y - pts[b].y) -
                  (pts[b].y - pts[a].y) * (pts[c].x - pts[b].x);
    if (cross > 0)
      pos++;
    else if (cross < 0)
      neg++;
  }

  if (pos > 0 && neg > 0)
    element_set_text(t3_result_lbl, "non-convex");
  else if (pos > 0)
    element_set_text(t3_result_lbl, "counter-clockwise");
  else if (neg > 0)
    element_set_text(t3_result_lbl, "clockwise");
  else
    element_set_text(t3_result_lbl, "Degenerate polygon");

  element_redraw(t3_result_lbl);
}

// helpers for task 4

static float crossTo(pt2_t O, pt2_t A, pt2_t B) {
  return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

static int cmp_pts(const void *a, const void *b) {
  const pt2_t *pa = (const pt2_t *)a;
  const pt2_t *pb = (const pt2_t *)b;
  if (pa->x != pb->x)
    return (pa->x < pb->x) ? -1 : 1;
  if (pa->y != pb->y)
    return (pa->y < pb->y) ? -1 : 1;
  return 0;
}

// Returns hull size; result stored in hull[]
static int convex_hull(pt2_t *pts, int n, pt2_t *hull) {
  if (n < 1)
    return 0;
  qsort(pts, n, sizeof(pt2_t), cmp_pts);

  int k = 0;
  // Lower hull
  for (int i = 0; i < n; i++) {
    while (k >= 2 && crossTo(hull[k - 2], hull[k - 1], pts[i]) < 0) //
      k--;
    hull[k++] = pts[i];
  }
  // Upper hull
  for (int i = n - 2, t = k + 1; i >= 0; i--) {
    while (k >= t && crossTo(hull[k - 2], hull[k - 1], pts[i]) < 0)
      k--;
    hull[k++] = pts[i];
  }
  return k - 1;
}

static void task4_draw(void) {
  pt2_t pts[64], hull[66];
  int n = parse_points(t4_pts_input->text, pts, 64);

  draw_axes(g_cnv_);

  if (n < 1)
    return;

  // All input points
  for (int i = 0; i < n; i++)
    draw_circle(g_cnv_, wx(pts[i].x), wy(pts[i].y), 3, COL_INNER_PT);

  if (n < 2)
    return;

  int hn = convex_hull(pts, n, hull);

  // Hull edges
  for (int i = 0; i < hn; i++) {
    int j = (i + 1) % hn;
    canvas_draw_line(g_cnv_, wx(hull[i].x), wy(hull[i].y), wx(hull[j].x),
                     wy(hull[j].y), COL_HULL);
  }

  // Hull vertices
  for (int i = 0; i < hn; i++)
    draw_circle(g_cnv_, wx(hull[i].x), wy(hull[i].y), 3, COL_HULL_PT);
}

// Redraw dispatcher
static void redraw(void) {
  if (!g_cnv_)
    return;
  canvas_clear(g_cnv_, COL_BG);

  switch (g_task) {
  case 1:
    task1_draw();
    break;
  case 2:
    task2_draw();
    break;
  case 3:
    task3_draw();
    break;
  case 4:
    task4_draw();
    break;
  }

  if (g_win_)
    object_flush(g_win_->surface);
}

// Task-1
static label_t *t1_lbl_px, *t1_lbl_py, *t1_lbl_cx, *t1_lbl_cy;
static button_t *t1_btn;

// Task-2
static label_t *t2_lbl_px, *t2_lbl_py;
static label_t *t2_lbl_ax, *t2_lbl_ay;
static button_t *t2_btn;

// Task-3
static label_t *t3_lbl_pts;
static button_t *t3_btn;

// Task-4
static label_t *t4_lbl_pts;
static button_t *t4_btn;

// Menu buttons
static button_t *menu_btn[4];

#define PANEL_X 590
#define OFF_X -2000

// Button callbacks
static void on_menu(int t) {
  g_task = t;
  g_dirty = 1;
}
static void on_menu1(void) { on_menu(1); }
static void on_menu2(void) { on_menu(2); }
static void on_menu3(void) { on_menu(3); }
static void on_menu4(void) { on_menu(4); }

static void on_apply(void) { g_dirty = 1; }

void app_init(void) {
  // Main window
  g_win_ = window_create(60, 40, 800, 580);

  // Drawing canvas
  g_cnv_ = canvas_create(CNV_X, CNV_Y, CNV_W, CNV_H);
  window_addElement(g_win_, g_cnv_);

  // Menu
  const char *menu_labels[] = {"1.Centre Sym", "2.Axis Sym", "3.Convexity",
                               "4.Hull"};
  void (*menu_cbs[])(void) = {on_menu1, on_menu2, on_menu3, on_menu4};
  for (int i = 0; i < 4; i++) {
    menu_btn[i] =
        button_create(PANEL_X, 10 + i * 38, 175, 30, (char *)menu_labels[i]);
    menu_btn[i]->on_click = menu_cbs[i];
    window_addElement(g_win_, menu_btn[i]);
  }

  int py = 175; // start y for task panels

  // Task-1 panel
  t1_lbl_px = label_create(PANEL_X, py, 30, 20, "Px:");
  t1_px = input_create(PANEL_X + 32, py, 70, 22, "80");
  t1_lbl_py = label_create(PANEL_X + 108, py, 30, 20, "Py:");
  t1_py = input_create(PANEL_X + 140, py, 30, 22, "60");
  window_addElement(g_win_, t1_lbl_px);
  window_addElement(g_win_, t1_px);
  window_addElement(g_win_, t1_lbl_py);
  window_addElement(g_win_, t1_py);

  py += 28;
  t1_lbl_cx = label_create(PANEL_X, py, 30, 20, "Cx:");
  t1_cx = input_create(PANEL_X + 32, py, 70, 22, "0");
  t1_lbl_cy = label_create(PANEL_X + 108, py, 30, 20, "Cy:");
  t1_cy = input_create(PANEL_X + 140, py, 30, 22, "0");
  window_addElement(g_win_, t1_lbl_cx);
  window_addElement(g_win_, t1_cx);
  window_addElement(g_win_, t1_lbl_cy);
  window_addElement(g_win_, t1_cy);

  py += 32;
  t1_btn = button_create(PANEL_X, py, 175, 28, "Apply");
  t1_btn->on_click = on_apply;
  window_addElement(g_win_, t1_btn);

  // Task-2 panel
  py += 40;
  t2_lbl_px = label_create(PANEL_X, py, 30, 20, "Px:");
  t2_px = input_create(PANEL_X + 32, py, 60, 22, "100");
  t2_lbl_py = label_create(PANEL_X + 98, py, 30, 20, "Py:");
  t2_py = input_create(PANEL_X + 130, py, 40, 22, "80");
  window_addElement(g_win_, t2_lbl_px);
  window_addElement(g_win_, t2_px);
  window_addElement(g_win_, t2_lbl_py);
  window_addElement(g_win_, t2_py);

  py += 26;
  t2_lbl_ax = label_create(PANEL_X, py, 30, 20, "ax:");
  t2_ax = input_create(PANEL_X + 32, py, 45, 22, "1");
  t2_lbl_ay = label_create(PANEL_X + 82, py, 30, 20, "ay:");
  t2_ay = input_create(PANEL_X + 114, py, 45, 22, "1");
  window_addElement(g_win_, t2_lbl_ax);
  window_addElement(g_win_, t2_ax);
  window_addElement(g_win_, t2_lbl_ay);
  window_addElement(g_win_, t2_ay);

  py += 32;
  t2_btn = button_create(PANEL_X, py, 175, 28, "Apply");
  t2_btn->on_click = on_apply;
  window_addElement(g_win_, t2_btn);

  // Task-3 panel
  py += 40;
  t3_lbl_pts = label_create(PANEL_X, py, 175, 20, "Pts (x,y;x,y;...):");
  window_addElement(g_win_, t3_lbl_pts);
  py += 22;
  t3_pts_input =
      input_create(PANEL_X - 75, py, 175 + 75, 22, "0,80;80,-40;-80,-40");
  window_addElement(g_win_, t3_pts_input);
  py += 28;
  t3_btn = button_create(PANEL_X, py, 175, 28, "Check");
  t3_btn->on_click = on_apply;
  window_addElement(g_win_, t3_btn);
  py += 32;
  t3_result_lbl = label_create(PANEL_X, py, 175, 22, "");
  window_addElement(g_win_, t3_result_lbl);

  // Task-4 panel
  py += 36;
  t4_lbl_pts = label_create(PANEL_X, py, 175, 20, "Pts (x,y;x,y;...):");
  window_addElement(g_win_, t4_lbl_pts);
  py += 22;
  t4_pts_input =
      input_create(PANEL_X - 75, py, 175 + 75, 22, "9,9;-9,9;9,-9;-9,-9");
  window_addElement(g_win_, t4_pts_input);
  py += 28;
  t4_btn = button_create(PANEL_X, py, 175, 28, "Build Hull");
  t4_btn->on_click = on_apply;
  window_addElement(g_win_, t4_btn);
}

void app_render(void) {
  if (!g_dirty)
    return;
  g_dirty = 0;
  redraw();
}
