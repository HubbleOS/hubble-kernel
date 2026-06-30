#include "fs.h"
#include "log.h"
#include "exec.h"
#include "builder.h"
#include "argparser.h"
#include "buildconfig.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static SourceFile c_files[MAX_FILES];
static SourceFile cpp_files[MAX_FILES];
static SourceFile asm_files[MAX_FILES];
static SourceFile tbl_files[MAX_FILES];

static int c_count = 0;
static int cpp_count = 0;
static int asm_count = 0;
static int tbl_count = 0;

typedef struct {
	char src[MAX_PATH];
	char obj[MAX_PATH];
	SourceLang lang;
} CompileJob;

static void build_command_string(const CompileJob *job, const BuildConfig *cfg,
				 const char *cxx_compiler, char *out, size_t out_size)
{
	switch (job->lang)
	{
	case LANG_C:
		snprintf(out, out_size, "%s -MMD -MP -fdiagnostics-color=always %s %s -c %s -o %s",
			 cfg->compiler, cfg->cflags, cfg->includes, job->src, job->obj);
		break;
	case LANG_CPP:
		snprintf(out, out_size, "%s -MMD -MP -fdiagnostics-color=always %s %s -c %s -o %s",
			 cxx_compiler, cfg->cflags, cfg->includes, job->src, job->obj);
		break;
	case LANG_ASM:
		snprintf(out, out_size, "%s %s %s -o %s",
			 cfg->assembler, cfg->asmflags, job->src, job->obj);
		break;
	default:
		out[0] = '\0';
		break;
	}
}

