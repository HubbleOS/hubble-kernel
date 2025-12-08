#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#include "fs/fs.h"
#include "log/log.h"
#include "exec/exec.h"

#define MAX_PATH 4096
#define MAX_FILES 2048
#define MAX_ARGS 128
#define MAX_CMD 8192
#define MAX_NAME 256

typedef struct
{
	char src_dir[MAX_PATH];
	char build_dir[MAX_PATH];
	char compiler[MAX_NAME];
	char assembler[MAX_NAME];
	char archiver[MAX_NAME];
	char cflags[1024];
	char asmflags[1024];
	char includes[2048];
	char output[MAX_PATH];
	char log_dir[MAX_PATH];
	int verbose;
	int force_rebuild;
	int enable_log;
} BuildConfig;

static SourceFile c_files[MAX_FILES];
static SourceFile cpp_files[MAX_FILES];
static SourceFile asm_files[MAX_FILES];
static int c_count = 0;
static int cpp_count = 0;
static int asm_count = 0;

// ============================================
// Вспомогательные функции
// ============================================

void print_usage(const char *prog)
{
	printf("Usage: %s [options]\n", prog);
	printf("Options:\n");
	printf("  --src-dir <dir>      Source directory (default: .)\n");
	printf("  --build-dir <dir>    Build output directory (required)\n");
	printf("  --cc <compiler>      C compiler (default: gcc)\n");
	printf("  --cxx <compiler>     C++ compiler (default: g++)\n");
	printf("  --asm <assembler>    Assembler (default: nasm)\n");
	printf("  --ar <archiver>      Archiver (default: ar)\n");
	printf("  --cflags <flags>     C/C++ compiler flags\n");
	printf("  --asmflags <flags>   Assembler flags\n");
	printf("  --includes <paths>   Include paths\n");
	printf("  --output <file>      Output file (archive or executable)\n");
	printf("  --type <type>        Output type: archive, exe (default: archive)\n");
	printf("  --log-dir <dir>      Directory for log files (creates YYYY-MM-DD.log)\n");
	printf("  -v, --verbose        Verbose output\n");
	printf("  -f, --force          Force rebuild all files\n");
	printf("  -h, --help           Show this help\n");
}

int compile_file(const char *src, const char *obj, const BuildConfig *cfg, const char *compiler)
{
	char cmd[MAX_CMD];
	char obj_dir[MAX_PATH];

	// Создаём директорию для объектного файла
	strncpy(obj_dir, obj, sizeof(obj_dir) - 1);
	char *last_slash = strrchr(obj_dir, '/');
	if (last_slash)
	{
		*last_slash = 0;
		mkdir_p(obj_dir);
	}

	// Проверяем, нужна ли пересборка
	if (!cfg->force_rebuild && !needs_rebuild(src, obj))
	{
		if (cfg->verbose)
		{
			printf("Skipping %s (up to date)\n", src);
		}
		LOG_INFO("Skipping %s (up to date)", src);
		return 0;
	}

	printf("Compiling: %s\n", src);
	LOG_INFO("Compiling: %s -> %s", src, obj);

	snprintf(cmd, sizeof(cmd), "%s %s %s -c %s -o %s",
		 compiler, cfg->cflags, cfg->includes, src, obj);

	int ret = run_command(cmd, cfg->verbose);
	if (ret == 0)
	{
		LOG_SUCCESS("Successfully compiled: %s", src);
	}
	else
	{
		LOG_ERROR("Failed to compile: %s", src);
	}

	return ret;
}

int assemble_file(const char *src, const char *obj, const BuildConfig *cfg)
{
	char cmd[MAX_CMD];
	char obj_dir[MAX_PATH];

	strncpy(obj_dir, obj, sizeof(obj_dir) - 1);
	char *last_slash = strrchr(obj_dir, '/');
	if (last_slash)
	{
		*last_slash = 0;
		mkdir_p(obj_dir);
	}

	if (!cfg->force_rebuild && !needs_rebuild(src, obj))
	{
		if (cfg->verbose)
		{
			printf("Skipping %s (up to date)\n", src);
		}
		LOG_INFO("Skipping %s (up to date)", src);
		return 0;
	}

	printf("Assembling: %s\n", src);
	LOG_INFO("Assembling: %s -> %s", src, obj);

	snprintf(cmd, sizeof(cmd), "%s %s %s -o %s",
		 cfg->assembler, cfg->asmflags, src, obj);

	int ret = run_command(cmd, cfg->verbose);
	if (ret == 0)
	{
		LOG_SUCCESS("Successfully assembled: %s", src);
	}
	else
	{
		LOG_ERROR("Failed to assemble: %s", src);
	}

	return ret;
}

