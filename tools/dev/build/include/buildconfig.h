#pragma once
#include <stdio.h>

#define MAX_PATH 4096
#define MAX_NAME 256
#define MAX_FILES 2048
#define MAX_ARGS 128
#define MAX_CMD 8192

typedef struct
{
	char src_dir[MAX_PATH];
	char build_dir[MAX_PATH];
	char compiler[MAX_NAME];
	char assembler[MAX_NAME];
	char archiver[MAX_NAME];
	char linker[MAX_NAME];
	char cflags[2048];
	char asmflags[2048];
	char ldflags[2048];
	char includes[2048];
	char libs[2048];
	char output[MAX_PATH];
	char ldscript[MAX_PATH];
	char log_dir[MAX_PATH];
	char log_file[MAX_PATH];
	int verbose;
	int force_rebuild;
	int enable_log;
} BuildConfig;
