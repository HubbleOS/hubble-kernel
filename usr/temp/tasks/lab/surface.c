#include "surface.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
   Helpers
    */
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

/*
   Allocation
    */
surface_t *surface_create(int nx, int ny, float x0, float x1, float y0,
                          float y1) {
  if (nx < 2 || ny < 2)
    return NULL;

  surface_t *s = calloc(1, sizeof(surface_t));
  if (!s)
    return NULL;

  s->nx = nx;
  s->ny = ny;
  s->x0 = x0;
  s->x1 = x1;
  s->y0 = y0;
  s->y1 = y1;

  s->pts = calloc((size_t)(nx * ny), sizeof(pt3_t));
  s->scr = calloc((size_t)(nx * ny), sizeof(pt2_t));

  if (!s->pts || !s->scr) {
    surface_destroy(s);
    return NULL;
  }
  return s;
}

void surface_destroy(surface_t *s) {
  if (!s)
    return;
  free(s->pts);
  free(s->scr);
  free(s);
}

/*
   Evaluation  z = x² + y²
    */
void surface_eval(surface_t *s) {
  if (!s)
    return;
  for (int iy = 0; iy < s->ny; iy++) {
    float y = s->y0 + (s->y1 - s->y0) * (float)iy / (float)(s->ny - 1);
    for (int ix = 0; ix < s->nx; ix++) {
      float x = s->x0 + (s->x1 - s->x0) * (float)ix / (float)(s->nx - 1);
      float z = x * x + y * y;
      s->pts[iy * s->nx + ix] = (pt3_t){x, y, z};
    }
  }
}

/*
   Projection  – oblique frontal isometry
     screen_x =  scale * (X + k*Y*cos(alpha))
     screen_y = -scale * (Z + k*Y*sin(alpha))   (Y up on screen)
    */
pt2_t proj_apply(const proj_t *p, pt3_t w) {
  float rad = (float)(p->alpha_deg * M_PI / 180.0);
  float kc = p->k * cosf(rad);
  float ks = p->k * sinf(rad);

  pt2_t out;
  out.x = p->ox + (int)(p->scale * (w.x + kc * w.y));
  out.y = p->oy - (int)(p->scale * (w.z + ks * w.y));
  return out;
}

void surface_project(surface_t *s, const proj_t *p) {
  if (!s || !p)
    return;
  int n = s->nx * s->ny;
  for (int i = 0; i < n; i++)
    s->scr[i] = proj_apply(p, s->pts[i]);
}

/*
   Floating-horizon hidden-line removal
   (Куфос / Rogerс algorithm)

   We scan surface rows from front (iy=0) to back (iy=ny-1).
   For each screen column x we maintain:
       horizon_top[x]    – highest (min) screen Y seen so far
       horizon_bot[x]    – lowest  (max) screen Y seen so far

   A segment is visible only outside both horizon bands.
   After drawing the visible parts we update the horizons.

   For fill: each cell quad is filled with bg colour BEFORE
   drawing edges — this hides lines behind it (painter's style
   combined with horizon tracking).
    */

/* Bresenham scan of a segment, updating horizon arrays.
   Returns nothing – side-effecting on top[]/bot[]. */
