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

	snprintf(cmd, len,
		 "qemu-system-%s "
		 "-M pc "
		 "-cpu Haswell "
		 "-m %d -smp %d "
		 "-drive if=pflash,format=raw,readonly=on,file=%s "
		 "-hda fat:rw:%s "
		 "-serial stdio",
		 opts->arch,
		 opts->mem,
		 opts->smp,
		 ovmf_path,
		 opts->iso_path);
}

int qemu_run(const QemuOptions *opts)
{
	char cmd[512];
	qemu.build(opts, cmd, sizeof(cmd));
	printf("Running: %s\n", cmd);
	return system(cmd);
}

Qemu qemu = {
    .build = qemu_build_command,
    .run = qemu_run,
};
