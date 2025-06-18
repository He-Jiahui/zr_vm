//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"

#include <string.h>

#include "xxHash/xxhash.h"
#include "zr_vm_core/memory.h"

TUInt64 ZrStringHash(SZrGlobalState *global, TNativeString string, const TZrSize length) {
    return XXH64(string, length, global->constantStringTable.seed);
}

TZrString *ZrStringCreate(SZrGlobalState *global, TNativeString string, TZrSize length) {
    TZrString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(TZrString);

    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (TZrString *) ZrMalloc(global, totalSize);
        ZrMemoryCopy(constantString->stringDataExtend, string, length);
        ((TNativeString) constantString->stringDataExtend)[length] = '\0';
        constantString->shortStringLength = (TUInt8) length;
        constantString->nextShortString = ZR_NULL;
    } else {
        totalSize += sizeof(TNativeString);
        constantString = (TZrString *) ZrMalloc(global, totalSize);
        TNativeString *pointer = (TNativeString *) &(constantString->stringDataExtend);
        *pointer = (TNativeString) ZrMalloc(global, length + 1);
        memcpy(*pointer, string, length);
        *pointer[length] = '\0';
        constantString->shortStringLength = ZR_VM_SHORT_STRING_MAX + 1;
        constantString->longStringLength = length;
    }
    ZrObjectInit(&constantString->super, ZR_VALUE_TYPE_OBJECT);
    constantString->hash = ZrStringHash(global, string, length);
    return constantString;
}

#undef MIX_MAGIC_NUMBER
