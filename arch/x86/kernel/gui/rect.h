#pragma once

#include <stdbool.h>

typedef struct
{
	int x, y, w, h;
} rect_t;

void merge_dirty_rects(rect_t *dirty, int *count);
rect_t rect_union(rect_t a, rect_t b);
bool rects_intersect(rect_t a, rect_t b);
