#include "tblgen.h"
#include "fs.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

int generate_tbl2header(const char *src, const char *out, TblGenType type)
{
	FILE *in = fopen(src, "r");
	if (!in)
	{
		LOG_ERROR("Cannot open tbl file: %s", src);
		return 1;
	}

	// Создаём директорию для out
	char out_dir[MAX_PATH];
	strncpy(out_dir, out, sizeof(out_dir) - 1);
	char *p = strrchr(out_dir, '/');
	if (p)
		*p = 0;
	mkdir_p(out_dir);

	FILE *o = fopen(out, "w");
	if (!o)
	{
		LOG_ERROR("Cannot write header: %s", out);
		fclose(in);
		return 1;
	}

	fprintf(o, "/* Auto-generated from %s */\n\n", src);
	fprintf(o, "#pragma once\n\n");

	char line[512];

	switch (type)
	{
	case TBL_SYSCALL:
	{
		int num;
		char alias[128];

		while (fgets(line, sizeof(line), in))
		{
			if (line[0] == '#' || line[0] == '\n')
				continue;

			if (sscanf(line, "%d %127s", &num, alias) == 2)
			{
				fprintf(o, "#define SYS_%s %d\n", alias, num);
			}
		}
		break;
	}

	default:
		LOG_ERROR("Unsupported TblGenType: %d", type);
		fclose(in);
		fclose(o);
		return 1;
	}

	fclose(in);
	fclose(o);

	LOG_SUCCESS("Generated header: %s", out);
	return 0;
}
