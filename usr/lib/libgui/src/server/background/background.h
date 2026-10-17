#pragma once

#include <core/object/object.h>
#include <utils/color/color.h>

object_t *background_create(color_t color);
void background_set_color(object_t *bg, color_t color);
