#include "object.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../compositor/compositor.h"

#define MAX_ELEMENTS 1024

static inline bool element_is_fully_dirty(element_t *el) {
  return el->dirty_rect.valid && el->dirty_rect.x == 0 &&
         el->dirty_rect.y == 0 && el->dirty_rect.w >= el->width &&
         el->dirty_rect.h >= el->height;
}

void element_mark_dirty(element_t *el) {
  el->dirty_rect = (dirty_rect_t){0, 0, el->width, el->height, true};
}

void element_mark_dirty_rect(element_t *el, int x, int y, int w, int h) {
  if (element_is_fully_dirty(el))
    return;

  if (!el->dirty_rect.valid) {
    el->dirty_rect = (dirty_rect_t){x, y, w, h, true};
    return;
  }

  int x1 = el->dirty_rect.x < x ? el->dirty_rect.x : x;
  int y1 = el->dirty_rect.y < y ? el->dirty_rect.y : y;
  int x2_old = el->dirty_rect.x + el->dirty_rect.w;
  int x2_new = x + w;
  int y2_old = el->dirty_rect.y + el->dirty_rect.h;
  int y2_new = y + h;
  int x2 = x2_old > x2_new ? x2_old : x2_new;
  int y2 = y2_old > y2_new ? y2_old : y2_new;
  el->dirty_rect = (dirty_rect_t){x1, y1, x2 - x1, y2 - y1, true};
}

object_t *object_create(int x, int y, int w, int h, color_t bg_color) {
  object_t *obj = malloc(sizeof(object_t));
  if (!obj) {
    return NULL;
  }

  obj->x = x;
  obj->y = y;
  obj->width = w;
  obj->height = h;
  obj->bg_color = bg_color;

  obj->elements = malloc(MAX_ELEMENTS * sizeof(element_t *));
  obj->element_count = 0;
  obj->element_capacity = MAX_ELEMENTS;

  obj->buffer = malloc(w * h * sizeof(uint32_t));
  if (!obj->buffer) {
    free(obj);
    return NULL;
  }

  for (size_t i = 0; i < (size_t)(w * h); i++)
    obj->buffer[i] = bg_color;

  return obj;
}

void object_destroy(object_t *obj) {
  free(obj->buffer);
  free(obj->elements);
  free(obj);
}

void object_flush(object_t *obj) {
  for (int e = 0; e < obj->element_count; e++) {
    element_t *el = obj->elements[e];
    if (!el->dirty_rect.valid)
      continue;

    if (el->draw && el->needs_redraw) {
      el->draw(el);
      el->needs_redraw = false;
    }

    dirty_rect_t dr = el->dirty_rect;

    int dx = dr.x < 0 ? 0 : dr.x;
    int dy = dr.y < 0 ? 0 : dr.y;
    int dw = dr.w;
    int dh = dr.h;
    if (dx + dw > el->width)
      dw = el->width - dx;
    if (dy + dh > el->height)
      dh = el->height - dy;

    int abs_x = el->x + dx;
    int abs_y = el->y + dy;
    if (abs_x < 0) {
      dx -= abs_x;
      dw += abs_x;
      abs_x = 0;
    }
    if (abs_y < 0) {
      dy -= abs_y;
      dh += abs_y;
      abs_y = 0;
    }
    if (abs_x + dw > obj->width)
      dw = obj->width - abs_x;
    if (abs_y + dh > obj->height)
      dh = obj->height - abs_y;
    if (dw <= 0 || dh <= 0) {
      el->dirty_rect.valid = false;
      continue;
    }

    for (int y = 0; y < dh; y++) {
      uint32_t *dst = obj->buffer + (abs_y + y) * obj->width + abs_x;
      for (int x = 0; x < dw; x++)
        dst[x] = obj->bg_color;
    }

    for (int y = 0; y < dh; y++) {
      uint32_t *src = el->buffer + (dy + y) * el->width + dx;
      uint32_t *dst = obj->buffer + (abs_y + y) * obj->width + abs_x;
      for (int x = 0; x < dw; x++) {
        uint8_t a = src[x] >> 24;
        if (a == 0)
          continue;
        if (a == 255)
          dst[x] = src[x];
        else
          dst[x] = color_blend(src[x], dst[x]);
      }
    }

    el->dirty_rect.valid = false;

    for (int j = e + 1; j < obj->element_count; j++) {
      element_t *over = obj->elements[j];
      if (!over->buffer)
        continue;

      int ix1 = over->x > abs_x ? over->x : abs_x;
      int iy1 = over->y > abs_y ? over->y : abs_y;
      int ix2 = (over->x + over->width) < (abs_x + dw) ? (over->x + over->width)
                                                       : (abs_x + dw);
      int iy2 = (over->y + over->height) < (abs_y + dh)
                    ? (over->y + over->height)
                    : (abs_y + dh);

      if (ix1 >= ix2 || iy1 >= iy2)
        continue;

      if (over->draw)
        over->draw(over);

      for (int y = iy1; y < iy2; y++) {
        int src_row = y - over->y;
        int src_col = ix1 - over->x;
        if (src_row < 0 || src_row >= over->height)
          continue;
        if (src_col < 0 || src_col >= over->width)
          continue;

        uint32_t *src = over->buffer + src_row * over->width + src_col;
        uint32_t *dst = obj->buffer + y * obj->width + ix1;

        for (int x = 0; x < ix2 - ix1; x++) {
          uint8_t a = src[x] >> 24;
          if (a == 0)
            continue;
          if (a == 255)
            dst[x] = src[x];
          else
            dst[x] = color_blend(src[x], dst[x]);
        }
      }
    }

    compositor_add_damage(obj->layer, obj->x + abs_x, obj->y + abs_y, dw, dh);
  }
}

