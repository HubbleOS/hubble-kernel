#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static FILE *log_file = NULL;

static void open_log()
{
	if (log_file)
		return;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	char filename[64];
	snprintf(filename, sizeof(filename), "logs/log_%04d%02d%02d_%02d%02d%02d.txt",
			 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			 t->tm_hour, t->tm_min, t->tm_sec);

	// Создаём папку logs, если надо (на Unix)
	system("mkdir -p logs");

	log_file = fopen(filename, "a");
	if (!log_file)
	{
		fprintf(stderr, "Failed to open log file %s\n", filename);
		exit(EXIT_FAILURE);
	}
}

void log_message(const char *fmt, ...)
{
	open_log();

	va_list args;
	va_start(args, fmt);

	// Время в логе
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	fprintf(log_file, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);

	vfprintf(log_file, fmt, args);
	fprintf(log_file, "\n");

	fflush(log_file);
	va_end(args);
}

void close_log()
{
	if (log_file)
	{
		fclose(log_file);
		log_file = NULL;
	}
}

#include "app.c"

int main()
{
	log_message("App started");
	// твой код
	log_message("App finished");

	close_log();
	return 0;
}
