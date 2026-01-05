#include "snake.h"
#include <ncurses.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../app.h"

/* ----------------- Input ----------------- */
int snake_handle_input();

void snake_apply_buffered_input();

/* ----------------- Move & Collision ----------------- */
void snake_move();

/* ----------------- Drawing ----------------- */
void snake_draw(WINDOW *win);

/* ----------------- Reset ----------------- */
void snake_reset();

/* ----------------- Getters ----------------- */
bool snake_is_game_over();
int snake_get_score();
