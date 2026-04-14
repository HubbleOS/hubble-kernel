#include "polyhedron3d.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct
{
	float m[4][4];
} mat4_t;

static mat4_t mat4_identity(void)
{
	mat4_t r = {{{0}}};
	r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
	return r;
}

static mat4_t mat4_mul(const mat4_t *a, const mat4_t *b)
{
	mat4_t c = {{{0}}};
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			for (int k = 0; k < 4; k++)
				c.m[i][j] += a->m[i][k] * b->m[k][j];
	return c;
}

static point3d_t mat4_apply(const mat4_t *m, point3d_t p)
{
	float x = m->m[0][0] * p.x + m->m[0][1] * p.y + m->m[0][2] * p.z + m->m[0][3];
	float y = m->m[1][0] * p.x + m->m[1][1] * p.y + m->m[1][2] * p.z + m->m[1][3];
	float z = m->m[2][0] * p.x + m->m[2][1] * p.y + m->m[2][2] * p.z + m->m[2][3];
	float w = m->m[3][0] * p.x + m->m[3][1] * p.y + m->m[3][2] * p.z + m->m[3][3];
	if (w != 0.0f && w != 1.0f)
	{
		x /= w;
		y /= w;
		z /= w;
	}
	return (point3d_t){x, y, z};
}

static mat4_t mat4_translate(float tx, float ty, float tz)
{
	mat4_t m = mat4_identity();
	m.m[0][3] = tx;
	m.m[1][3] = ty;
	m.m[2][3] = tz;
	return m;
}

static mat4_t mat4_scale(float sx, float sy, float sz)
{
	mat4_t m = mat4_identity();
	m.m[0][0] = sx;
	m.m[1][1] = sy;
	m.m[2][2] = sz;
	return m;
}

static mat4_t mat4_rot_x(float rad)
{
	mat4_t m = mat4_identity();
	float c = cosf(rad), s = sinf(rad);
	m.m[1][1] = c;
	m.m[1][2] = -s;
	m.m[2][1] = s;
	m.m[2][2] = c;
	return m;
}

static mat4_t mat4_rot_y(float rad)
{
	mat4_t m = mat4_identity();
	float c = cosf(rad), s = sinf(rad);
	m.m[0][0] = c;
	m.m[0][2] = s;
	m.m[2][0] = -s;
	m.m[2][2] = c;
	return m;
}

static mat4_t mat4_rot_z(float rad)
{
	mat4_t m = mat4_identity();
	float c = cosf(rad), s = sinf(rad);
	m.m[0][0] = c;
	m.m[0][1] = -s;
	m.m[1][0] = s;
	m.m[1][1] = c;
	return m;
}

static void apply_to_all(polyhedron3d_t *poly, const mat4_t *m)
{
	for (size_t i = 0; i < poly->count; i++)
		poly->transformed[i] = mat4_apply(m, poly->transformed[i]);
}

polyhedron3d_t *polyhedron3d_create(size_t count, size_t edge_count,
				    size_t face_count)
{
	polyhedron3d_t *p = calloc(1, sizeof(polyhedron3d_t));
	if (!p)
		return NULL;

	p->count = count;
	p->edge_count = edge_count;
	p->face_count = face_count;

	p->vertices = calloc(count, sizeof(point3d_t));
	p->transformed = calloc(count, sizeof(point3d_t));
	p->edges = calloc(edge_count, sizeof(edge_t));
	p->faces = face_count
		       ? calloc(face_count, sizeof(face_t))
		       : NULL;

	if (!p->vertices || !p->transformed || !p->edges ||
	    (face_count && !p->faces))
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
	free(p->faces);
	free(p);
}

void polyhedron3d_reset(polyhedron3d_t *poly)
{
	if (!poly)
		return;
	memcpy(poly->transformed, poly->vertices, poly->count * sizeof(point3d_t));
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

void polyhedron3d_scale(polyhedron3d_t *poly, float sx, float sy, float sz)
{
	if (!poly)
		return;
	point3d_t c = polyhedron3d_centroid(poly);

	mat4_t t_pos = mat4_translate(c.x, c.y, c.z);
	mat4_t t_neg = mat4_translate(-c.x, -c.y, -c.z);
	mat4_t s = mat4_scale(sx, sy, sz);

	mat4_t tmp = mat4_mul(&s, &t_neg);
	mat4_t m = mat4_mul(&t_pos, &tmp);
	apply_to_all(poly, &m);
}

void polyhedron3d_rotate_x(polyhedron3d_t *poly, float angle_deg)
{
	if (!poly)
		return;
	point3d_t c = polyhedron3d_centroid(poly);
	float rad = angle_deg * (float)M_PI / 180.0f;

	mat4_t t_pos = mat4_translate(c.x, c.y, c.z);
	mat4_t t_neg = mat4_translate(-c.x, -c.y, -c.z);
	mat4_t r = mat4_rot_x(rad);

	mat4_t tmp = mat4_mul(&r, &t_neg);
	mat4_t m = mat4_mul(&t_pos, &tmp);
	apply_to_all(poly, &m);
}

void polyhedron3d_rotate_y(polyhedron3d_t *poly, float angle_deg)
{
	if (!poly)
		return;
	point3d_t c = polyhedron3d_centroid(poly);
	float rad = angle_deg * (float)M_PI / 180.0f;

	mat4_t t_pos = mat4_translate(c.x, c.y, c.z);
	mat4_t t_neg = mat4_translate(-c.x, -c.y, -c.z);
	mat4_t r = mat4_rot_y(rad);

	mat4_t tmp = mat4_mul(&r, &t_neg);
	mat4_t m = mat4_mul(&t_pos, &tmp);
	apply_to_all(poly, &m);
}

void polyhedron3d_rotate_z(polyhedron3d_t *poly, float angle_deg)
{
	if (!poly)
		return;
	point3d_t c = polyhedron3d_centroid(poly);
	float rad = angle_deg * (float)M_PI / 180.0f;

	mat4_t t_pos = mat4_translate(c.x, c.y, c.z);
	mat4_t t_neg = mat4_translate(-c.x, -c.y, -c.z);
	mat4_t r = mat4_rot_z(rad);

	mat4_t tmp = mat4_mul(&r, &t_neg);
	mat4_t m = mat4_mul(&t_pos, &tmp);
	apply_to_all(poly, &m);
}

void polyhedron3d_translate(polyhedron3d_t *poly, float tx, float ty, float tz)
{
	if (!poly)
		return;
	mat4_t m = mat4_translate(tx, ty, tz);
	apply_to_all(poly, &m);
}
