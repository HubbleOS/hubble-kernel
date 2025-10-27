#pragma once

#include <stdbool.h>

typedef struct
{
	bool enabled;
} DockerConfig;

extern DockerConfig docker;