int main(int argc, char **argv)
{
	BuildConfig cfg = {0};
	cfg.jobs = 1;
	char cxx_compiler[256] = "g++";
	char output_type[32] = "executable";

	if (parse_arguments(argc, argv, &cfg, cxx_compiler, output_type) != 0)
	{
		return 0;
	}

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
		LOG_INFO("Parallel jobs: %d", cfg.jobs);
	}

	if (strlen(cfg.build_dir) == 0)
	{
		fprintf(stderr, "Error: --build-dir is required\n");
		LOG_ERROR("Missing required parameter: --build-dir");
		print_usage(argv[0]);
		log_close();
		return 1;
	}

	mkdir_p(cfg.build_dir);

	LOG_INFO("Scanning source files in: %s", cfg.src_dir);

	scan_directory(cfg.src_dir, ".c", c_files, &c_count, MAX_FILES);
	scan_directory(cfg.src_dir, ".cpp", cpp_files, &cpp_count, MAX_FILES);
	scan_directory(cfg.src_dir, ".asm", asm_files, &asm_count, MAX_FILES);
	scan_directory(cfg.src_dir, ".tbl", tbl_files, &tbl_count, MAX_FILES);

	LOG_INFO("Found: %d C files, %d C++ files, %d ASM files, %d TBL files",
		 c_count, cpp_count, asm_count, tbl_count);

	if (c_count == 0 && cpp_count == 0 && asm_count == 0)
	{
		LOG_WARN("No source files found in %s", cfg.src_dir);
		log_close();
		return 0;
	}

	// Process TBL files first (must be serial, they generate headers)
	for (int i = 0; i < tbl_count; i++)
	{
		char header_path[MAX_PATH];
		get_obj_path(tbl_files[i].path, cfg.src_dir, cfg.build_dir,
			     header_path, MAX_PATH);

		char *dot = strrchr(header_path, '.');
		if (dot)
			strcpy(dot, ".h");
		else
			strncat(header_path, ".h", MAX_PATH - strlen(header_path) - 1);

		if (build_file(tbl_files[i].path, header_path, LANG_TBL, &cfg, cxx_compiler) != 0)
		{
			fprintf(stderr, "Error generating header from %s\n", tbl_files[i].path);
			log_close();
			return 1;
		}
	}

	// Collect compile jobs for C, C++, and ASM files
	static CompileJob jobs[MAX_FILES];
	int job_count = 0;
	static char obj_files[MAX_FILES][MAX_PATH];
	int obj_count = 0;

	SourceFile *src_groups[] = {c_files, cpp_files, asm_files};
	int src_counts[] = {c_count, cpp_count, asm_count};
	SourceLang src_langs[] = {LANG_C, LANG_CPP, LANG_ASM};

	for (int g = 0; g < 3; g++)
	{
		for (int i = 0; i < src_counts[g]; i++)
		{
			char obj_path[MAX_PATH];
			get_obj_path(src_groups[g][i].path, cfg.src_dir, cfg.build_dir,
				     obj_path, MAX_PATH);

			// Create output directory and check rebuild status (do in parent)
			char obj_dir[MAX_PATH];
			strncpy(obj_dir, obj_path, sizeof(obj_dir) - 1);
			char *last_slash = strrchr(obj_dir, '/');
			if (last_slash)
				*last_slash = 0;
			mkdir_p(obj_dir);

			if (!cfg.force_rebuild && !needs_rebuild(src_groups[g][i].path, obj_path))
			{
				if (cfg.verbose)
					LOG_INFO("Skipping %s (up to date)", src_groups[g][i].path);
				strncpy(obj_files[obj_count], obj_path, MAX_PATH);
				obj_count++;
				continue;
			}

			strncpy(jobs[job_count].src, src_groups[g][i].path, MAX_PATH);
			strncpy(jobs[job_count].obj, obj_path, MAX_PATH);
			jobs[job_count].lang = src_langs[g];
			strncpy(obj_files[obj_count], obj_path, MAX_PATH);
			obj_count++;
			job_count++;
		}
	}

	// Build compile command strings
	char *cmd_ptrs[MAX_FILES];
	char (*cmd_bufs)[MAX_CMD] = malloc(sizeof(char[MAX_CMD]) * job_count);
	if (!cmd_bufs)
	{
		fprintf(stderr, "Error: out of memory allocating command buffers\n");
		log_close();
		return 1;
	}
	for (int i = 0; i < job_count; i++)
	{
		build_command_string(&jobs[i], &cfg, cxx_compiler, cmd_bufs[i], MAX_CMD);
		cmd_ptrs[i] = cmd_bufs[i];
		LOG_INFO("Compiling: %s -> %s", jobs[i].src, jobs[i].obj);
	}

	// Execute compile jobs
	if (job_count > 0)
	{
		int failed = 0;
		if (cfg.jobs > 1)
		{
			char *src_ptrs[MAX_FILES];
			for (int i = 0; i < job_count; i++)
				src_ptrs[i] = jobs[i].src;
			failed = run_commands_parallel((const char **)cmd_ptrs,
						       (const char **)src_ptrs,
						       job_count, cfg.jobs, cfg.verbose);
		}
		else
		{
			for (int i = 0; i < job_count; i++)
			{
				if (run_command(cmd_ptrs[i], cfg.verbose) != 0)
				{
					LOG_ERROR("Failed to build: %s", jobs[i].src);
					failed++;
					break;
				}
			}
		}
		if (failed)
		{
			free(cmd_bufs);
			log_close();
			return 1;
		}
	}
	free(cmd_bufs);

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
		else if (strcmp(output_type, "exe") == 0 || strcmp(output_type, "executable") == 0)
		{
			if (link_executable(cfg.output, obj_files, obj_count, &cfg, cxx_compiler) != 0)
			{
				log_close();
				return 1;
			}
		}
		else if (strcmp(output_type, "module") == 0)
		{
			if (link_module(cfg.output, obj_files, obj_count, &cfg) != 0)
			{
				log_close();
				return 1;
			}
		}
		else if (strcmp(output_type, "objects") == 0)
		{
			printf("Objects compiled to: %s\n", cfg.build_dir);
		}
		else
		{
			fprintf(stderr, "Error: Unknown output type '%s'. Use 'archive', 'exe', or 'module'\n", output_type);
			LOG_ERROR("Unknown output type: %s", output_type);
			log_close();
			return 1;
		}
	}

	LOG_SUCCESS("Build completed successfully");
	LOG_INFO("Total object files created: %d", obj_count);

	log_close();
	return 0;
}
