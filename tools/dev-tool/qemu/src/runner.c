#include "qemu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/*

#OS specific handling for ISO path
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
	WIN_ISO_DIR=$(cygpath -w "$ISO_DIR")
	QEMU_ISO_PATH="$WIN_ISO_DIR"
	;;
Linux | Darwin)
	QEMU_ISO_PATH="$ISO_DIR"
	;;
*)
	echo "Unsupported OS: $(uname -s)"
	exit 1
	;;
esac

*/

#define NEXT_VALUE_OR_DEFAULT(target, default_value)                      \
	if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0)           \
	{                                                                 \
		strcpy(target, argv[++i]);                                \
	}                                                                 \
	else                                                              \
	{                                                                 \
		printf("Using default %s: %s\n", #target, default_value); \
	}

#define NEXT_INT_OR_DEFAULT(target, default_value)                        \
	if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0)           \
	{                                                                 \
		target = atoi(argv[++i]);                                 \
	}                                                                 \
	else                                                              \
	{                                                                 \
		printf("Using default %s: %d\n", #target, default_value); \
	}

int main(int argc, char **argv)
{
	QemuOptions opts = {
	    .arch = "x86_64",
	    .mem = 512,
	    .smp = 2,
	    .debug_port = 1000,
	    .iso_path = "out/x86/iso/",
	};

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--iso") == 0)
		{
			NEXT_VALUE_OR_DEFAULT(opts.iso_path, opts.iso_path);
		}
		else if (strcmp(argv[i], "--arch") == 0)
		{
			NEXT_VALUE_OR_DEFAULT(opts.arch, opts.arch);
		}
		else if (strcmp(argv[i], "--mem") == 0)
		{
			NEXT_INT_OR_DEFAULT(opts.mem, opts.mem);
		}
		else if (strcmp(argv[i], "--smp") == 0)
		{
			NEXT_INT_OR_DEFAULT(opts.smp, opts.smp);
		}
		else if (strcmp(argv[i], "--debug") == 0)
		{
			NEXT_INT_OR_DEFAULT(opts.debug_port, opts.debug_port);
		}
		else
		{
			printf("Unknown argument or missing value: %s\n", argv[i]);
		}
	}

	if (access(opts.iso_path, F_OK) != 0)
	{
		fprintf(stderr, "ISO directory not found: %s\n", opts.iso_path);
		return 1;
	}

	return qemu.run(&opts);
}
