#include <stdio.h>

void __attribute__((weak)) sandbox_init() {};
void __attribute__((weak)) sandbox_start() {
    printf("[WARNING] Running without a sandbox\n");
};
