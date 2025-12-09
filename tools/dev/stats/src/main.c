#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define MAX_LANGS 32

typedef struct
{
	const char *name;
	const char *exts[4]; // maximum 4 extensions per language
	int files;
	int blank;
	int comment;
	int code;
} LangStats;

LangStats langs[MAX_LANGS] = {
    {"C", {".c"}, 0, 0, 0, 0},
    {"C/C++ Header", {".h"}, 0, 0, 0, 0},
    {"C++", {".cpp"}, 0, 0, 0, 0},
    {"Assembly", {".asm", ".s", ".S"}, 0, 0, 0, 0},
    {"Makefile", {"Makefile"}, 0, 0, 0, 0},
    {"Markdown", {".md"}, 0, 0, 0, 0},
    {"Dockerfile", {"Dockerfile"}, 0, 0, 0, 0},
    {"YAML", {".yml", ".yaml"}, 0, 0, 0, 0},
    {"INI", {".ini"}, 0, 0, 0, 0},
    {NULL, {NULL}, 0, 0, 0, 0}};

void analyze_file(const char *path, LangStats *stats)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	stats->files++;
	char line[4096];
	int in_block_comment = 0;
	while (fgets(line, sizeof(line), f))
	{
		char *trim = line;
		while (*trim && (*trim == ' ' || *trim == '\t'))
			trim++;
		if (*trim == 0 || *trim == '\n')
		{
			stats->blank++;
		}
		else if (in_block_comment)
		{
			stats->comment++;
			if (strstr(trim, "*/"))
				in_block_comment = 0;
		}
		else if (strncmp(trim, "//", 2) == 0)
		{
			stats->comment++;
		}
		else if (strncmp(trim, "/*", 2) == 0)
		{
			stats->comment++;
			if (!strstr(trim, "*/"))
				in_block_comment = 1;
		}
		else
		{
			stats->code++;
		}
	}
	fclose(f);
}

void scan_directory(const char *dir)
{
	DIR *d = opendir(dir);
	if (!d)
		return;
	struct dirent *entry;
	char path[MAX_PATH];
	while ((entry = readdir(d)) != NULL)
	{
		if (entry->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
		struct stat st;
		if (stat(path, &st) == 0)
		{
			if (S_ISDIR(st.st_mode))
			{
				scan_directory(path);
			}
			else if (S_ISREG(st.st_mode))
			{
				for (int i = 0; langs[i].name != NULL; i++)
				{
					for (int e = 0; langs[i].exts[e] != NULL; e++)
					{
						size_t len = strlen(langs[i].exts[e]);
						if ((strcmp(langs[i].exts[e], "Makefile") == 0 && strcmp(entry->d_name, "Makefile") == 0) ||
						    (len <= strlen(entry->d_name) && strcmp(entry->d_name + strlen(entry->d_name) - len, langs[i].exts[e]) == 0))
						{
							analyze_file(path, &langs[i]);
							break;
						}
					}
				}
			}
		}
	}
	closedir(d);
}

int main(int argc, char **argv)
{
	const char *root_dir = ".";
	if (argc > 1)
		root_dir = argv[1];

	scan_directory(root_dir);

	int total_files = 0, total_blank = 0, total_comment = 0, total_code = 0;

	// 1. Считаем общий total_files
	for (int i = 0; langs[i].name != NULL; i++)
	{
		total_files += langs[i].files;
	}

	// 2. Выводим таблицу с процентами
	printf("-----------------------------------------------------------------------------------------\n");
	printf("%-28s %12s %12s %12s %12s %8s\n", "Language", "files", "blank", "comment", "code", "percent");
	printf("-----------------------------------------------------------------------------------------\n");

	for (int i = 0; langs[i].name != NULL; i++)
	{
		if (langs[i].files > 0)
		{
			double pct = (langs[i].files * 100.0) / total_files;
			printf("%-28s %12d %12d %12d %12d %7.2f%%\n",
			       langs[i].name,
			       langs[i].files,
			       langs[i].blank,
			       langs[i].comment,
			       langs[i].code,
			       pct);

			total_blank += langs[i].blank;
			total_comment += langs[i].comment;
			total_code += langs[i].code;
		}
	}

	printf("-----------------------------------------------------------------------------------------\n");
	printf("%-28s %12d %12d %12d %12d %7s\n", "SUM", total_files, total_blank, total_comment, total_code, "100%");
	printf("-----------------------------------------------------------------------------------------\n");

	return 0;
}
