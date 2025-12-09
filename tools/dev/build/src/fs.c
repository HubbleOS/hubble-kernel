#include "fs.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

int mkdir_p(const char *path)
{
	char tmp[MAX_PATH];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	for (p = tmp + 1; *p; p++)
	{
		if (*p == '/')
		{
			*p = 0;
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
			{
				return -1;
			}
			*p = '/';
		}
	}
	if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
	{
		return -1;
	}
	return 0;
}

time_t get_mtime(const char *path)
{
	struct stat st;
	if (stat(path, &st) == 0)
	{
		return st.st_mtime;
	}
	return 0;
}

int needs_rebuild(const char *src, const char *obj)
{
	time_t src_time = get_mtime(src);
	time_t obj_time = get_mtime(obj);

	if (obj_time == 0)
		return 1; // объектный файл не существует
	return src_time > obj_time;
}

void scan_directory(const char *dir, const char *ext, SourceFile *files, int *count, int max)
{
	DIR *d;
	struct dirent *entry;
	char path[MAX_PATH];

	d = opendir(dir);
	if (!d)
	{
		LOG_WARN("Cannot open directory: %s", dir);
		return;
	}

	while ((entry = readdir(d)) != NULL && *count < max)
	{
		if (entry->d_name[0] == '.')
			continue;

		snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

		struct stat st;
		if (stat(path, &st) == 0)
		{
			if (S_ISDIR(st.st_mode))
			{
				scan_directory(path, ext, files, count, max);
			}
			else if (S_ISREG(st.st_mode))
			{
				char *dot = strrchr(entry->d_name, '.');
				if (dot && strcmp(dot, ext) == 0)
				{
					strncpy(files[*count].path, path, MAX_PATH - 1);
					files[*count].mtime = st.st_mtime;
					LOG_INFO("Found source file: %s", path);
					(*count)++;
				}
			}
		}
	}
	closedir(d);
}

void get_obj_path(const char *src, const char *src_dir, const char *build_dir, char *obj, size_t obj_size)
{
	const char *rel = src;

	// Убираем src_dir из начала пути
	if (strncmp(src, src_dir, strlen(src_dir)) == 0)
	{
		rel = src + strlen(src_dir);
		while (*rel == '/')
			rel++;
	}

	snprintf(obj, obj_size, "%s/%s", build_dir, rel);

	// Меняем расширение на .o
	char *dot = strrchr(obj, '.');
	if (dot)
	{
		strcpy(dot, ".o");
	}
}
