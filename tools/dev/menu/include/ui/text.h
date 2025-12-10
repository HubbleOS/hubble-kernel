#pragma once

#include <ui/window.h>

UIElement *text_create(const char *text, int x, int y);
void text_set(UIElement *elem, const char *new_text);
