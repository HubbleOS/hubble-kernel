#include "builder.h"
#include "buildconfig.h"
#include "exec.h"
#include "fs.h"
#include "log.h"
#include "tblgen.h"
#include <stdio.h>
#include <string.h>

int build_file(const char *src, const char *obj, SourceLang lang,
               const BuildConfig *cfg, const char *cxx_compiler) {
  char cmd[8192];
  char obj_dir[4096];

  strncpy(obj_dir, obj, sizeof(obj_dir) - 1);
  char *last_slash = strrchr(obj_dir, '/');
  if (last_slash)
    *last_slash = 0;
  mkdir_p(obj_dir);

  if (!cfg->force_rebuild && !needs_rebuild(src, obj)) {
    if (cfg->verbose)
      LOG_INFO("Skipping %s (up to date)", src);
    return 0;
  }

  int ret = 0;
  switch (lang) {
  case LANG_C:
    LOG_INFO("Compiling: %s -> %s", src, obj);
    snprintf(cmd, sizeof(cmd),
             "%s -MMD -MP -fdiagnostics-color=always %s %s -c %s -o %s",
             cfg->compiler, cfg->cflags, cfg->includes, src, obj);
    ret = run_command(cmd, cfg->verbose);
    break;
  case LANG_CPP:
    LOG_INFO("Compiling C++: %s -> %s", src, obj);
    snprintf(cmd, sizeof(cmd),
             "%s -MMD -MP -fdiagnostics-color=always %s %s -c %s -o %s",
             cxx_compiler, cfg->cflags, cfg->includes, src, obj);
    ret = run_command(cmd, cfg->verbose);
    break;
  case LANG_ASM:
    LOG_INFO("Assembling: %s -> %s", src, obj);
    snprintf(cmd, sizeof(cmd), "%s %s %s -o %s", cfg->assembler, cfg->asmflags,
             src, obj);
    ret = run_command(cmd, cfg->verbose);
    break;
  case LANG_TBL: {
    LOG_INFO("Generating header from TBL: %s", src);

    char header_path[MAX_PATH];
    strncpy(header_path, src, sizeof(header_path) - 1);
    header_path[sizeof(header_path) - 1] = '\0';

    char *dot = strrchr(header_path, '.');
    if (dot)
      strcpy(dot, ".h");
    else
      strncat(header_path, ".h", sizeof(header_path) - strlen(header_path) - 1);

    ret = generate_tbl2header(src, header_path, TBL_SYSCALL);
    break;
  }
  default:
    fprintf(stderr, "Error: Unknown source language for %s\n", src);
    LOG_ERROR("Unknown source language for %s", src);
    return 1;
  }

  if (ret == 0) {
    LOG_SUCCESS("Successfully built: %s", src);
  } else {
    LOG_ERROR("Failed to build: %s", src);
  }

  return ret;
}

int create_archive(const char *output, char obj_files[][MAX_PATH],
                   int obj_count, const BuildConfig *cfg) {
  char cmd[MAX_CMD];
  int cmd_len;
  int use_response_file = 0;
  char response_file[MAX_PATH];

  LOG_INFO("Creating archive: %s (%d object files)", output, obj_count);

  char output_dir[MAX_PATH];
  strncpy(output_dir, output, sizeof(output_dir) - 1);
  char *last_slash = strrchr(output_dir, '/');
  if (last_slash)
    *last_slash = 0;
  mkdir_p(output_dir);

  if (obj_count > 50) {
    use_response_file = 1;
    snprintf(response_file, sizeof(response_file), "%s.rsp", output);

    FILE *rsp = fopen(response_file, "w");
    if (!rsp) {
      fprintf(stderr, "Error: cannot create response file %s\n", response_file);
      LOG_ERROR("Cannot create response file: %s", response_file);
      return 1;
    }

    for (int i = 0; i < obj_count; i++)
      fprintf(rsp, "%s\n", obj_files[i]);

    fclose(rsp);
    LOG_INFO("Created response file: %s", response_file);
  }

  if (use_response_file) {
    snprintf(cmd, sizeof(cmd), "%s rcs %s @%s", cfg->archiver, output,
             response_file);
  } else {
    cmd_len = snprintf(cmd, sizeof(cmd), "%s rcs %s", cfg->archiver, output);

    for (int i = 0; i < obj_count; i++) {
      int written =
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
      if (written < 0 || cmd_len + written >= (int)sizeof(cmd) - 4096) {
        fprintf(stderr, "Error: archive command too long\n");
        LOG_ERROR("Archive command too long");
        return 1;
      }
      cmd_len += written;
    }
  }

  int ret = run_command(cmd, cfg->verbose);

  if (use_response_file)
    remove(response_file);

  if (ret == 0) {
    LOG_SUCCESS("Successfully created archive: %s", output);
  } else {
    LOG_ERROR("Failed to create archive: %s", output);
  }

  return ret;
}

