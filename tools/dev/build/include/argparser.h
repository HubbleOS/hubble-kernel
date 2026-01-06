#pragma once

#include "buildconfig.h"

int parse_arguments(int argc, char **argv, BuildConfig *cfg, char *cxx_compiler, char *output_type);
void print_usage(const char *prog);
