#pragma once

#include <stdbool.h>

typedef struct
{
	void (*save)(const char *path);
	void (*init)(const char *path);
	void (*load)(const char *path);
	void (*set)(const char *label, const char *value);
	const char *(*get)(const char *label);
} Config;

extern Config config;
