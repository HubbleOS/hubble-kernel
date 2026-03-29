#include "exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

// удаляет ANSI escape-коды
void strip_ansi(char *out, const char *in)
{
	const char *src = in;
	char *dst = out;

	while (*src)
	{
		if (*src == 0x1B && src[1] == '[')
		{
			// пропускаем ESC[
			src += 2;

			// пропускаем цифры и символы типа ; до буквы завершающего кода
			while ((*src >= '0' && *src <= '9') || *src == ';')
				src++;

			if (*src)
				src++; // пропустить финальный символ (обычно 'm')
		}
		else
		{
			*dst++ = *src++;
		}
	}

	*dst = 0;
}

int run_command(const char *cmd, int verbose)
{
	char full_cmd[8192];

	// объединяем stderr -> stdout
	snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", cmd);

	if (verbose)
		LOG_INFO("Executing: %s", cmd);

	FILE *pipe = popen(full_cmd, "r");
	if (!pipe)
	{
		LOG_ERROR("popen() failed");
		return 1;
	}

	char buffer[4096];
	while (fgets(buffer, sizeof(buffer), pipe))
	{
		// вывод на экран (цветной, как есть)
		fputs(buffer, stdout);

		// одновременно записываем в лог
		char clean[4096];
		strip_ansi(clean, buffer);

		if (log_file)
			fprintf(log_file, "%s", clean);
	}

	int status = pclose(pipe);
	int exit_code = WEXITSTATUS(status);

	if (exit_code != 0)
		LOG_ERROR("Command failed with exit code %d: %s", exit_code, cmd);

	return exit_code;
}
