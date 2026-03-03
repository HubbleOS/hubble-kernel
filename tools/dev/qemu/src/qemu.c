#include "qemu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>

// helper: получить папку бинарника
void get_exe_dir(char *buf, size_t len)
{
	char path[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
#ifdef __APPLE__
	uint32_t size = sizeof(path);
	_NSGetExecutablePath(path, &size);
	n = strlen(path);
#endif
	if (n < 0)
	{
		perror("readlink");
		exit(1);
	}
	path[n] = '\0';
	strncpy(buf, dirname(path), len - 1);
	buf[len - 1] = '\0';
}

void qemu_build_command(const QemuOptions *opts, char *cmd, size_t len)
{
	char exe_dir[PATH_MAX];
	get_exe_dir(exe_dir, sizeof(exe_dir));

	char ovmf_path[PATH_MAX];
	snprintf(ovmf_path, sizeof(ovmf_path), "%s/ovmf/OVMF_CODE.fd", exe_dir);

	if (access(ovmf_path, F_OK) != 0)
	{
		fprintf(stderr, "OVMF_CODE.fd not found in %s\n", ovmf_path);
		exit(1);
	}

	int pos = 0;
	pos += snprintf(cmd + pos, len - pos, "qemu-system-%s ", opts->arch);

	pos += snprintf(cmd + pos, len - pos, "-M pc ");
	// pos += snprintf(cmd + pos, len - pos, "-M q35 ");
	pos += snprintf(cmd + pos, len - pos, "-cpu Haswell ");
	pos += snprintf(cmd + pos, len - pos, "-m %d ", opts->mem);
	pos += snprintf(cmd + pos, len - pos, "-smp %d ", opts->smp);

	// // Main disk
	pos += snprintf(cmd + pos, len - pos, "-drive file=%s,format=raw,index=0,media=disk,cache=none ", "out/disks/disk.img");

	// FAT ISO-disk
	// pos += snprintf(cmd + pos, len - pos,
	// 		"-drive file=fat:rw:%s,if=none,id=nvm-1 "
	// 		"-device nvme,drive=nvm-1,serial=nvme-test ",
	// 		opts->iso_path);
	pos += snprintf(cmd + pos, len - pos, "-drive file=fat:rw:%s,format=raw,index=1,media=disk ", opts->iso_path);

	// OVMF
	pos += snprintf(cmd + pos, len - pos, "-drive if=pflash,format=raw,readonly=on,file=%s ", ovmf_path);

	// Serial
	pos += snprintf(cmd + pos, len - pos, "-serial stdio ");

	// Debug
	// pos += snprintf(cmd + pos, len - pos, "-S -s ");
	// pos += snprintf(cmd + pos, len - pos, "-d int ");
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