static void horizon_update(int *top, int *bot, int w, int x0, int y0, int x1,
                           int y1) {
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx - dy, x = x0, y = y0;

  for (;;) {
    if (x >= 0 && x < w) {
      if (y < top[x])
        top[x] = y;
      if (y > bot[x])
        bot[x] = y;
    }
    if (x == x1 && y == y1)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
}

/* Draw only the visible portions of segment (x0,y0)-(x1,y1),
   using the horizon arrays. */
static void horizon_draw_seg(int *top, int *bot, int w, int x0, int y0, int x1,
                             int y1, draw_line_fn draw, void *ud,
                             unsigned color) {
  /* Walk Bresenham; collect runs of visible pixels */
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx - dy, x = x0, y = y0;

  int run_x0 = -1, run_y0 = 0, prev_x = -1, prev_y = 0;

  for (;;) {
    int visible = 0;
    if (x >= 0 && x < w)
      visible = (y <= top[x] || y >= bot[x]);

    if (visible) {
      if (run_x0 < 0) {
        run_x0 = x;
        run_y0 = y;
      }
      prev_x = x;
      prev_y = y;
    } else {
      if (run_x0 >= 0) {
        draw(ud, run_x0, run_y0, prev_x, prev_y, color);
        run_x0 = -1;
      }
    }

    if (x == x1 && y == y1)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
  if (run_x0 >= 0)
    draw(ud, run_x0, run_y0, prev_x, prev_y, color);
}

void surface_draw(const surface_t *s, draw_line_fn draw_line,
                  fill_quad_fn fill_quad, void *userdata, unsigned wire_color,
                  unsigned fill_color, int canvas_w) {
  if (!s || !draw_line)
    return;

  int *top = malloc((size_t)canvas_w * sizeof(int));
  int *bot = malloc((size_t)canvas_w * sizeof(int));
  if (!top || !bot) {
    free(top);
    free(bot);
    return;
  }

  /* Initialise horizons to "nothing seen" */
  for (int i = 0; i < canvas_w; i++) {
    top[i] = INT_MAX;
    bot[i] = INT_MIN;
  }

  /* Draw front-to-back (iy=0 is front in our coord system:
     smaller Y world value = closer to viewer in oblique projection).
     Adjust loop direction if your domain orientation differs. */
  for (int iy = 0; iy < s->ny - 1; iy++) {
    for (int ix = 0; ix < s->nx - 1; ix++) {
      /* four corners of this cell */
      pt2_t p00 = s->scr[iy * s->nx + ix];
      pt2_t p10 = s->scr[iy * s->nx + ix + 1];
      pt2_t p01 = s->scr[(iy + 1) * s->nx + ix];
      pt2_t p11 = s->scr[(iy + 1) * s->nx + ix + 1];

      /* fill quad (painter's algorithm – hides lines behind) */
      if (fill_quad) {
        int qx[4] = {p00.x, p10.x, p11.x, p01.x};
        int qy[4] = {p00.y, p10.y, p11.y, p01.y};
        fill_quad(userdata, qx, qy, fill_color);
      }

      /* draw & update horizon for the 4 edges of the cell
         (shared edges between cells are drawn twice but that
         is acceptable; an optimised version would skip them) */

      /* top edge (iy row) */
      horizon_draw_seg(top, bot, canvas_w, p00.x, p00.y, p10.x, p10.y,
                       draw_line, userdata, wire_color);
      horizon_update(top, bot, canvas_w, p00.x, p00.y, p10.x, p10.y);

      /* left edge */
      horizon_draw_seg(top, bot, canvas_w, p00.x, p00.y, p01.x, p01.y,
                       draw_line, userdata, wire_color);
      horizon_update(top, bot, canvas_w, p00.x, p00.y, p01.x, p01.y);

      /* right edge */
      horizon_draw_seg(top, bot, canvas_w, p10.x, p10.y, p11.x, p11.y,
                       draw_line, userdata, wire_color);
      horizon_update(top, bot, canvas_w, p10.x, p10.y, p11.x, p11.y);

      /* bottom edge (iy+1 row) */
      horizon_draw_seg(top, bot, canvas_w, p01.x, p01.y, p11.x, p11.y,
                       draw_line, userdata, wire_color);
      horizon_update(top, bot, canvas_w, p01.x, p01.y, p11.x, p11.y);
    }
  }

  free(top);
  free(bot);
}

/*
   Axes
    */
void surface_draw_axes(const proj_t *p, float len, draw_line_fn draw_line,
                       void *ud, unsigned color) {
  pt3_t o = {0, 0, 0};
  pt3_t xa = {len, 0, 0};
  pt3_t ya = {0, len, 0};
  pt3_t za = {0, 0, len};

  pt2_t po = proj_apply(p, o);
  pt2_t pxa = proj_apply(p, xa);
  pt2_t pya = proj_apply(p, ya);
  pt2_t pza = proj_apply(p, za);

  draw_line(ud, po.x, po.y, pxa.x, pxa.y, color);
  draw_line(ud, po.x, po.y, pya.x, pya.y, color);
  draw_line(ud, po.x, po.y, pza.x, pza.y, color);
}
