#include "fs.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

/**
 * @brief Recursively creates a directory and all intermediate directories
 */
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

/**
 * @brief Returns the last modification time of a file
 */
time_t get_mtime(const char *path)
{
	struct stat st;
	if (stat(path, &st) == 0)
	{
		return st.st_mtime;
	}
	return 0;
}

/**
 * @brief Determines if a target file needs to be rebuilt based on modification times
 */
int needs_rebuild(const char *src, const char *obj)
{
	time_t obj_time = get_mtime(obj);

	if (obj_time == 0)
		return 1; // object file does not exist

	if (get_mtime(src) > obj_time)
		return 1;

	// Check header dependencies from .d file
	char dep_file[MAX_PATH];
	strncpy(dep_file, obj, sizeof(dep_file) - 1);
	char *dot = strrchr(dep_file, '.');
	if (!dot)
		return 0;
	strcpy(dot, ".d");

	FILE *f = fopen(dep_file, "r");
	if (!f)
		return 0;

	// Concatenate all dependency lines (handling backslash continuations)
	char all_deps[65536] = {0};
	char buf[8192];
	int past_colon = 0;

	while (fgets(buf, sizeof(buf), f))
	{
		size_t blen = strlen(buf);
		while (blen > 0 && (buf[blen - 1] == '\n' || buf[blen - 1] == '\r'))
			buf[--blen] = '\0';
		if (blen == 0)
			continue;

		int has_cont = (blen > 0 && buf[blen - 1] == '\\');
		if (has_cont)
			buf[--blen] = '\0';

		if (!past_colon)
		{
			char *colon = strchr(buf, ':');
			if (!colon)
				continue;
			past_colon = 1;
			strncat(all_deps, colon + 1, sizeof(all_deps) - strlen(all_deps) - 1);
		}
		else
		{
			strncat(all_deps, buf, sizeof(all_deps) - strlen(all_deps) - 1);
		}
		strncat(all_deps, " ", sizeof(all_deps) - strlen(all_deps) - 1);

		if (!has_cont)
			break;
	}
	fclose(f);

	// Tokenize and check each dependency
	char *token = strtok(all_deps, " \t");
	while (token)
	{
		if (strlen(token) > 0 && token[0] != '\0')
		{
			if (get_mtime(token) > obj_time)
				return 1;
		}
		token = strtok(NULL, " \t");
	}

	return 0;
}

#include <string.h>
#include <stdlib.h>
#include "fs.h"

#define MAX_PATH 4096

const char *pretty_path(const char *full_path)
{
	static char buf[MAX_PATH];
	const char *root = getenv("ROOT_DIR"); // берём корень проекта из Makefile

	if (!root || !full_path)
		return full_path;

	size_t root_len = strlen(root);

	if (strncmp(full_path, root, root_len) == 0)
	{
		const char *p = full_path + root_len;
		if (*p == '/')
			p++; // убираем лишний слеш
		strncpy(buf, p, MAX_PATH - 1);
		buf[MAX_PATH - 1] = '\0';
		return buf;
	}

	return full_path; // если путь не внутри ROOT_DIR, возвращаем как есть
}

/**
 * @brief Recursively scans a directory for files with a given extension
 */
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

	char real[MAX_PATH];

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
					if (realpath(path, real))
					{
						strncpy(files[*count].path, real, MAX_PATH - 1);
					}
					else
					{
						strncpy(files[*count].path, path, MAX_PATH - 1);
					}

					files[*count].mtime = st.st_mtime;
					// LOG_INFO("Found source file: %s", files[*count].path);
					LOG_INFO("Found source file: %s", pretty_path(files[*count].path));
					(*count)++;
				}
			}
		}
	}
	closedir(d);
}

/**
 * @brief Computes the object file path for a given source file
 *
 * Changes extension to ".o" and preserves relative path inside build_dir
 */
// void get_obj_path(const char *src, const char *src_dir, const char *build_dir, char *obj, size_t obj_size)
// {
// 	const char *rel = src;

// 	// Remove src_dir prefix
// 	if (strncmp(src, src_dir, strlen(src_dir)) == 0)
// 	{
// 		rel = src + strlen(src_dir);
// 		while (*rel == '/')
// 			rel++;
// 	}

// 	snprintf(obj, obj_size, "%s/%s", build_dir, rel);

// 	// Replace extension with ".o"
// 	char *dot = strrchr(obj, '.');
// 	if (dot)
// 	{
// 		strcpy(dot, ".o");
// 	}
// }

#include <limits.h>
#include <string.h>
#include <stdlib.h>

void get_obj_path(const char *src, const char *src_dir, const char *build_dir, char *obj, size_t obj_size)
{
	char abs_src[MAX_PATH];
	char abs_src_dir[MAX_PATH];

	// Получаем абсолютный путь к исходнику
	if (!realpath(src, abs_src))
	{
		strncpy(abs_src, src, sizeof(abs_src) - 1);
		abs_src[sizeof(abs_src) - 1] = '\0';
	}

	// Абсолютный путь к каталогу исходников
	if (!realpath(src_dir, abs_src_dir))
	{
		strncpy(abs_src_dir, src_dir, sizeof(abs_src_dir) - 1);
		abs_src_dir[sizeof(abs_src_dir) - 1] = '\0';
	}

	const char *rel = abs_src;

	// Отрезаем префикс src_dir
	size_t prefix_len = strlen(abs_src_dir);
	if (strncmp(abs_src, abs_src_dir, prefix_len) == 0)
	{
		rel = abs_src + prefix_len;
		while (*rel == '/')
			rel++; // убираем ведущий слеш
	}

	// Формируем путь в build
	snprintf(obj, obj_size, "%s/%s", build_dir, rel);

	// Меняем расширение на .o
	char *dot = strrchr(obj, '.');
	if (dot)
		strcpy(dot, ".o");
}
