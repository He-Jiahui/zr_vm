//
// Created by HeJiahui on 2025/6/5.
//
#include "zr_vm_cli.h"
#include "zr_vm_core.h"
#include <stdio.h>
#include <stdlib.h>

#include "zr_vm_core/global.h"

static TZrPtr TestAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }
    return (TZrPtr) realloc(pointer, newSize);
}

void ZrCliMain(const int argc, char **argv) {
    printf("use argc %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv %d is '%s'\n", argc, argv[i]);
    }
    Hello();
    // printf("%p",(TZrPtr)ZrStringObjectCreate("hello world", 11));
    SZrGlobalState *global = ZrGlobalStateNew(TestAllocator, ZR_NULL, 0x1234567890ABCDEF);

    ZrGlobalStateFree(global);
    global = ZR_NULL;
}

int main(const int argc, char **argv) {
    ZrCliMain(argc, argv);
    return 0;
}
