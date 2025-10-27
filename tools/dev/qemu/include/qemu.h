#pragma once

#include <stdlib.h>

typedef struct
{
	char arch[16];	    // ARCH
	int mem;	    // Memory in MB
	int smp;	    // CPU number of cores
	char iso_path[256]; // ISO path
	int debug_port;
} QemuOptions;

typedef struct
{
	void (*build)(const QemuOptions *opts, char *cmd, size_t len);
	int (*run)(const QemuOptions *opts);
} Qemu;

extern Qemu qemu;
