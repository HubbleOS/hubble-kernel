#pragma once

#include <stddef.h>

// 3-D point (float)
typedef struct {
  float x, y, z;
} pt3_t;

// Projected 2-D point (integer screen coords)
typedef struct {
  int x, y;
} pt2_t;

// Surface grid
//   nx * ny vertices, stored row-major: [iy * nx + ix]
typedef struct {
  float x0, x1; /* domain [x0,x1] */
  float y0, y1; /* domain [y0,y1] */
  int nx, ny;   /* grid resolution (cells = nx-1 by ny-1) */

  pt3_t *pts; /* nx*ny world points */
  pt2_t *scr; /* nx*ny projected screen points */
} surface_t;

// Projection parameters (oblique frontal isometry)
//   X  -> screen right
//   Z  -> screen up
//   Y  -> oblique at angle alpha, factor k (default k=0.5, alpha=45°)
typedef struct {
  float scale;     /* uniform world->screen scale           */
  float k;         /* oblique axis shortening (0.5 typical) */
  float alpha_deg; /* oblique axis angle in degrees (45)    */
  int ox, oy;      /* screen origin (centre of canvas)      */
} proj_t;

// API

/* Allocate grid; does NOT fill pts yet */
surface_t *surface_create(int nx, int ny, float x0, float x1, float y0,
                          float y1);
void surface_destroy(surface_t *s);

/* Evaluate z = f(x,y) for every grid vertex */
void surface_eval(surface_t *s);

/* Project all world pts -> scr using proj */
void surface_project(surface_t *s, const proj_t *p);

/* Project a single point */
pt2_t proj_apply(const proj_t *p, pt3_t w);

/* Draw the surface with floating-horizon hidden-line removal.
   canvas_draw_line_fn  – pointer to your canvas line-drawing function.
   fill_cell_fn         – pointer to your canvas fill-quad function (may be
   NULL). userdata             – passed through to both callbacks. wire_color –
   colour for wireframe lines. fill_color           – colour for cell fill
   (ignored if fill_cell_fn == NULL). bg_color             – background colour
   used to "erase" hidden lines.     */
typedef void (*draw_line_fn)(void *ud, int x0, int y0, int x1, int y1,
                             unsigned color);
typedef void (*fill_quad_fn)(void *ud, int x[4], int y[4], unsigned color);

void surface_draw(const surface_t *s, draw_line_fn draw_line,
                  fill_quad_fn fill_quad, void *userdata, unsigned wire_color,
                  unsigned fill_color, int canvas_w);

/* Draw XYZ axes in oblique frontal isometry */
void surface_draw_axes(const proj_t *p, float len, draw_line_fn draw_line,
                       void *userdata, unsigned color);
