#include "../app.h"
#include <ncurses.h>

AppState tetris_classic_run(App *app);

void tetris_reset();

bool tetris_is_game_over();
int tetris_get_score();

int tetris_handle_input();
void tetris_move();
void tetris_draw(WINDOW *win);
