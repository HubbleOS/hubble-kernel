#include "transform_object.h"

#include <stdlib.h>
#include <math.h>

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

// helper: get center of object
static void get_center(transform_object_t *obj, float *cx, float *cy)
{
	*cx = 0.0f;
	*cy = 0.0f;
	for (size_t i = 0; i < obj->count; i++)
	{
		*cx += obj->points[i].x;
		*cy += obj->points[i].y;
	}
	*cx /= (float)obj->count;
	*cy /= (float)obj->count;
}

// regular coordinates
// Operations are applied directly to the current points, using formulas without matrices.

// translation: x' = x + dx,  y' = y + dy
void transform_translate(transform_object_t *obj, float dx, float dy)
{
	for (size_t i = 0; i < obj->count; i++)
	{
		obj->points[i].x += dx;
		obj->points[i].y += dy;
	}
	obj->offset_x += dx;
	obj->offset_y += dy;
}

// Scale relative to its own reference point (arithmetic mean):
// x' = cx + (x - cx) * sx
// y' = cy + (y - cy) * sy
void transform_scale(transform_object_t *obj, float sx, float sy)
{
	float cx, cy;
	get_center(obj, &cx, &cy);

	for (size_t i = 0; i < obj->count; i++)
	{
		obj->points[i].x = cx + (obj->points[i].x - cx) * sx;
		obj->points[i].y = cy + (obj->points[i].y - cy) * sy;
	}
	obj->scale_x *= sx;
	obj->scale_y *= sy;
}

// Rotation around your own control point:
// x' = cx + (x-cx)*cos - (y-cy)*sin
// y' = cy + (x-cx)*sin + (y-cy)*cos
void transform_rotate(transform_object_t *obj, float angle_deg)
{
	float cx, cy;
	get_center(obj, &cx, &cy);

	float rad = angle_deg * (float)M_PI / 180.0f;
	float cosA = cosf(rad);
	float sinA = sinf(rad);

	for (size_t i = 0; i < obj->count; i++)
	{
		float dx = obj->points[i].x - cx;
		float dy = obj->points[i].y - cy;
		obj->points[i].x = cx + dx * cosA - dy * sinA;
		obj->points[i].y = cy + dx * sinA + dy * cosA;
	}
	obj->angle += angle_deg;
}

// homogeneous coordinates (3x3 matrices)
// Each point is a vector [x, y, 1], multiplied by a transformation matrix.

// Auxiliary: apply the matrix [a b tx / c d ty / 0 0 1] to all points
static void apply_matrix(transform_object_t *obj,
			 float a, float b, float tx,
			 float c, float d, float ty)
{
	for (size_t i = 0; i < obj->count; i++)
	{
		float x = obj->points[i].x;
		float y = obj->points[i].y;
		// [x', y', 1] = M * [x, y, 1]
		obj->points[i].x = a * x + b * y + tx;
		obj->points[i].y = c * x + d * y + ty;
	}
}

// translation:
// | 1  0  dx |
// | 0  1  dy |
// | 0  0   1 |
void transform_translate_homogeneous(transform_object_t *obj, float dx, float dy)
{
	apply_matrix(obj,
		     1, 0, dx,
		     0, 1, dy);
	obj->offset_x += dx;
	obj->offset_y += dy;
}

// Scale relative to its own reference point c:
// T(c) * S * T(-c) =
// | sx  0   cx*(1-sx) |
// |  0  sy  cy*(1-sy) |
// |  0   0      1     |
void transform_scale_homogeneous(transform_object_t *obj, float sx, float sy)
{
	float cx, cy;
	get_center(obj, &cx, &cy);

	apply_matrix(obj,
		     sx, 0, cx * (1.0f - sx),
		     0, sy, cy * (1.0f - sy));
	obj->scale_x *= sx;
	obj->scale_y *= sy;
}

// Rotation around your own control point c:
// T(c) * R * T(-c) =
// | cos  -sin   cx - cx*cos + cy*sin |
// | sin   cos   cy - cx*sin - cy*cos |
// |  0     0             1           |
void transform_rotate_homogeneous(transform_object_t *obj, float angle_deg)
{
	float cx, cy;
	get_center(obj, &cx, &cy);

	float rad = angle_deg * (float)M_PI / 180.0f;
	float cosA = cosf(rad);
	float sinA = sinf(rad);

	apply_matrix(obj,
		     cosA, -sinA, cx - cx * cosA + cy * sinA,
		     sinA, cosA, cy - cx * sinA - cy * cosA);
	obj->angle += angle_deg;
}
