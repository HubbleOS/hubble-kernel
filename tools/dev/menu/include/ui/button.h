#pragma once

#include <ui/window.h>
#include <ui/button.h>

typedef void (*UIButtonCallback)(void);

typedef struct
{
	const char *label;
	UIButtonCallback action;
	bool highlighted;
} UIButtonData;

UIElement *button_create(const char *label, UIButtonCallback on_click, int x, int y, int w, int h);
