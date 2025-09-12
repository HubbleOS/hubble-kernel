#include <config/config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ui/main.h>
#include <apps/app.h>
#include <apps/qemu.h>
#include <apps/menus.h>

static void config_save(const char *path);
static void config_init(const char *path);
static void config_load(const char *path);
static void config_set(const char *label, const char *value);
static const char *config_get(const char *label);

static void config_apply_kv(const char *key, const char *value)
{
	if (strcmp(key, "ISO") == 0)
		strncpy(qemu_config.iso, value, sizeof(qemu_config.iso) - 1);
	else if (strcmp(key, "ARCH") == 0)
		strncpy(qemu_config.arch, value, sizeof(qemu_config.arch) - 1);
	else if (strcmp(key, "MEM") == 0)
		qemu_config.mem = atoi(value);
	else if (strcmp(key, "SMP") == 0)
		qemu_config.smp = atoi(value);
	else if (strcmp(key, "DEBUG_PORT") == 0)
		qemu_config.debug_port = atoi(value);
	else
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
}

static void config_set(const char *label, const char *value)
{
	config_apply_kv(label, value);
}

static const char *config_get(const char *label)
{
	static char buffer[128];

	if (strcmp(label, "ISO") == 0)
		return qemu_config.iso;
	else if (strcmp(label, "ARCH") == 0)
		return qemu_config.arch;
	else if (strcmp(label, "MEM") == 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", qemu_config.mem);
		return buffer;
	}
	else if (strcmp(label, "SMP") == 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", qemu_config.smp);
		return buffer;
	}
	else if (strcmp(label, "DEBUG_PORT") == 0)
	{
		snprintf(buffer, sizeof(buffer), "%d", qemu_config.debug_port);
		return buffer;
	}
	else
	{
		for (size_t i = 0; i < checklists.count; i++)
		{
			if (strcmp(checklists.items[i].label, label) == 0)
			{
				snprintf(buffer, sizeof(buffer), "%d", checklists.items[i].checked);
				return buffer;
			}
		}
	}
	return NULL;
}

static void config_write(const char *path, bool defaults)
{
	FILE *file = fopen(path, "w");
	if (!file)
		return;

	if (defaults)
	{
		QemuConfig default_config = {
		    .iso = "../../../out/x86/iso/",
		    .arch = "x86_64",
		    .mem = 1024,
		    .smp = 2,
		    .debug_port = 1000,
		};
	}

	// QEMU config
	fprintf(file, "ISO=%s\n", qemu_config.iso);
	fprintf(file, "ARCH=%s\n", qemu_config.arch);
	fprintf(file, "MEM=%d\n", qemu_config.mem);
	fprintf(file, "SMP=%d\n", qemu_config.smp);
	fprintf(file, "DEBUG_PORT=%d\n", qemu_config.debug_port);

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
