#include "action_list.h"

#include "snake/snake.h"

static AppState exit_run(App *app)
{
	(void)app;
	return STATE_EXIT;
}

Action action_list[] = {
    {
	.name = "Classic Snake",
	.run = snake_classic_run,
    },
    {
	.name = "Exit",
	.run = exit_run,
    },
};

const int action_count = sizeof(action_list) / sizeof(action_list[0]);
