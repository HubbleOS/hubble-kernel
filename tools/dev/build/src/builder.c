#include "builder.h"
#include "buildconfig.h"
#include "fs.h"
#include "log.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include "tblgen.h"

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
	case LANG_TBL:
	{
		printf("Generating header from TBL: %s\n", src);
		LOG_INFO("Generating header from TBL: %s", src);

		// формируем путь к .h
		char header_path[MAX_PATH];
		strncpy(header_path, src, sizeof(header_path) - 1);
		header_path[sizeof(header_path) - 1] = '\0';

		char *dot = strrchr(header_path, '.');
		if (dot)
		{
			strcpy(dot, ".h"); // заменяем расширение на .h
		}
		else
		{
			// если нет точки, просто добавляем .h
			strncat(header_path, ".h", sizeof(header_path) - strlen(header_path) - 1);
		}

		ret = generate_tbl2header(src, header_path, TBL_SYSCALL);
		break;
	}
	default:
		fprintf(stderr, "Error: Unknown source language for %s\n", src);
		LOG_ERROR("Unknown source language for %s", src);
		return 1;
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
	int use_response_file = 0;
	char response_file[MAX_PATH];

	printf("Creating archive: %s\n", output);
	LOG_INFO("Creating archive: %s (%d object files)", output, obj_count);

	char output_dir[MAX_PATH];
	strncpy(output_dir, output, sizeof(output_dir) - 1);
	char *last_slash = strrchr(output_dir, '/');
	if (last_slash)
		*last_slash = 0;
	mkdir_p(output_dir);

	// Проверяем, нужен ли response file (если много объектных файлов)
	if (obj_count > 50)
	{
		use_response_file = 1;
		snprintf(response_file, sizeof(response_file), "%s.rsp", output);

		FILE *rsp = fopen(response_file, "w");
		if (!rsp)
		{
			fprintf(stderr, "Error: cannot create response file %s\n", response_file);
			LOG_ERROR("Cannot create response file: %s", response_file);
			return 1;
		}

		// Записываем все объектные файлы в response file
		for (int i = 0; i < obj_count; i++)
		{
			fprintf(rsp, "%s\n", obj_files[i]);
		}

		fclose(rsp);
		LOG_INFO("Created response file: %s", response_file);
	}

	// Формируем команду создания архива
	if (use_response_file)
	{
		// Используем response file
		snprintf(cmd, sizeof(cmd), "%s rcs %s @%s", cfg->archiver, output, response_file);
	}
	else
	{
		// Обычная команда без response file
		cmd_len = snprintf(cmd, sizeof(cmd), "%s rcs %s", cfg->archiver, output);

		for (int i = 0; i < obj_count; i++)
		{
			int written = snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
			if (written < 0 || cmd_len + written >= sizeof(cmd) - 4096)
			{
				fprintf(stderr, "Error: archive command too long, use fewer files or increase MAX_CMD\n");
				LOG_ERROR("Archive command too long");
				return 1;
			}
			cmd_len += written;
		}
	}

	int ret = run_command(cmd, cfg->verbose);

	// Удаляем response file после использования
	if (use_response_file)
	{
		remove(response_file);
	}

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
	int use_response_file = 0;
	char response_file[MAX_PATH];

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

	// Проверяем, нужен ли response file (если много объектных файлов)
	if (obj_count > 50)
	{
		use_response_file = 1;
		snprintf(response_file, sizeof(response_file), "%s.rsp", output);

		FILE *rsp = fopen(response_file, "w");
		if (!rsp)
		{
			fprintf(stderr, "Error: cannot create response file %s\n", response_file);
			LOG_ERROR("Cannot create response file: %s", response_file);
			return 1;
		}

		// Записываем все объектные файлы в response file
		for (int i = 0; i < obj_count; i++)
		{
			fprintf(rsp, "%s\n", obj_files[i]);
		}

		// Добавляем ldflags и libs
		if (strlen(cfg->ldflags) > 0)
		{
			fprintf(rsp, "%s\n", cfg->ldflags);
		}
		if (strlen(cfg->libs) > 0)
		{
			fprintf(rsp, "%s\n", cfg->libs);
		}

		fclose(rsp);
		LOG_INFO("Created response file: %s", response_file);
	}

	// Формируем команду линковки
	if (use_response_file)
	{
		// Используем response file
		if (strlen(cfg->ldscript) > 0)
		{
			snprintf(cmd, sizeof(cmd), "%s -T%s -o %s @%s",
				 (strlen(cfg->linker) > 0) ? cfg->linker : "ld",
				 cfg->ldscript, output, response_file);
		}
		else
		{
			snprintf(cmd, sizeof(cmd), "%s -o %s @%s",
				 linker, output, response_file);
		}
	}
	else
	{
		// Обычная команда без response file
		if (strlen(cfg->ldscript) > 0)
		{
			cmd_len = snprintf(cmd, sizeof(cmd), "%s -T%s -o %s",
					   (strlen(cfg->linker) > 0) ? cfg->linker : "ld",
					   cfg->ldscript, output);
		}
		else
		{
			cmd_len = snprintf(cmd, sizeof(cmd), "%s -o %s",
					   linker, output);
		}

		// Добавляем все объектные файлы
		for (int i = 0; i < obj_count; i++)
		{
			int written = snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
			if (written < 0 || cmd_len + written >= sizeof(cmd) - 4096)
			{
				fprintf(stderr, "Error: link command too long, use fewer files or increase MAX_CMD\n");
				LOG_ERROR("Link command too long");
				return 1;
			}
			cmd_len += written;
		}

		// Добавляем ldflags
		if (strlen(cfg->ldflags) > 0)
		{
			cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->ldflags);
		}

		// Добавляем библиотеки В КОНЦЕ
		if (strlen(cfg->libs) > 0)
		{
			cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->libs);
		}
	}

	int ret = run_command(cmd, cfg->verbose);

	// Удаляем response file после использования
	if (use_response_file)
	{
		remove(response_file);
	}

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
