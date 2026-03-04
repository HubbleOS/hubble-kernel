#pragma once
#include <_cheader.h>
#include <stddef.h>

_Begin_C_Header;

typedef struct input_device
{
	size_t (*read)(char *buffer, size_t len, void *user_data);
	void *user_data; // arbitrary context (for example, a pointer to Terminal)
} input_device_t;

void register_stdin_device(input_device_t *dev);
input_device_t *get_stdin_device(void);

_End_C_Header;
