#include "fs/fs.h"
#include "log/log.h"
#include "exec/exec.h"
#include "builder/builder.h"
#include "parser/argparser.h"
#include "buildconfig.h"

static SourceFile c_files[MAX_FILES];
static SourceFile cpp_files[MAX_FILES];
static SourceFile asm_files[MAX_FILES];
static int c_count = 0;
static int cpp_count = 0;
static int asm_count = 0;

int main(int argc, char **argv)
{
	BuildConfig cfg = {0};
	char cxx_compiler[256] = "g++";
	char output_type[32] = "archive";

	if (parse_arguments(argc, argv, &cfg, cxx_compiler, output_type) != 0)
	{
		return 0; // help или ошибка
	}

	// Инициализация логирования
	if (cfg.enable_log)
	{
		if (strlen(cfg.log_file) > 0)
			log_init_file(cfg.log_file);
		else
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

	SourceFile *all_files[] = {c_files, cpp_files, asm_files};
	int counts[] = {c_count, cpp_count, asm_count};
	SourceLang langs[] = {LANG_C, LANG_CPP, LANG_ASM};

	int obj_count = 0;
	static char obj_files[MAX_FILES][MAX_PATH];

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < counts[i]; j++)
		{
			get_obj_path(all_files[i][j].path, cfg.src_dir, cfg.build_dir, obj_files[obj_count], MAX_PATH);
			if (build_file(all_files[i][j].path, obj_files[obj_count], langs[i], &cfg, cxx_compiler) != 0)
			{
				fprintf(stderr, "Error building %s\n", all_files[i][j].path);
				log_close();
				return 1;
			}
			obj_count++;
		}
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
