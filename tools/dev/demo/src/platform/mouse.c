#include "mouse.h"

#include <stddef.h>

mouse_t *mouse = NULL;

mouse_t *get_mouse_info(void)
{
	return mouse;
}
