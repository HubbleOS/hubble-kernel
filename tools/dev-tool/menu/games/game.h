#pragma once

typedef struct Game
{
	const char *name;
	// int best_score;
	void (*run)(void);
} Game;

extern Game games[];
extern int games_count;
