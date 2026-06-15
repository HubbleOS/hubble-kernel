#pragma once

#include <stdbool.h>

int module_load(const char *path);
int module_load_directory(const char *path);
bool module_is_loaded(const char *name);
