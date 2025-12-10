#include "argparser.h"
#include "buildconfig.h"
#include <stdio.h>
#include <string.h>

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
	printf("  --ld <linker>        Linker (default: ld or g++)\n");
	printf("  --cflags <flags>     C/C++ compiler flags\n");
	printf("  --asmflags <flags>   Assembler flags\n");
	printf("  --ldflags <flags>    Linker flags\n");
	printf("  --includes <paths>   Include paths\n");
	printf("  --libs <libraries>   Libraries to link (e.g., -lm -lpthread)\n");
	printf("  --ldscript <file>    Linker script file\n");
	printf("  --output <file>      Output file (archive or executable)\n");
	printf("  --type <type>        Output type: archive, exe (default: archive)\n");
	printf("  --log-dir <dir>      Directory for log files (creates YYYY-MM-DD.log)\n");
	printf("  --log-file <file>    Specific log file path\n");
	printf("  -v, --verbose        Verbose output\n");
	printf("  -f, --force          Force rebuild all files\n");
	printf("  -h, --help           Show this help\n");
}

int parse_arguments(int argc, char **argv, BuildConfig *cfg, char *cxx_compiler, char *output_type)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--src-dir") == 0 && i + 1 < argc)
		{
			strncpy(cfg->src_dir, argv[++i], sizeof(cfg->src_dir) - 1);
		}
		else if (strcmp(argv[i], "--build-dir") == 0 && i + 1 < argc)
		{
			strncpy(cfg->build_dir, argv[++i], sizeof(cfg->build_dir) - 1);
		}
		else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc)
		{
			strncpy(cfg->compiler, argv[++i], sizeof(cfg->compiler) - 1);
		}
		else if (strcmp(argv[i], "--cxx") == 0 && i + 1 < argc)
		{
			strncpy(cxx_compiler, argv[++i], 256 - 1);
		}
		else if (strcmp(argv[i], "--asm") == 0 && i + 1 < argc)
		{
			strncpy(cfg->assembler, argv[++i], sizeof(cfg->assembler) - 1);
		}
		else if (strcmp(argv[i], "--ar") == 0 && i + 1 < argc)
		{
			strncpy(cfg->archiver, argv[++i], sizeof(cfg->archiver) - 1);
		}
		else if (strcmp(argv[i], "--ld") == 0 && i + 1 < argc)
		{
			strncpy(cfg->linker, argv[++i], sizeof(cfg->linker) - 1);
		}
		else if (strcmp(argv[i], "--cflags") == 0 && i + 1 < argc)
		{
			strncpy(cfg->cflags, argv[++i], sizeof(cfg->cflags) - 1);
		}
		else if (strcmp(argv[i], "--asmflags") == 0 && i + 1 < argc)
		{
			strncpy(cfg->asmflags, argv[++i], sizeof(cfg->asmflags) - 1);
		}
		else if (strcmp(argv[i], "--ldflags") == 0 && i + 1 < argc)
		{
			strncpy(cfg->ldflags, argv[++i], sizeof(cfg->ldflags) - 1);
		}
		else if (strcmp(argv[i], "--includes") == 0 && i + 1 < argc)
		{
			strncpy(cfg->includes, argv[++i], sizeof(cfg->includes) - 1);
		}
		else if (strcmp(argv[i], "--libs") == 0 && i + 1 < argc)
		{
			strncpy(cfg->libs, argv[++i], sizeof(cfg->libs) - 1);
		}
		else if (strcmp(argv[i], "--ldscript") == 0 && i + 1 < argc)
		{
			strncpy(cfg->ldscript, argv[++i], sizeof(cfg->ldscript) - 1);
		}
		else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
		{
			strncpy(cfg->output, argv[++i], sizeof(cfg->output) - 1);
		}
		else if (strcmp(argv[i], "--type") == 0 && i + 1 < argc)
		{
			strncpy(output_type, argv[++i], 32 - 1);
		}
		else if (strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc)
		{
			strncpy(cfg->log_dir, argv[++i], sizeof(cfg->log_dir) - 1);
			cfg->enable_log = 1;
		}
		else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc)
		{
			strncpy(cfg->log_file, argv[++i], sizeof(cfg->log_file) - 1);
			cfg->enable_log = 1;
		}
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
		{
			cfg->verbose = 1;
		}
		else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0)
		{
			cfg->force_rebuild = 1;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			print_usage(argv[0]);
			return 1; // special return to indicate help requested
		}
	}
	return 0;
}
