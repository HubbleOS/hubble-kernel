#pragma once
#include "object.h"

#define MAX_OBJECTS 256
#define MAX_DIRTY 64

#define LAYER_BG 0
#define LAYER_WINDOWS 1
#define LAYER_CURSOR 2
#define LAYER_EFFECTS 3

#define MAX_LAYERS 4

typedef struct
{
	int x, y, w, h;
} rect_t;

typedef struct
{
	object_t *objects[MAX_OBJECTS];
	int count;
} layer_t;

typedef struct
{
	layer_t layers[MAX_LAYERS]; // массив слоев
	rect_t dirty[MAX_DIRTY];
	int dirty_count;
} compositor_t;

void compositor_init();
void compositor_add(object_t *obj, int layer);
void compositor_remove(object_t *obj, int layer);
void compositor_render();

void compositor_add_damage(int x, int y, int w, int h);
void compositor_bring_to_front(object_t *obj, int layer);
void compositor_move_object(object_t *obj, int new_x, int new_y);
void compositor_change_size_object(object_t *obj, int new_w, int new_h);
