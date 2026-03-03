#include "object.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

object_t *object_create(int x, int y, int w, int h)
{
	object_t *obj = malloc(sizeof(object_t));
	if (!obj)
		return NULL;

	obj->x = x;
	obj->y = y;
	obj->width = w;
	obj->height = h;
	obj->buffer = malloc(w * h * sizeof(uint32_t));

	if (!obj->buffer)
	{
		free(obj);
		return NULL;
	}

	memset(obj->buffer, 0, w * h * sizeof(uint32_t));

	return obj;
}

void object_destroy(object_t *obj)
{
	free(obj->buffer);
	free(obj);
}
