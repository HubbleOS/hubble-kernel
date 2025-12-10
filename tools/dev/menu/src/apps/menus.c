#include <stdlib.h>
#include <string.h>
#include "menus.h"
#include "runner.h"
#include "screen.h"
#include <ui/modal.h>
#include <ui/window.h>
#include <ui/main.h>
#include <games/game.h>
#include <config/config.h>

static void save_config()
{
	config.save("../../../.config");
}

void show_save_modal()
{
	ModalButton buttons[] = {
	    {"Save", save_config},
	    {"Don't save", NULL},
	};
	show_modal_with_buttons("Save changes?", buttons, COUNT(buttons));
}

void show_save_modal2()
{
	ModalButton buttons[] = {
	    {"Save", NULL},
	    {"Save", NULL},
	    {"Save", NULL},
	    {"Save", NULL},
	    {"Don't save", NULL},
	};
	show_modal_with_buttons("bla bla bla?", buttons, COUNT(buttons));
}

// ────────────────────────────── Games ──────────────────────────────

static MenuItem games_items[16];
static int games_items_count = 0;

static void games_items_init(void)
{
	for (int i = 0; i < games_count; i++)
	{
		games_items[i].label = games[i].name;
		games_items[i].action = games[i].run;
	}
	games_items_count = games_count;
}

void show_games_menu()
{
	UIWindow *win = CREATE_WIN(LINES, COLS, 0, 0, "Games");

	games_items_init();
	UIElement *games_el = MAKE_MENU(ACTION_MENU, "Games", games_items, games_items_count);
	ADD_ELEMENT(win, games_el);

	screen.flush();
	SET_FOCUS(win);
	DRAW_WIN(win);

	int ch;
	while ((ch = getch()) != 27)
	{
		if (ch == 'q')
		{
			break;
		}

		WIN_HANDLE_KEY(win, ch);
		DRAW_WIN(win);
	}

	DESTROY_WIN(win);
}

// ────────────────────────────── Main menu ──────────────────────────────

MenuItem main_items[] = {
    {"make build", run_build},
    {"make host-run", run_qemu_default},
    {"make clean", run_clean},
    {"make run", run_run},
    {"Config", show_save_modal},
    {"Games", show_games_menu},
    {"Option 1", show_save_modal2},
};

Menu main_menu = {
    .items = main_items,
    .count = COUNT(main_items),
};

static ChecklistItem checklist_items[] = {
    {"item1", true},
    {"item2", true},
    {"item3", true},
    {"item4", true},
    {"item5", true},
    {"item6", true},
    {"item7", true},
    {"item8", true},
    {"item9", true},
    {"item10", true},
    {"item11", true},
    {"item12", true},
    {"item13", true},
    {"item14", true},
    {"item15", true},
    {"item16", true},
    {"item17", true},
    {"item18", true},
    {"item19", true},
    {"item20", true},
    {"item21", true},
    {"item22", true},
    {"item23", true},
    {"item24", true},
    {"item25", true},
    {"item26", true},
    {"item27", true},
    {"item28", true},
    {"item29", true},
    {"item30", true},
    {"item31", true},
    {"item32", true},
    {"item33", true},
    {"item34", true},
    {"item35", true},
    {"item36", true},
    {"item37", true},
    {"item38", true},
    {"item39", true},
    {"item40", true},
};

Checklist checklists = {
    .items = checklist_items,
    .count = COUNT(checklist_items),
};
