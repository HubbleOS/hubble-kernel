#pragma once

#include <stddef.h>

typedef struct {
  float x, y, z;
} point3d_t;

typedef struct {
  int a, b;
} edge_t;

/* Triangular face (indices into vertices[]) */
typedef struct {
  int a, b, c;
} face_t;

typedef struct {
  point3d_t *vertices;
  point3d_t *transformed;
  size_t count;

  edge_t *edges;
  size_t edge_count;

  face_t *faces;
  size_t face_count;

  float color_r, color_g, color_b;
} polyhedron3d_t;

polyhedron3d_t *polyhedron3d_create(size_t count, size_t edge_count,
                                    size_t face_count);
void polyhedron3d_destroy(polyhedron3d_t *p);
void polyhedron3d_reset(polyhedron3d_t *p);

point3d_t polyhedron3d_centroid(const polyhedron3d_t *p);

void polyhedron3d_scale(polyhedron3d_t *p, float sx, float sy, float sz);
void polyhedron3d_rotate_x(polyhedron3d_t *p, float angle_deg);
void polyhedron3d_rotate_y(polyhedron3d_t *p, float angle_deg);
void polyhedron3d_rotate_z(polyhedron3d_t *p, float angle_deg);
void polyhedron3d_translate(polyhedron3d_t *p, float tx, float ty, float tz);
