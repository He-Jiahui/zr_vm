//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"
#include <string.h>

#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"


void ZrStringTableInit(SZrGlobalState *global) {
    SZrStringTable *stringTable = &global->stringTable;
    stringTable->bucketCount = 0;
    stringTable->elementCount = 0;
    stringTable->capacity = 0;
    stringTable->buckets = ZR_NULL;
}

TZrString *ZrStringCreate(SZrGlobalState *global, TNativeString string, TZrSize length) {
    TZrString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(TZrString);

    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (TZrString *) ZrMemoryMalloc(global, totalSize);
        ZrMemoryCopy(constantString->stringDataExtend, string, length);
        ((TNativeString) constantString->stringDataExtend)[length] = '\0';
        constantString->shortStringLength = (TUInt8) length;
        constantString->nextShortString = ZR_NULL;
    } else {
        totalSize += sizeof(TNativeString);
        constantString = (TZrString *) ZrMemoryMalloc(global, totalSize);
        TNativeString *pointer = (TNativeString *) &(constantString->stringDataExtend);
        *pointer = (TNativeString) ZrMemoryMalloc(global, length + 1);
        memcpy(*pointer, string, length);
        *pointer[length] = '\0';
        constantString->shortStringLength = ZR_VM_SHORT_STRING_MAX + 1;
        constantString->longStringLength = length;
    }
    ZrObjectInit(&constantString->super, ZR_VALUE_TYPE_OBJECT);
    constantString->hash = ZrHashCreate(global, string, length);
    return constantString;
}
