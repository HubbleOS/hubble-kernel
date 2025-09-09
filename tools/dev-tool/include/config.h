#pragma once

typedef struct
{
	void (*save)(const char *path);
	void (*init)(const char *path);
	void (*load)(const char *path);
} Config;

extern Config config;