int link_executable(const char *output, char obj_files[][MAX_PATH],
                    int obj_count, const BuildConfig *cfg,
                    const char *cxx_compiler) {
  char cmd[MAX_CMD];
  int cmd_len;
  int use_response_file = 0;
  char response_file[MAX_PATH];

  LOG_INFO("Linking executable: %s (%d object files)", output, obj_count);

  char output_dir[MAX_PATH];
  strncpy(output_dir, output, sizeof(output_dir) - 1);
  char *last_slash = strrchr(output_dir, '/');
  if (last_slash)
    *last_slash = 0;
  mkdir_p(output_dir);

  const char *linker = (strlen(cfg->linker) > 0) ? cfg->linker : cxx_compiler;

  if (obj_count > 50) {
    use_response_file = 1;
    snprintf(response_file, sizeof(response_file), "%s.rsp", output);

    FILE *rsp = fopen(response_file, "w");
    if (!rsp) {
      fprintf(stderr, "Error: cannot create response file %s\n", response_file);
      LOG_ERROR("Cannot create response file: %s", response_file);
      return 1;
    }

    for (int i = 0; i < obj_count; i++)
      fprintf(rsp, "%s\n", obj_files[i]);

    // extra obj_files идут после основных объектов
    if (strlen(cfg->obj_files) > 0)
      fprintf(rsp, "%s\n", cfg->obj_files);

    if (strlen(cfg->ldflags) > 0)
      fprintf(rsp, "%s\n", cfg->ldflags);
    if (strlen(cfg->libs) > 0)
      fprintf(rsp, "%s\n", cfg->libs);

    fclose(rsp);
    LOG_INFO("Created response file: %s", response_file);
  }

  if (use_response_file) {
    if (strlen(cfg->ldscript) > 0) {
      snprintf(cmd, sizeof(cmd), "%s -T%s -o %s @%s",
               (strlen(cfg->linker) > 0) ? cfg->linker : "ld", cfg->ldscript,
               output, response_file);
    } else {
      snprintf(cmd, sizeof(cmd), "%s -o %s @%s", linker, output, response_file);
    }
  } else {
    if (strlen(cfg->ldscript) > 0) {
      cmd_len = snprintf(cmd, sizeof(cmd), "%s -T%s -o %s",
                         (strlen(cfg->linker) > 0) ? cfg->linker : "ld",
                         cfg->ldscript, output);
    } else {
      cmd_len = snprintf(cmd, sizeof(cmd), "%s -o %s", linker, output);
    }

    // Основные объектные файлы
    for (int i = 0; i < obj_count; i++) {
      int written =
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
      if (written < 0 || cmd_len + written >= (int)sizeof(cmd) - 4096) {
        fprintf(stderr, "Error: link command too long\n");
        LOG_ERROR("Link command too long");
        return 1;
      }
      cmd_len += written;
    }

    // Extra obj_files (после основных, перед ldflags)
    if (strlen(cfg->obj_files) > 0) {
      int written =
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->obj_files);
      if (written < 0 || cmd_len + written >= (int)sizeof(cmd) - 4096) {
        fprintf(stderr, "Error: link command too long (obj_files)\n");
        LOG_ERROR("Link command too long (obj_files)");
        return 1;
      }
      cmd_len += written;
    }

    // ldflags
    if (strlen(cfg->ldflags) > 0)
      cmd_len +=
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->ldflags);

    // Библиотеки В КОНЦЕ
    if (strlen(cfg->libs) > 0)
      cmd_len +=
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->libs);
  }

  int ret = run_command(cmd, cfg->verbose);

  if (use_response_file)
    remove(response_file);

  if (ret == 0) {
    LOG_SUCCESS("Successfully linked executable: %s", output);
  } else {
    LOG_ERROR("Failed to link executable: %s", output);
  }

  return ret;
}

