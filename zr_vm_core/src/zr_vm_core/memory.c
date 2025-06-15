//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/memory.h"

#include <stdlib.h>
#include <string.h>
// todo:
void *ZrMalloc(const TZrSize size) {
    return malloc(size);
}


void ZrMemoryCopy(const TZrPtr destination, const TZrPtr source, const TZrSize size) {
    memcpy(destination, source, size);
}

void ZrMemoryFree(const TZrPtr pointer) {
    free(pointer);
}
