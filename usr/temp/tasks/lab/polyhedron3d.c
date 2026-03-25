#include "polyhedron3d.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// lifecycle
polyhedron3d_t *polyhedron3d_create(size_t count, size_t edge_count)
{
	polyhedron3d_t *p = calloc(1, sizeof(polyhedron3d_t));
	if (!p)
		return NULL;

	p->count = count;
	p->edge_count = edge_count;

	p->vertices = calloc(count, sizeof(point3d_t));
	p->transformed = calloc(count, sizeof(point3d_t));
	p->edges = calloc(edge_count, sizeof(edge_t));

	if (!p->vertices || !p->transformed || !p->edges)
	{
		polyhedron3d_destroy(p);
		return NULL;
	}

	return p;
}

void polyhedron3d_destroy(polyhedron3d_t *p)
{
	if (!p)
		return;

	free(p->vertices);
	free(p->transformed);
	free(p->edges);
	free(p);
}

void polyhedron3d_reset(polyhedron3d_t *poly)
{
	if (!poly)
		return;
	memcpy(poly->transformed, poly->vertices,
	       poly->count * sizeof(point3d_t));
}

point3d_t polyhedron3d_centroid(const polyhedron3d_t *poly)
{
	point3d_t c = {0.0f, 0.0f, 0.0f};
	if (!poly || poly->count == 0)
		return c;

	for (size_t i = 0; i < poly->count; i++)
	{
		c.x += poly->transformed[i].x;
		c.y += poly->transformed[i].y;
		c.z += poly->transformed[i].z;
	}

	float inv = 1.0f / (float)poly->count;
	c.x *= inv;
	c.y *= inv;
	c.z *= inv;

	return c;
}

// geometric transforms

void polyhedron3d_scale(polyhedron3d_t *poly, float sx, float sy, float sz)
{
	if (!poly)
		return;

	point3d_t c = polyhedron3d_centroid(poly);

	for (size_t i = 0; i < poly->count; i++)
	{
		poly->transformed[i].x = c.x + (poly->transformed[i].x - c.x) * sx;
		poly->transformed[i].y = c.y + (poly->transformed[i].y - c.y) * sy;
		poly->transformed[i].z = c.z + (poly->transformed[i].z - c.z) * sz;
	}
}

void polyhedron3d_rotate_x(polyhedron3d_t *poly, float angle_deg)
{
	if (!poly)
		return;

	float rad = angle_deg * (float)M_PI / 180.0f;
	float cosA = cosf(rad);
	float sinA = sinf(rad);

	point3d_t c = polyhedron3d_centroid(poly);

	for (size_t i = 0; i < poly->count; i++)
	{
		float dy = poly->transformed[i].y - c.y;
		float dz = poly->transformed[i].z - c.z;

		poly->transformed[i].y = c.y + dy * cosA - dz * sinA;
		poly->transformed[i].z = c.z + dy * sinA + dz * cosA;
	}
}

void polyhedron3d_rotate_y(polyhedron3d_t *poly, float angle_deg)
{
	if (!poly)
		return;

	float rad = angle_deg * (float)M_PI / 180.0f;
	float cosA = cosf(rad);
	float sinA = sinf(rad);

	point3d_t c = polyhedron3d_centroid(poly);

	for (size_t i = 0; i < poly->count; i++)
	{
		float dx = poly->transformed[i].x - c.x;
		float dz = poly->transformed[i].z - c.z;

		poly->transformed[i].x = c.x + dx * cosA + dz * sinA;
		poly->transformed[i].z = c.z - dx * sinA + dz * cosA;
	}
}

void polyhedron3d_rotate_z(polyhedron3d_t *poly, float angle_deg)
{
	if (!poly)
		return;

	float rad = angle_deg * (float)M_PI / 180.0f;
	float cosA = cosf(rad);
	float sinA = sinf(rad);

	point3d_t c = polyhedron3d_centroid(poly);

	for (size_t i = 0; i < poly->count; i++)
	{
		float dx = poly->transformed[i].x - c.x;
		float dy = poly->transformed[i].y - c.y;

		poly->transformed[i].x = c.x + dx * cosA - dy * sinA;
		poly->transformed[i].y = c.y + dx * sinA + dy * cosA;
	}
}
