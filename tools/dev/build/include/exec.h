/**
 * @file exec.h
 * @brief
 *
 *
 */

#pragma once

int run_command(const char *cmd, int verbose);
int run_commands_parallel(const char *cmds[], const char *src_files[], int count, int max_jobs, int verbose);
