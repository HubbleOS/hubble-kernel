/**
 * @file fs.h
 * @brief
 *
 */

#pragma once

#include <time.h>

#define MAX_PATH 4096
#define MAX_FILES 2048

typedef struct
{
	char path[MAX_PATH];
	time_t mtime;
} SourceFile;

int mkdir_p(const char *path);
time_t get_mtime(const char *path);
int needs_rebuild(const char *src, const char *obj);
void scan_directory(const char *dir, const char *ext, SourceFile *files, int *count, int max);
void get_obj_path(const char *src, const char *src_dir, const char *build_dir, char *obj, size_t obj_size);
