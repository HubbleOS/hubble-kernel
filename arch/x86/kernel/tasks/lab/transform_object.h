#pragma once
#include <stddef.h>

typedef struct
{
	float x, y;
} point_t;

typedef struct
{
	point_t *points;   // current (transformed)
	point_t *original; // original - do not change
	size_t count;
	float color_r, color_g, color_b; // 0.0 .. 1.0

	// current state of transformations
	float angle;	// degrees (accumulating)
	float scale_x;	// multiplier by X (default 1.0)
	float scale_y;	// multiplier by Y (default 1.0)
	float offset_x; // X shift
	float offset_y; // Y shift
} transform_object_t;

transform_object_t *transform_object_create(point_t *pts, size_t count);
void transform_object_destroy(transform_object_t *obj);

// Geometric transformations - ordinary coordinates
void transform_translate(transform_object_t *obj, float dx, float dy);
void transform_scale(transform_object_t *obj, float sx, float sy);
void transform_rotate(transform_object_t *obj, float angle_deg);

// Through homogeneous coordinates (3x3 matrices)
void transform_translate_homogeneous(transform_object_t *obj, float dx, float dy);
void transform_scale_homogeneous(transform_object_t *obj, float sx, float sy);
void transform_rotate_homogeneous(transform_object_t *obj, float angle_deg);
