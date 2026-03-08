#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "log.h"
#include "fs.h"

#define MAX_PATH 4096

FILE *log_file = NULL;

void log_init(const char *log_dir)
{
	if (!log_dir || strlen(log_dir) == 0)
		return;

	mkdir_p(log_dir);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char log_path[MAX_PATH];

	snprintf(log_path, sizeof(log_path), "%s/%04d-%02d-%02d.log",
		 log_dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

	log_file = fopen(log_path, "w");
	if (!log_file)
	{
		fprintf(stderr, "Warning: Cannot open log file: %s\n", log_path);
		return;
	}

	fprintf(log_file, "\n========================================\n");
	fprintf(log_file, "Build session started: %04d-%02d-%02d %02d:%02d:%02d\n",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec);
	fprintf(log_file, "========================================\n");
	fflush(log_file);
}

void log_init_file(const char *log_file_path)
{
	if (!log_file_path || strlen(log_file_path) == 0)
		return;

	char dir[MAX_PATH];
	strncpy(dir, log_file_path, sizeof(dir));
	char *last_slash = strrchr(dir, '/');
	if (last_slash)
		*last_slash = 0;
	mkdir_p(dir);

	log_file = fopen(log_file_path, "a");
	if (!log_file)
	{
		fprintf(stderr, "Warning: Cannot open log file: %s\n", log_file_path);
		return;
	}

	fprintf(log_file, "\n===== Build session started =====\n");
	fflush(log_file);
}

void log_close(void)
{
	if (log_file)
	{
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		fprintf(log_file, "Build session ended: %04d-%02d-%02d %02d:%02d:%02d\n\n",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);
		fclose(log_file);
		log_file = NULL;
	}
}

void log_message(const char *level, const char *format, ...)
{
	if (!log_file)
		return;

	const char *color = COLOR_WHITE;
	if (strcmp(level, "INFO") == 0)
		color = COLOR_BLUE;
	else if (strcmp(level, "WARN") == 0)
		color = COLOR_YELLOW;
	else if (strcmp(level, "ERROR") == 0)
		color = COLOR_RED;
	else if (strcmp(level, "SUCCESS") == 0)
		color = COLOR_GREEN;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	char msg[4096];
	va_list args;
	va_start(args, format);
	vsnprintf(msg, sizeof(msg), format, args);
	va_end(args);

	fprintf(log_file, "[%02d:%02d:%02d] [%s] %s\n",
		t->tm_hour, t->tm_min, t->tm_sec, level, msg);
	fflush(log_file);

	printf("%s[%02d:%02d:%02d] [%s] %s%s\n",
	       color, t->tm_hour, t->tm_min, t->tm_sec, level, msg, COLOR_RESET);
}
