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

int main(int argc, char **argv)
{
	QemuOptions opts = {
	    .arch = "x86_64",
	    .mem = 1024,
	    .smp = 2,
	    .debug_port = 1000,
	    .iso_path = "../../../out/x86/iso/"};

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--iso") == 0 && i + 1 < argc)
		{
			strcpy(opts.iso_path, argv[++i]);
		}
		else if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc)
		{
			strcpy(opts.arch, argv[++i]);
		}
		else if (strcmp(argv[i], "--mem") == 0 && i + 1 < argc)
		{
			opts.mem = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--smp") == 0 && i + 1 < argc)
		{
			opts.smp = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--debug") == 0 && i + 1 < argc)
		{
			opts.debug_port = atoi(argv[++i]);
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
