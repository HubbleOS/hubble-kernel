#include "builder.h"
#include "buildconfig.h"
#include "fs.h"
#include "log.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>

int build_file(const char *src, const char *obj, SourceLang lang, const BuildConfig *cfg, const char *cxx_compiler)
{
	char cmd[8192];
	char obj_dir[4096];

	strncpy(obj_dir, obj, sizeof(obj_dir) - 1);
	char *last_slash = strrchr(obj_dir, '/');
	if (last_slash)
		*last_slash = 0;
	mkdir_p(obj_dir);

	if (!cfg->force_rebuild && !needs_rebuild(src, obj))
	{
		if (cfg->verbose)
			printf("Skipping %s (up to date)\n", src);
		LOG_INFO("Skipping %s (up to date)", src);
		return 0;
	}

	int ret = 0;
	switch (lang)
	{
	case LANG_C:
		printf("Compiling: %s\n", src);
		LOG_INFO("Compiling: %s -> %s", src, obj);
		snprintf(cmd, sizeof(cmd), "%s -MMD -MP -fdiagnostics-color=always %s %s -c %s -o %s",
			 cfg->compiler, cfg->cflags, cfg->includes, src, obj);
		ret = run_command(cmd, cfg->verbose);
		break;
	case LANG_CPP:
		printf("Compiling C++: %s\n", src);
		LOG_INFO("Compiling C++: %s -> %s", src, obj);
		snprintf(cmd, sizeof(cmd), "%s -MMD -MP -fdiagnostics-color=always %s %s -c %s -o %s",
			 cxx_compiler, cfg->cflags, cfg->includes, src, obj);
		ret = run_command(cmd, cfg->verbose);
		break;
	case LANG_ASM:
		printf("Assembling: %s\n", src);
		LOG_INFO("Assembling: %s -> %s", src, obj);
		snprintf(cmd, sizeof(cmd), "%s %s %s -o %s",
			 cfg->assembler, cfg->asmflags, src, obj);
		ret = run_command(cmd, cfg->verbose);
		break;
	}

	if (ret == 0)
	{
		LOG_SUCCESS("Successfully built: %s", src);
	}
	else
	{
		LOG_ERROR("Failed to build: %s", src);
	}

	return ret;
}

int create_archive(const char *output, char obj_files[][MAX_PATH], int obj_count, const BuildConfig *cfg)
{
	char cmd[MAX_CMD];
	int cmd_len;

	printf("Creating archive: %s\n", output);
	LOG_INFO("Creating archive: %s (%d object files)", output, obj_count);

	char output_dir[MAX_PATH];
	strncpy(output_dir, output, sizeof(output_dir) - 1);
	char *last_slash = strrchr(output_dir, '/');
	if (last_slash)
		*last_slash = 0;
	mkdir_p(output_dir);

	cmd_len = snprintf(cmd, sizeof(cmd), "%s rcs %s", cfg->archiver, output);

	for (int i = 0; i < obj_count; i++)
	{
		cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
		if (cmd_len >= sizeof(cmd) - 4096)
		{
			fprintf(stderr, "Error: command too long\n");
			LOG_ERROR("Archive command too long");
			return 1;
		}
	}

	int ret = run_command(cmd, cfg->verbose);
	if (ret == 0)
	{
		LOG_SUCCESS("Successfully created archive: %s", output);
	}
	else
	{
		LOG_ERROR("Failed to create archive: %s", output);
	}

	return ret;
}

int link_executable(const char *output, char obj_files[][MAX_PATH], int obj_count, const BuildConfig *cfg, const char *cxx_compiler)
{
	char cmd[MAX_CMD];
	int cmd_len;

	printf("Linking executable: %s\n", output);
	LOG_INFO("Linking executable: %s (%d object files)", output, obj_count);

	// Создаём директорию для выходного файла
	char output_dir[MAX_PATH];
	strncpy(output_dir, output, sizeof(output_dir) - 1);
	char *last_slash = strrchr(output_dir, '/');
	if (last_slash)
		*last_slash = 0;
	mkdir_p(output_dir);

	// Выбираем линкер (если есть C++, используем g++/clang++, иначе gcc/clang)
	const char *linker = (strlen(cfg->linker) > 0) ? cfg->linker : cxx_compiler;

	// Начинаем команду линковки
	if (strlen(cfg->ldscript) > 0)
	{
		// Если указан linker script, используем ld напрямую и добавляем -T
		cmd_len = snprintf(cmd, sizeof(cmd), "%s -T%s -o %s",
				   (strlen(cfg->linker) > 0) ? cfg->linker : "ld",
				   cfg->ldscript, output);
	}
	else
	{
		// Используем компилятор как линкер
		cmd_len = snprintf(cmd, sizeof(cmd), "%s -o %s",
				   linker, output);
	}

	// Добавляем все объектные файлы
	for (int i = 0; i < obj_count; i++)
	{
		cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
		if (cmd_len >= sizeof(cmd) - 4096)
		{
			fprintf(stderr, "Error: link command too long\n");
			LOG_ERROR("Link command too long");
			return 1;
		}
	}

	// Добавляем ldflags (могут содержать -L пути к библиотекам)
	if (strlen(cfg->ldflags) > 0)
	{
		cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->ldflags);
	}

	// Добавляем библиотеки В КОНЦЕ (важно для линкера!)
	if (strlen(cfg->libs) > 0)
	{
		cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->libs);
	}

	int ret = run_command(cmd, cfg->verbose);
	if (ret == 0)
	{
		LOG_SUCCESS("Successfully linked executable: %s", output);
	}
	else
	{
		LOG_ERROR("Failed to link executable: %s", output);
	}

	return ret;
}
