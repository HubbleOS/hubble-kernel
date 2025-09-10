#pragma once

#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ui/menu.h>
#include <ui/main.h>

void app();

// // ─────────────────────────────────────────────────────────────────────────────
// // Terminal size warning

void show_terminal_size_warning_window();

// // ─────────────────────────────────────────────────────────────────────────────
// // Modal example actions

// void on_save();
// void on_dont_save();
// void fun();

void show_save_modal();
// void show_save_modal2();

// void start_game();

// ─────────────────────────────────────────────────────────────────────────── ──
// Menu setup

extern MenuItem main_items[];

typedef struct
{
	ChecklistItem *items;
	size_t count;
} Checklist;

typedef struct
{
	MenuItem *items;
	size_t count;
} Menu;

extern Checklist checklists;
extern Menu main_menu;
