#pragma once
#include "object.h"

#define MAX_OBJECTS 256
#define MAX_DIRTY 64

typedef struct
{
	int x, y, w, h;
} rect_t;

typedef struct
{
	object_t *objects[MAX_OBJECTS];
	int count;

	rect_t dirty[MAX_DIRTY];
	int dirty_count;

} compositor_t;

void compositor_init();
void compositor_add(object_t *obj);
void compositor_remove(object_t *obj);
void compositor_render();

void compositor_bring_to_front(object_t *obj);
void compositor_move_object(object_t *obj, int new_x, int new_y);
