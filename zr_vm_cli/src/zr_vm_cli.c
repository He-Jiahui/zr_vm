//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_cli.h"
#include <stdio.h>

ZR_API_IMPORT void Hello();

void zr_vm_cli_main(const int argc, char **argv) {
    printf("use argc %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv %d is '%s'\n", argc, argv[i]);
    }
    Hello();

}

int main(const int argc, char **argv) {
    zr_vm_cli_main(argc, argv);
    return 0;
}
