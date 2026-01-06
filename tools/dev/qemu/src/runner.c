#include "qemu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int parse_arguments(int argc, char **argv, QemuOptions *opts)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--iso") == 0 && i + 1 < argc)
		{
			strncpy(opts->iso_path, argv[++i], sizeof(opts->iso_path) - 1);
		}
		else if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc)
		{
			strncpy(opts->arch, argv[++i], sizeof(opts->arch) - 1);
		}
		else if (strcmp(argv[i], "--mem") == 0 && i + 1 < argc)
		{
			opts->mem = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--smp") == 0 && i + 1 < argc)
		{
			opts->smp = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--debug") == 0 && i + 1 < argc)
		{
			opts->debug_port = atoi(argv[++i]);
		}
		else
		{
			printf("Unknown argument or missing value: %s\n", argv[i]);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	QemuOptions opts = {
	    .arch = "x86_64",
	    .mem = 256,
	    .smp = 2,
	    .debug_port = 1000,
	    .iso_path = "out/build/x86/iso/",
	};

	if (parse_arguments(argc, argv, &opts) != 0)
	{
		fprintf(stderr, "Failed to parse arguments\n");
		return 1;
	}

	if (access(opts.iso_path, F_OK) != 0)
	{
		fprintf(stderr, "ISO directory not found: %s\n", opts.iso_path);
		return 1;
	}

	return qemu.run(&opts);
}
