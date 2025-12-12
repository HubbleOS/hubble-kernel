#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ui/main.h>
#include <apps/app.h>
#include <apps/menus.h>

static void config_save(const char *path);
static void config_init(const char *path);
static void config_load(const char *path);
static void config_set(const char *label, const char *value);
static const char *config_get(const char *label);

static void config_apply_kv(const char *key, const char *value)
{
	for (size_t i = 0; i < checklists.count; i++)
	{
		if (strcmp(checklists.items[i].label, key) == 0)
		{
			checklists.items[i].checked = atoi(value);
			break;
		}
	}
}

static void config_set(const char *label, const char *value)
{
	config_apply_kv(label, value);
}

static const char *config_get(const char *label)
{
	static char buffer[128];

	for (size_t i = 0; i < checklists.count; i++)
	{
		if (strcmp(checklists.items[i].label, label) == 0)
		{
			snprintf(buffer, sizeof(buffer), "%d", checklists.items[i].checked);
			return buffer;
		}
	}

	return NULL;
}

static void config_write(const char *path, bool defaults)
{
	FILE *file = fopen(path, "w");
	if (!file)
		return;

	// Checklist items
	for (size_t i = 0; i < checklists.count; i++)
	{
		int value = defaults ? 0 : checklists.items[i].checked;
		fprintf(file, "%s=%d\n", checklists.items[i].label, value);
	}

	fclose(file);
}

static void config_save(const char *path)
{
	config_write(path, false);
}

static void config_init(const char *path)
{
	config_write(path, true);
}

static void config_load(const char *path)
{
	FILE *file = fopen(path, "r");
	if (!file)
	{
		config.init(path);
		file = fopen(path, "r");
		if (!file)
			return;
	}

	char line[256];
	while (fgets(line, sizeof(line), file))
	{
		line[strcspn(line, "\n")] = '\0';
		char *key = strtok(line, "=");
		char *value = strtok(NULL, "=");
		if (key && value)
			config_apply_kv(key, value);
	}
	fclose(file);
}

Config config = {
    .save = config_save,
    .init = config_init,
    .load = config_load,
    .set = config_set,
    .get = config_get,
};
