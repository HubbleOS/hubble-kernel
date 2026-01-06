#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int run(const char *cmd)
{
	printf(">> %s\n", cmd);
	return system(cmd);
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s <output.img> <size_mb> <usr_dir>\n", argv[0]);
		return 1;
	}

	const char *img = argv[1];
	int size = atoi(argv[2]); // MB
	const char *usrdir = argv[3];

	char cmd[1024];

	// 1. Create empty image
	snprintf(cmd, sizeof(cmd),
		 "dd if=/dev/zero of=%s bs=1M count=%d",
		 img, size);
	if (run(cmd) != 0)
		return 1;

	// 2. Format FAT32
	snprintf(cmd, sizeof(cmd),
		 "mkfs.vfat -F 32 %s", img);
	if (run(cmd) != 0)
		return 1;

	// 3. Make dirs
	snprintf(cmd, sizeof(cmd),
		 "mmd -i %s ::/usr", img);
	run(cmd);

	snprintf(cmd, sizeof(cmd),
		 "mmd -i %s ::/usr/bin", img);
	run(cmd);

	// 4. Copy userland files
	snprintf(cmd, sizeof(cmd),
		 "mcopy -i %s -s %s/* ::/usr/bin/",
		 img, usrdir);
	if (run(cmd) != 0)
		return 1;

	printf("Image created successfully.\n");
	return 0;
}
