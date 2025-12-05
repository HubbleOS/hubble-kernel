#include "exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log/log.h"

int run_command(const char *cmd, int verbose)
{
	if (verbose)
	{
		printf("%s\n", cmd);
	}
	LOG_INFO("Executing: %s", cmd);

	int ret = system(cmd);
	int exit_code = WEXITSTATUS(ret);

	if (exit_code != 0)
	{
		LOG_ERROR("Command failed with exit code %d: %s", exit_code, cmd);
	}

	return exit_code;
}
