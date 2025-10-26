#pragma once

typedef void (*power_fn_t)(void);

typedef struct PowerOps
{
	power_fn_t shutdown;
	power_fn_t reboot;
} PowerOps;
