#pragma once
#include "object.h"

#define MAX_OBJECTS 256

typedef struct
{
	object_t *objects[MAX_OBJECTS];
	int count;
} compositor_t;

void compositor_init();
void compositor_add(object_t *obj);
void compositor_remove(object_t *obj);
void compositor_render();

void compositor_bring_to_front(object_t *obj);
