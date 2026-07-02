#include "style.h"

#include "../element.h"

void element_style_init(element_style_t *style) {
  if (!style)
    return;

  style->background_color = rgb(180, 180, 180);
  style->text_color = rgb(30, 30, 30);
}
