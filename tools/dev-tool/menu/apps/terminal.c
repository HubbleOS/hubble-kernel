#include "app.h"

// ─────────────────────────────────────────────────────────────────────────────
// Modal example actions

void on_save() { /* save logic */ }
void on_dont_save() { /* don't save logic */ }
void fun();

void show_save_modal()
{
	ModalButton buttons[] = {
	    {"Save", fun},
	    {"Don't save", on_dont_save},
	};
	show_modal_with_buttons("Save changes?", buttons, COUNT(buttons));
}

void show_save_modal2()
{
	ModalButton buttons[] = {
	    {"Save", fun},
	    {"Save", fun},
	    {"Save", fun},
	    {"Save", fun},
	    {"Don't save", on_dont_save},
	};
	show_modal_with_buttons("bla bla bla?", buttons, COUNT(buttons));
}

void start_game()
{
	system("gcc games/snake.c -o games/snake -lncurses && ./games/snake");
}

MenuItem main_items[] = {
    {"make build", NULL},
    {"make host-run", NULL},
    {"make clean", NULL},
    {"make help", NULL},
    {"make run", NULL},
    {"make mkvars", NULL},

    {"Settings", NULL},

    {"Option 1", show_save_modal},
    {"Option 2", show_save_modal2},
    {"Option 3", fun},
    {"Games :)", start_game},
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

Menu main_menu = {
    .items = main_items,
    .count = COUNT(main_items),
};

QemuConfig qemu_config = {
    .iso = "../../../out/x86/iso/",
    .arch = "x86_64",
    .mem = 1024,
    .smp = 2,
    .debug_port = 1234,
};

void run_qemu(const char *iso, const char *arch, int mem)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "make -C qemu run ISO=%s ARCH=%s MEM=%d",
		 iso, arch, mem);
	system(cmd);
}

void fun()
{
	run_qemu(qemu_config.iso,
		 qemu_config.arch,
		 qemu_config.mem);
}
