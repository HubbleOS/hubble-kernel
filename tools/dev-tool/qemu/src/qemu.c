#include "qemu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

void qemu_build_command(const QemuOptions *opts, char *cmd, size_t len)
{
	char ovmf_path[PATH_MAX];

	if (!realpath("ovmf/OVMF_CODE.fd", ovmf_path))
	{

		fprintf(stderr, "OVMF_CODE.fd not found in ./ovmf/\n");
		exit(1);
	}

	int pos = 0;

	pos += snprintf(cmd + pos, len - pos, "qemu-system-%s ", opts->arch);

	// pos += snprintf(cmd + pos, len - pos, "-M pc ");
	pos += snprintf(cmd + pos, len - pos, "-M q35 ");
	pos += snprintf(cmd + pos, len - pos, "-cpu Haswell ");
	pos += snprintf(cmd + pos, len - pos, "-m %d ", opts->mem);
	pos += snprintf(cmd + pos, len - pos, "-smp %d ", opts->smp);

	// // Main disk
	pos += snprintf(cmd + pos, len - pos, "-drive file=%s,format=raw,index=0,media=disk,cache=none ", "../../../out/x86/disk.img");

	// FAT ISO-disk
	// pos += snprintf(cmd + pos, len - pos, "-drive file=fat:rw:%s,format=raw,index=1,media=disk ", opts->iso_path);
	pos += snprintf(cmd + pos, len - pos,
			"-drive file=fat:rw:%s,if=none,id=nvm-1 "
			"-device nvme,drive=nvm-1,serial=nvme-test ",
			opts->iso_path);
	// pos += snprintf(cmd + pos, len - pos, "-drive file=fat:rw:%s,format=raw,index=1,media=disk ", opts->iso_path);

	// OVMF
	pos += snprintf(cmd + pos, len - pos, "-drive if=pflash,format=raw,readonly=on,file=%s ", ovmf_path);

	// Serial
	pos += snprintf(cmd + pos, len - pos, "-serial stdio ");
}

int qemu_run(const QemuOptions *opts)
{
	char cmd[4096];
	qemu.build(opts, cmd, sizeof(cmd));
	printf("Running: %s\n", cmd);
	return system(cmd);
}

Qemu qemu = {
    .build = qemu_build_command,
    .run = qemu_run,
};
