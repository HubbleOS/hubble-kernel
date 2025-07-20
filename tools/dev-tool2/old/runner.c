#include <stdio.h>
#include "runner.h"
#include "utils.h"

#define CMD(fmt) run_cmd_in_window(fmt)

void act_qemu() { CMD("make -C qemu"); }
void act_build() { CMD("make -C ../.. build"); }
void act_run() { CMD("make -C ../.. run"); }
void act_clean() { CMD("make -C ../.. clean"); }
void act_help() { CMD("make -C ../.. help"); }
void act_flash() { CMD("make -C ../.. flash"); }
void act_docker_run() { CMD("make -C ../.. docker-run"); }
void act_docker_build() { CMD("make -C ../.. docker-build"); }
void act_docker_clean() { CMD("make -C ../.. docker-clean"); }
