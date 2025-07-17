#include <string.h>
#include "menu.h"
#include "ui.h"
#include "runner.h"

#define COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MAKE_MENU(type, title, items) make_menu(type, title, items, COUNT(items))

static MenuItem main_items[] = {
	{"Make", show_make_menu},
	{"List", show_checklist},
};

static MenuItem make_items[] = {
	{"host-run", act_qemu},
	{"build", act_build},
	{"run", act_run},
	{"clean", act_clean},
	{"help", act_help},
	{"flash", act_flash},
	{"docker-run", act_docker_run},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-build", act_docker_build},
	{"docker-clean", act_docker_clean},
};

static ChecklistItem checklist_items[] = {
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item4", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item4", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item4", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item4", false},
	{"item1", true},
	{"item2", false},
	{"item4", false},
	{"item3", false},
	{"item4", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item4", false},
	{"item1", true},
	{"item2", false},
	{"item4", false},
};

void show_main_menu()
{
	Menu m = MAKE_MENU(ACTION_MENU, "Main Menu", main_items);
	menu_loop(&m);
}

void show_make_menu()
{
	Menu m = MAKE_MENU(ACTION_MENU, "Make Menu", make_items);
	menu_loop(&m);
}

void show_checklist()
{
	Menu m = MAKE_MENU(CHECKLIST_MENU, "Checklist", checklist_items);
	menu_loop(&m);
}
