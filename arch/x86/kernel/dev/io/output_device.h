#pragma once
#include <_cheader.h>
#include <stddef.h>

_Begin_C_Header;

typedef struct output_device
{
	void (*write)(const char *buf, size_t len, void *user_data);
	void *user_data; // arbitrary context (for example, a pointer to Terminal)
} output_device_t;

void register_stdout_device(output_device_t *dev);
output_device_t *get_stdout_device(void);

_End_C_Header;
