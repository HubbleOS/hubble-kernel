#pragma once

#include <core/object/object.h>
#include <utils/color/color.h>

/** The wallpaper: a vertical gradient from @p top to @p bottom. */
object_t *background_create(color_t top, color_t bottom);
void background_set_gradient(object_t *bg, color_t top, color_t bottom);