int link_module(const char *output, char obj_files[][MAX_PATH], int obj_count,
                const BuildConfig *cfg) {
  char cmd[MAX_CMD];
  int cmd_len;
  int use_response_file = 0;
  char response_file[MAX_PATH];

  LOG_INFO("Linking module: %s (%d object files)", output, obj_count);

  char output_dir[MAX_PATH];
  strncpy(output_dir, output, sizeof(output_dir) - 1);
  output_dir[sizeof(output_dir) - 1] = '\0';
  char *last_slash = strrchr(output_dir, '/');
  if (last_slash)
    *last_slash = 0;
  mkdir_p(output_dir);

  const char *linker = (strlen(cfg->linker) > 0) ? cfg->linker : "ld";

  if (obj_count > 50) {
    use_response_file = 1;
    snprintf(response_file, sizeof(response_file), "%s.rsp", output);

    FILE *rsp = fopen(response_file, "w");
    if (!rsp) {
      fprintf(stderr, "Error: cannot create response file %s\n", response_file);
      LOG_ERROR("Cannot create response file: %s", response_file);
      return 1;
    }

    for (int i = 0; i < obj_count; i++)
      fprintf(rsp, "%s\n", obj_files[i]);

    if (strlen(cfg->obj_files) > 0)
      fprintf(rsp, "%s\n", cfg->obj_files);

    if (strlen(cfg->ldflags) > 0)
      fprintf(rsp, "%s\n", cfg->ldflags);

    fclose(rsp);
    LOG_INFO("Created response file: %s", response_file);
  }

  if (use_response_file) {
    snprintf(cmd, sizeof(cmd), "%s -r -o %s @%s", linker, output,
             response_file);
  } else {
    cmd_len = snprintf(cmd, sizeof(cmd), "%s -r -o %s", linker, output);

    for (int i = 0; i < obj_count; i++) {
      int written =
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", obj_files[i]);
      if (written < 0 || cmd_len + written >= (int)sizeof(cmd) - 4096) {
        fprintf(stderr, "Error: module link command too long\n");
        LOG_ERROR("Module link command too long");
        return 1;
      }
      cmd_len += written;
    }

    if (strlen(cfg->obj_files) > 0) {
      int written =
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->obj_files);
      if (written < 0 || cmd_len + written >= (int)sizeof(cmd) - 4096) {
        fprintf(stderr, "Error: module link command too long (obj_files)\n");
        LOG_ERROR("Module link command too long (obj_files)");
        return 1;
      }
      cmd_len += written;
    }

    if (strlen(cfg->ldflags) > 0)
      cmd_len +=
          snprintf(cmd + cmd_len, sizeof(cmd) - cmd_len, " %s", cfg->ldflags);
  }

  int ret = run_command(cmd, cfg->verbose);

  if (use_response_file)
    remove(response_file);

  if (ret == 0)
    LOG_SUCCESS("Successfully linked module: %s", output);
  else
    LOG_ERROR("Failed to link module: %s", output);

  return ret;
}
