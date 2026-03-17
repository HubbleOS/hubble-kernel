#pragma once

#include <gui/utils/color/color.h>
#include <gui/core/object/object.h>

object_t *background_create(color_t color);
void background_set_color(object_t *bg, color_t color);
