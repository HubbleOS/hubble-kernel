#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <qemu args>\n", argv[0]);
		return 1;
	}

	char command[512] = "sh qemu.sh";
	for (int i = 1; i < argc; ++i)
	{
		strcat(command, " ");
		strcat(command, argv[i]);
	}

	system(command);
	return 0;
}
