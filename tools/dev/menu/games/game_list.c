#include "game.h"

void snake_run(void);
void tetris_run(void);

Game games[] = {
    {"Snake", snake_run},
    {"Tetris", tetris_run},
};

int games_count = sizeof(games) / sizeof(games[0]);
