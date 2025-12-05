#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "log.h"

#define MAX_PATH 4096

static FILE *log_file = NULL;

void log_init(const char *log_dir)
{
	if (!log_dir || strlen(log_dir) == 0)
		return;

	// Создаём директорию для логов
	mkdir_p(log_dir);

	// Получаем текущую дату
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char log_path[MAX_PATH];

	snprintf(log_path, sizeof(log_path), "%s/%04d-%02d-%02d.log",
		 log_dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

	// Открываем файл в режиме добавления
	log_file = fopen(log_path, "w");
	if (!log_file)
	{
		fprintf(stderr, "Warning: Cannot open log file: %s\n", log_path);
		return;
	}

	// Записываем заголовок сессии
	fprintf(log_file, "\n========================================\n");
	fprintf(log_file, "Build session started: %04d-%02d-%02d %02d:%02d:%02d\n",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec);
	fprintf(log_file, "========================================\n");
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

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	// Записываем временную метку и уровень
	fprintf(log_file, "[%02d:%02d:%02d] [%s] ",
		t->tm_hour, t->tm_min, t->tm_sec, level);

	// Записываем сообщение
	va_list args;
	va_start(args, format);
	vfprintf(log_file, format, args);
	va_end(args);

	fprintf(log_file, "\n");
	fflush(log_file);
}
