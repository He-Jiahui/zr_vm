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


void ZrMemoryCopy(TZrPtr destination, TZrPtr source, const TZrSize size) {
    memcpy(destination, source, size);
}

void ZrMemoryFree(TZrPtr pointer) {
    free(pointer);
}
