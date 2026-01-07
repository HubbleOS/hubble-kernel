// #include "menu_json.h"
// #include <jansson.h>
// #include <stdlib.h>
// #include <string.h>

// static Menu *parse_menu_array(json_t *array);

// void action_run_command(void)
// {
// 	return;
// }

// static MenuItem parse_menu_item(json_t *obj)
// {
// 	MenuItem item = {0};

// 	const char *title = json_string_value(json_object_get(obj, "title"));
// 	const char *hint = json_string_value(json_object_get(obj, "hint"));
// 	const char *cmd = json_string_value(json_object_get(obj, "cmd"));

// 	if (title)
// 		item.label = strdup(title);
// 	if (hint)
// 		item.hint = strdup(hint);

// 	if (cmd)
// 	{
// 		// action будет универсальной функцией запуска команды
// 		item.action = action_run_command;
// 		item.cmd = strdup(cmd);
// 	}

// 	json_t *submenu = json_object_get(obj, "submenu");
// 	if (submenu && json_is_array(submenu))
// 	{
// 		// item.submenu = parse_menu_array(submenu);
// 	}

// 	return item;
// }

// static Menu *parse_menu_array(json_t *array)
// {
// 	size_t count = json_array_size(array);

// 	Menu *menu = calloc(1, sizeof(Menu));
// 	menu->items = calloc(count, sizeof(MenuItem));
// 	menu->count = count;

// 	for (size_t i = 0; i < count; i++)
// 	{
// 		json_t *obj = json_array_get(array, i);
// 		menu->items[i] = parse_menu_item(obj);
// 	}

// 	return menu;
// }

// Menu *load_menu_from_json(const char *path)
// {
// 	json_error_t err;
// 	json_t *root = json_load_file(path, 0, &err);
// 	if (!root)
// 	{
// 		fprintf(stderr, "menu.json error: %s\n", err.text);
// 		return NULL;
// 	}

// 	Menu *menu = parse_menu_array(root);
// 	json_decref(root);
// 	return menu;
// }
