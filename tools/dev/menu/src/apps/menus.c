#include <stdlib.h>
#include <string.h>
#include "menus.h"
#include "runner.h"
#include "screen.h"
#include <ui/modal.h>
#include <ui/window.h>
#include <ui/main.h>
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

// ────────────────────────────── Main menu ──────────────────────────────

MenuItem main_items[] = {
    {"make build", run_build},
    {"make clean", run_clean},
    {"make run", run_run},
    {"Config", show_save_modal},
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
