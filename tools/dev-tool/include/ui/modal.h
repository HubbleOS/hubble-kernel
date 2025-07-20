#pragma once

#include <ui/windows.h>
#include <ui/button.h>

typedef struct
{
	const char *label;
	void (*action)(void);
} ModalButton;

void show_modal_with_buttons(const char *title, ModalButton *buttons, size_t count);