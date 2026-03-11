#include "transform_object.h"

#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ─── створення / знищення ────────────────────────────────────────────────────

transform_object_t *transform_object_create(point_t *pts, size_t count)
{
	transform_object_t *obj = malloc(sizeof(transform_object_t));
	if (!obj)
		return NULL;

	obj->points = malloc(sizeof(point_t) * count);
	obj->original = malloc(sizeof(point_t) * count);
	if (!obj->points || !obj->original)
	{
		free(obj->points);
		free(obj->original);
		free(obj);
		return NULL;
	}

	for (size_t i = 0; i < count; i++)
	{
		obj->points[i] = pts[i];
		obj->original[i] = pts[i];
	}

	obj->count = count;
	obj->color_r = 1.0f;
	obj->color_g = 1.0f;
	obj->color_b = 1.0f;

	obj->angle = 0.0f;
	obj->scale_x = 1.0f;
	obj->scale_y = 1.0f;
	obj->offset_x = 0.0f;
	obj->offset_y = 0.0f;

	return obj;
}

void transform_object_destroy(transform_object_t *obj)
{
	if (!obj)
		return;
	free(obj->points);
	free(obj->original);
	free(obj);
}

// ─── внутрішній перерахунок від original ─────────────────────────────────────
// Порядок: scale → rotate → translate
// Всі операції відносно центру оригіналу

static void transform_apply(transform_object_t *obj)
{
	// центр оригіналу
	float cx = 0.0f, cy = 0.0f;
	for (size_t i = 0; i < obj->count; i++)
	{
		cx += obj->original[i].x;
		cy += obj->original[i].y;
	}
	cx /= (float)obj->count;
	cy /= (float)obj->count;

	float rad = obj->angle * (float)M_PI / 180.0f;
	float c = cosf(rad);
	float s = sinf(rad);

	for (size_t i = 0; i < obj->count; i++)
	{
		// 1. масштаб відносно центру
		float x = cx + (obj->original[i].x - cx) * obj->scale_x;
		float y = cy + (obj->original[i].y - cy) * obj->scale_y;

		// 2. поворот відносно центру
		float dx = x - cx;
		float dy = y - cy;
		x = cx + dx * c - dy * s;
		y = cy + dx * s + dy * c;

		// 3. зсув
		obj->points[i].x = x + obj->offset_x;
		obj->points[i].y = y + obj->offset_y;
	}
}

// ─── звичайні координати ─────────────────────────────────────────────────────

void transform_translate(transform_object_t *obj, float dx, float dy)
{
	obj->offset_x += dx;
	obj->offset_y += dy;
	transform_apply(obj);
}

void transform_scale(transform_object_t *obj, float sx, float sy)
{
	obj->scale_x = sx;
	obj->scale_y = sy;
	transform_apply(obj);
}

void transform_rotate(transform_object_t *obj, float angle_deg)
{
	obj->angle += angle_deg;
	transform_apply(obj);
}

// ─── однорідні координати (матриці 3x3) ──────────────────────────────────────
//
//  Зсув:        | 1  0  dx |       застосовується як offset
//               | 0  1  dy |
//               | 0  0   1 |
//
//  Масштаб      | sx 0  cx*(1-sx) |   відносно центру c
//  відносно c:  | 0  sy cy*(1-sy) |
//               | 0  0      1     |
//
//  Поворот      | cos -sin  cx - cx*cos + cy*sin |
//  відносно c:  | sin  cos  cy - cx*sin - cy*cos |
//               |  0    0            1           |
//
// Реалізовано через збереження стану + transform_apply,
// що еквівалентно послідовному множенню матриць T⁻¹·M·T.

void transform_translate_homogeneous(transform_object_t *obj, float dx, float dy)
{
	obj->offset_x += dx;
	obj->offset_y += dy;
	transform_apply(obj);
}

void transform_scale_homogeneous(transform_object_t *obj, float sx, float sy)
{
	obj->scale_x = sx;
	obj->scale_y = sy;
	transform_apply(obj);
}

void transform_rotate_homogeneous(transform_object_t *obj, float angle_deg)
{
	obj->angle += angle_deg;
	transform_apply(obj);
}
