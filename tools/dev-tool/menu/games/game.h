#pragma once

typedef struct Game
{
	const char *name;
	void (*run)(void);
} Game;

extern Game games[];
extern int games_count;
