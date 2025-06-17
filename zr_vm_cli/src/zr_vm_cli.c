//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_cli.h"
#include "zr_vm_core.h"
#include <stdio.h>

#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

void zr_vm_cli_main(const int argc, char **argv) {
    printf("use argc %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv %d is '%s'\n", argc, argv[i]);
    }
    Hello();
    // printf("%p",(TZrPtr)ZrStringCreate("hello world", 11));
}

int main(const int argc, char **argv) {
    zr_vm_cli_main(argc, argv);
    return 0;
}