void object_redraw_elements(object_t *obj) {
  for (int e = 0; e < obj->element_count; e++)
    element_mark_dirty(obj->elements[e]);
  object_flush(obj);
}

void object_add_element(object_t *obj, element_t *el) {
  if (obj->element_count >= MAX_ELEMENTS)
    return;
  el->owner = obj;
  obj->elements[obj->element_count++] = el;

  if (el->style_set)
    element_apply_style(el, NULL);

  element_mark_dirty(el);
  object_flush(obj);
}

void object_move_element(object_t *obj, element_t *el, int new_x, int new_y) {
  int ox = el->x < 0 ? 0 : el->x;
  int oy = el->y < 0 ? 0 : el->y;
  int ow = el->width, oh = el->height;
  if (ox + ow > obj->width)
    ow = obj->width - ox;
  if (oy + oh > obj->height)
    oh = obj->height - oy;

  for (int y = 0; y < oh; y++) {
    uint32_t *row = obj->buffer + (oy + y) * obj->width + ox;
    for (int x = 0; x < ow; x++)
      row[x] = obj->bg_color;
  }

  for (int e = 0; e < obj->element_count; e++) {
    element_t *other = obj->elements[e];
    if (other == el)
      continue;
    if (!other->buffer)
      continue;

    int ix1 = other->x > ox ? other->x : ox;
    int iy1 = other->y > oy ? other->y : oy;
    int ix2 = (other->x + other->width) < (ox + ow) ? (other->x + other->width)
                                                    : (ox + ow);
    int iy2 = (other->y + other->height) < (oy + oh)
                  ? (other->y + other->height)
                  : (oy + oh);

    if (ix1 >= ix2 || iy1 >= iy2)
      continue;

    for (int y = iy1; y < iy2; y++) {
      int src_row = y - other->y;
      int src_col = ix1 - other->x;
      if (src_row < 0 || src_row >= other->height)
        continue;
      if (src_col < 0 || src_col >= other->width)
        continue;

      uint32_t *src = other->buffer + src_row * other->width + src_col;
      uint32_t *dst = obj->buffer + y * obj->width + ix1;

      for (int x = 0; x < ix2 - ix1; x++) {
        uint8_t a = src[x] >> 24;
        if (a == 0)
          continue;
        if (a == 255)
          dst[x] = src[x];
        else
          dst[x] = color_blend(src[x], dst[x]);
      }
    }
  }

  compositor_add_damage(obj->layer, obj->x + ox, obj->y + oy, ow, oh);

  el->x = new_x;
  el->y = new_y;
  element_mark_dirty(el);
  object_flush(obj);
}
