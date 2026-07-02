#include "exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "log.h"
#include "buildconfig.h"

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

int run_commands_parallel(const char *cmds[], const char *src_files[], int count, int max_jobs, int verbose)
{
	if (count <= 0)
		return 0;

	pid_t *pids = malloc(sizeof(pid_t) * count);
	int *indices = malloc(sizeof(int) * count);
	char (*tmp_files)[MAX_PATH] = malloc(sizeof(char[MAX_PATH]) * count);
	int running = 0, submitted = 0, failed = 0;

	// Create temp files for output capture
	for (int i = 0; i < count; i++)
	{
		snprintf(tmp_files[i], MAX_PATH, "/tmp/hbuild-XXXXXX");
		int fd = mkstemp(tmp_files[i]);
		if (fd < 0) { perror("mkstemp"); free(pids); free(indices); free(tmp_files); return 1; }
		close(fd);
	}

	while (submitted < count || running > 0)
	{
		// Start new jobs up to max_jobs limit
		while (running < max_jobs && submitted < count)
		{
			pid_t pid = fork();
			if (pid == 0)
			{
				// Child: redirect stdout/stderr to temp file
				int fd = open(tmp_files[submitted], O_WRONLY | O_TRUNC);
				if (fd < 0) exit(1);
				dup2(fd, STDOUT_FILENO);
				dup2(fd, STDERR_FILENO);
				close(fd);
				execl("/bin/sh", "sh", "-c", cmds[submitted], (char *)NULL);
				exit(1);
			}
			if (verbose)
				LOG_INFO("Started: %s (pid %d)", src_files[submitted], pid);
			pids[running] = pid;
			indices[running] = submitted;
			running++;
			submitted++;
		}

		// Wait for any child to finish
		int status;
		pid_t done = waitpid(-1, &status, 0);
		if (done < 0) break;

		// Find which job finished
		int done_idx = -1;
		for (int i = 0; i < running; i++)
		{
			if (pids[i] == done)
			{
				done_idx = indices[i];
				// Compact arrays
				for (int j = i; j < running - 1; j++)
				{
					pids[j] = pids[j + 1];
					indices[j] = indices[j + 1];
				}
				running--;
				break;
			}
		}

		if (done_idx >= 0)
		{
			// Print captured output
			FILE *f = fopen(tmp_files[done_idx], "r");
			if (f)
			{
				char buf[4096];
				while (fgets(buf, sizeof(buf), f))
				{
					fputs(buf, stdout);
					char clean[4096];
					strip_ansi(clean, buf);
					if (log_file)
						fprintf(log_file, "%s", clean);
				}
				fclose(f);
			}

			int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
			if (exit_code == 0)
			{
				if (verbose)
					LOG_SUCCESS("Successfully built: %s", src_files[done_idx]);
			}
			else
			{
				LOG_ERROR("Failed to build: %s (exit code %d)", src_files[done_idx], exit_code);
				failed++;
			}
		}
	}

	// Cleanup
	for (int i = 0; i < count; i++)
		unlink(tmp_files[i]);
	free(pids);
	free(indices);
	free(tmp_files);

	return failed;
}
