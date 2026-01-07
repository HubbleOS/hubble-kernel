#include <app.h>

#include <ncurses.h>

void tetris_classic_run();

void tetris_reset();

bool tetris_is_game_over();
int tetris_get_score();

int tetris_handle_input();
void tetris_move();
void tetris_draw(WINDOW *win);