int create_archive(const char *output, char obj_files[][MAX_PATH], int obj_count, const BuildConfig *cfg)
{
	char cmd[MAX_CMD];
	int cmd_len;

	printf("Creating archive: %s\n", output);
	LOG_INFO("Creating archive: %s (%d object files)", output, obj_count);

	// Создаём директорию для выходного файла
	char output_dir[MAX_PATH];
	strncpy(output_dir, output, sizeof(output_dir) - 1);
	char *last_slash = strrchr(output_dir, '/');
	if (last_slash)
	{
		*last_slash = 0;
		mkdir_p(output_dir);
	}

	cmd_len = snprintf(cmd, sizeof(cmd), "%s rcs %s", cfg->archiver, output);

	for (int i = 0; i < obj_count; i++)
	{
		cmd_len += snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
		if (cmd_len >= sizeof(cmd) - MAX_PATH)
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

int main(int argc, char **argv)
{
	BuildConfig cfg = {
	    .src_dir = ".",
	    .build_dir = "",
	    .compiler = "gcc",
	    .assembler = "nasm",
	    .archiver = "ar",
	    .cflags = "",
	    .asmflags = "-f elf64",
	    .includes = "",
	    .output = "",
	    .log_dir = "",
	    .verbose = 0,
	    .force_rebuild = 0,
	    .enable_log = 0};

	char cxx_compiler[256] = "g++";
	char output_type[32] = "archive";

	// Парсинг аргументов
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--src-dir") == 0 && i + 1 < argc)
		{
			strncpy(cfg.src_dir, argv[++i], sizeof(cfg.src_dir) - 1);
		}
		else if (strcmp(argv[i], "--build-dir") == 0 && i + 1 < argc)
		{
			strncpy(cfg.build_dir, argv[++i], sizeof(cfg.build_dir) - 1);
		}
		else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc)
		{
			strncpy(cfg.compiler, argv[++i], sizeof(cfg.compiler) - 1);
		}
		else if (strcmp(argv[i], "--cxx") == 0 && i + 1 < argc)
		{
			strncpy(cxx_compiler, argv[++i], sizeof(cxx_compiler) - 1);
		}
		else if (strcmp(argv[i], "--asm") == 0 && i + 1 < argc)
		{
			strncpy(cfg.assembler, argv[++i], sizeof(cfg.assembler) - 1);
		}
		else if (strcmp(argv[i], "--ar") == 0 && i + 1 < argc)
		{
			strncpy(cfg.archiver, argv[++i], sizeof(cfg.archiver) - 1);
		}
		else if (strcmp(argv[i], "--cflags") == 0 && i + 1 < argc)
		{
			strncpy(cfg.cflags, argv[++i], sizeof(cfg.cflags) - 1);
		}
		else if (strcmp(argv[i], "--asmflags") == 0 && i + 1 < argc)
		{
			strncpy(cfg.asmflags, argv[++i], sizeof(cfg.asmflags) - 1);
		}
		else if (strcmp(argv[i], "--includes") == 0 && i + 1 < argc)
		{
			strncpy(cfg.includes, argv[++i], sizeof(cfg.includes) - 1);
		}
		else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
		{
			strncpy(cfg.output, argv[++i], sizeof(cfg.output) - 1);
		}
		else if (strcmp(argv[i], "--type") == 0 && i + 1 < argc)
		{
			strncpy(output_type, argv[++i], sizeof(output_type) - 1);
		}
		else if (strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc)
		{
			strncpy(cfg.log_dir, argv[++i], sizeof(cfg.log_dir) - 1);
			cfg.enable_log = 1;
		}
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
		{
			cfg.verbose = 1;
		}
		else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0)
		{
			cfg.force_rebuild = 1;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			print_usage(argv[0]);
			return 0;
		}
	}

	// Инициализация логирования
	if (cfg.enable_log)
	{
		log_init(cfg.log_dir);
		LOG_INFO("BUILD_TOOL started");
		LOG_INFO("Source directory: %s", cfg.src_dir);
		LOG_INFO("Build directory: %s", cfg.build_dir);
		LOG_INFO("Compiler: %s", cfg.compiler);
		LOG_INFO("C++ Compiler: %s", cxx_compiler);
		LOG_INFO("Assembler: %s", cfg.assembler);
	}

	// Проверка обязательных параметров
	if (strlen(cfg.build_dir) == 0)
	{
		fprintf(stderr, "Error: --build-dir is required\n");
		LOG_ERROR("Missing required parameter: --build-dir");
		print_usage(argv[0]);
		log_close();
		return 1;
	}

	// Создаём build директорию
	mkdir_p(cfg.build_dir);

	// Сканируем исходники
	printf("Scanning source files in: %s\n", cfg.src_dir);
	LOG_INFO("Scanning source files in: %s", cfg.src_dir);

	scan_directory(cfg.src_dir, ".c", c_files, &c_count, MAX_FILES);
	scan_directory(cfg.src_dir, ".cpp", cpp_files, &cpp_count, MAX_FILES);
	scan_directory(cfg.src_dir, ".asm", asm_files, &asm_count, MAX_FILES);

	printf("Found: %d C files, %d C++ files, %d ASM files\n", c_count, cpp_count, asm_count);
	LOG_INFO("Found: %d C files, %d C++ files, %d ASM files", c_count, cpp_count, asm_count);

	if (c_count == 0 && cpp_count == 0 && asm_count == 0)
	{
		fprintf(stderr, "Warning: No source files found\n");
		LOG_WARN("No source files found in %s", cfg.src_dir);
		log_close();
		return 0;
	}

	// Массив для объектных файлов
	static char obj_files[MAX_FILES][MAX_PATH];
	int obj_count = 0;

	// Компиляция C файлов
	LOG_INFO("Starting C compilation phase");
	for (int i = 0; i < c_count; i++)
	{
		get_obj_path(c_files[i].path, cfg.src_dir, cfg.build_dir, obj_files[obj_count], MAX_PATH);
		if (compile_file(c_files[i].path, obj_files[obj_count], &cfg, cfg.compiler) != 0)
		{
			fprintf(stderr, "Error compiling %s\n", c_files[i].path);
			log_close();
			return 1;
		}
		obj_count++;
	}

	// Компиляция C++ файлов
	LOG_INFO("Starting C++ compilation phase");
	for (int i = 0; i < cpp_count; i++)
	{
		get_obj_path(cpp_files[i].path, cfg.src_dir, cfg.build_dir, obj_files[obj_count], MAX_PATH);
		if (compile_file(cpp_files[i].path, obj_files[obj_count], &cfg, cxx_compiler) != 0)
		{
			fprintf(stderr, "Error compiling %s\n", cpp_files[i].path);
			log_close();
			return 1;
		}
		obj_count++;
	}

	// Ассемблирование
	LOG_INFO("Starting assembly phase");
	for (int i = 0; i < asm_count; i++)
	{
		get_obj_path(asm_files[i].path, cfg.src_dir, cfg.build_dir, obj_files[obj_count], MAX_PATH);
		if (assemble_file(asm_files[i].path, obj_files[obj_count], &cfg) != 0)
		{
			fprintf(stderr, "Error assembling %s\n", asm_files[i].path);
			log_close();
			return 1;
		}
		obj_count++;
	}

	// Создание выходного файла
	if (strlen(cfg.output) > 0)
	{
		if (strcmp(output_type, "archive") == 0)
		{
			if (create_archive(cfg.output, obj_files, obj_count, &cfg) != 0)
			{
				log_close();
				return 1;
			}
		}
	}

	printf("✅ Build completed successfully\n");
	LOG_SUCCESS("Build completed successfully");
	LOG_INFO("Total object files created: %d", obj_count);

	log_close();
	return 0;
}
