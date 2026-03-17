#include "style.h"

#include "../element.h"

void element_apply_style(element_t *el, element_style_t *override)
{
	if (!el->style_set || !el->style_set->normal)
		return;

	el->active_style = *el->style_set->normal;

	if (!override)
		return;

	if (override->background_color)
		el->active_style.background_color = override->background_color;
	if (override->text_color)
		el->active_style.text_color = override->text_color;
	if (override->border_radius)
		el->active_style.border_radius = override->border_radius;
}
