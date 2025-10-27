#include "runner.h"
#include <apps/screen.h>
#include <qemu/qemu.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static void system_command(const char *cmd)
{
	screen.end();
	int ret = system(cmd);

	if (ret == -1)
	{
		printf("\n[Failed to start command]\n");
		getchar();
	}
	else if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0)
	{
		printf("\nCommand failed (exit code %d). Press Enter...\n", WEXITSTATUS(ret));
		getchar();
	}
	else if (WIFSIGNALED(ret))
	{
		printf("\nCommand killed by signal %d. Press Enter...\n", WTERMSIG(ret));
		getchar();
	}

	// return control to ncurses
	screen.reset();
	screen.init();
}

void run_cmdf(const char *fmt, ...)
{
	char cmd[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(cmd, sizeof(cmd), fmt, args);
	va_end(args);

	system_command(cmd);
}

void run_qemu(const char *iso, const char *arch, int mem)
{
	run_cmdf("make -C qemu run ISO=%s ARCH=%s MEM=%d", iso, arch, mem);
}

void run_qemu_default(void)
{
	extern QemuConfig qemu_config;
	run_qemu(qemu_config.iso, qemu_config.arch, qemu_config.mem);
}

#include <docker/docker.h>

void run_build(void)
{
	run_cmdf("make -C ../../ %s", docker.enabled ? "docker-build" : "build");
}

void run_run(void)
{
	run_cmdf("make -C ../../ %s", docker.enabled ? "docker-run" : "run");
}

void run_clean(void)
{
	run_cmdf("make -C ../../ clean");
}
