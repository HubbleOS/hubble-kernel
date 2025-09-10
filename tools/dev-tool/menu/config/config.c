#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ui/main.h>
#include "../apps/app.h"

void config_save(const char *path);
void config_init(const char *path);
void config_load(const char *path);
void config_set(const char *label, bool value);
void config_get(const char *label, bool *value);

void config_set(const char *label, bool value)
{
	for (size_t i = 0; i < checklists.count; i++)
	{
		if (strcmp(checklists.items[i].label, label) == 0)
		{
			checklists.items[i].checked = value;
			return;
		}
	}
}

void config_get(const char *label, bool *value)
{
	for (size_t i = 0; i < checklists.count; i++)
	{
		if (strcmp(checklists.items[i].label, label) == 0)
		{
			*value = checklists.items[i].checked;
			return;
		}
	}
}

void config_save(const char *path)
{
	FILE *file = fopen(path, "w");
	if (!file)
		return;

	for (size_t i = 0; i < checklists.count; i++)
	{
		fprintf(file, "%s=%d\n", checklists.items[i].label, checklists.items[i].checked);
	}

	fclose(file);
}

void config_init(const char *path)
{
	FILE *file = fopen(path, "w");
	if (!file)
		return;

	for (size_t i = 0; i < checklists.count; i++)
	{
		// fprintf(file, "%s=%d\n", checklists.items[i].label, checklists.items[i].checked);
		fprintf(file, "%s=%d\n", checklists.items[i].label, 0);
	}

	fclose(file);
}

void config_load(const char *path)
{
	FILE *file = fopen(path, "r");
	if (!file)
	{
		config.init(path);
		file = fopen(path, "r");
		if (!file)
			return;
	}

	char line[128];
	while (fgets(line, sizeof(line), file))
	{
		line[strcspn(line, "\n")] = '\0'; // delete \n

		char *key = strtok(line, "=");
		char *value = strtok(NULL, "=");
		if (!key || !value)
			continue;

		for (size_t i = 0; i < checklists.count; i++)
		{
			if (strcmp(key, checklists.items[i].label) == 0)
			{
				checklists.items[i].checked = atoi(value);
				break;
			}
		}
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
