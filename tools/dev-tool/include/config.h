#pragma once

#include <stdbool.h>

typedef struct
{
	void (*save)(const char *path);
	void (*init)(const char *path);
	void (*load)(const char *path);
	void (*set)(const char *label, bool value);
	void (*get)(const char *label, bool *value);
} Config;

extern Config config;
