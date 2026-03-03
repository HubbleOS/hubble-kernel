#pragma once

#include <stdint.h>

typedef struct
{
	int x, y;
	int width, height;

	uint32_t *buffer;
} object_t;

object_t *object_create(int x, int y, int w, int h);
void object_destroy(object_t *obj);
