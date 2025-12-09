#pragma once

#include "buildconfig.h"

typedef enum
{
	LANG_C,
	LANG_CPP,
	LANG_ASM
} SourceLang;

int build_file(const char *src, const char *obj, SourceLang lang, const BuildConfig *cfg, const char *cxx_compiler);
int create_archive(const char *output, char obj_files[][4096], int obj_count, const BuildConfig *cfg);
