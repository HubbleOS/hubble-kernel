#pragma once

#include <stddef.h>

// 3D point
typedef struct
{
	float x, y, z;
} point3d_t;

// edges
typedef struct
{
	int a, b;
} edge_t;

// Polyhedron: vertices + adjacency matrix for wireframe
typedef struct
{
	size_t count;		// number of vertices
	point3d_t *vertices;	// array of original vertices
	point3d_t *transformed; // working copy after transforms
	// int **adj;		// adjacency matrix [count x count]

	edge_t *edges;
	size_t edge_count;

	// color
	float color_r, color_g, color_b;
} polyhedron3d_t;

// lifecycle
polyhedron3d_t *polyhedron3d_create(size_t count, size_t edge_count);
void polyhedron3d_destroy(polyhedron3d_t *poly);

// reset transformed vertices from original
void polyhedron3d_reset(polyhedron3d_t *poly);

// compute centroid of transformed vertices
point3d_t polyhedron3d_centroid(const polyhedron3d_t *poly);

// geometric transforms (applied to transformed vertices, around centroid)
void polyhedron3d_scale(polyhedron3d_t *poly, float sx, float sy, float sz);
void polyhedron3d_rotate_x(polyhedron3d_t *poly, float angle_deg);
void polyhedron3d_rotate_y(polyhedron3d_t *poly, float angle_deg);
void polyhedron3d_rotate_z(polyhedron3d_t *poly, float angle_deg);
void polyhedron3d_translate(polyhedron3d_t *poly, float tx, float ty, float tz);
