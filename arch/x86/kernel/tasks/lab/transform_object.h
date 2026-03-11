#pragma once
#include <stddef.h>

typedef struct
{
	float x, y;
} point_t;

typedef struct
{
	point_t *points;   // поточні (трансформовані)
	point_t *original; // оригінальні — не змінюються
	size_t count;
	float color_r, color_g, color_b; // 0.0 .. 1.0

	// поточний стан трансформацій
	float angle;	// градуси (накопичується)
	float scale_x;	// множник по X (default 1.0)
	float scale_y;	// множник по Y (default 1.0)
	float offset_x; // зсув по X
	float offset_y; // зсув по Y
} transform_object_t;

transform_object_t *transform_object_create(point_t *pts, size_t count);
void transform_object_destroy(transform_object_t *obj);

// Геометричні перетворення — звичайні координати
void transform_translate(transform_object_t *obj, float dx, float dy);
void transform_scale(transform_object_t *obj, float sx, float sy);
void transform_rotate(transform_object_t *obj, float angle_deg);

// Через однорідні координати (матриці 3x3)
void transform_translate_homogeneous(transform_object_t *obj, float dx, float dy);
void transform_scale_homogeneous(transform_object_t *obj, float sx, float sy);
void transform_rotate_homogeneous(transform_object_t *obj, float angle_deg);
