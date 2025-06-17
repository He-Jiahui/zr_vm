//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"

#include <string.h>

#include "xxHash/xxhash.h"
#include "zr_vm_core/memory.h"

TUInt64 ZrStringHash(SZrGlobalState *global, TRawString string, const TZrSize length) {
    return XXH64(string, length, global->constantStringTable.seed);
}

TZrConstantString *ZrStringCreate(SZrGlobalState *global, TRawString string, TZrSize length) {
    TZrConstantString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(TZrConstantString);

    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (TZrConstantString *) ZrMalloc(global, totalSize);
        ZrMemoryCopy(constantString->stringData, string, length);
        ((TRawString) constantString->stringData)[length] = '\0';
        constantString->shortStringLength = (TUInt8) length;
        constantString->nextShortString = ZR_NULL;
    } else {
        totalSize += sizeof(TRawString);
        constantString = (TZrConstantString *) ZrMalloc(global, totalSize);
        TRawString *pointer = (TRawString *) &(constantString->stringData);
        *pointer = (TRawString) ZrMalloc(global, length + 1);
        memcpy(*pointer, string, length);
        *pointer[length] = '\0';
        constantString->shortStringLength = ZR_VM_SHORT_STRING_MAX + 1;
        constantString->longStringLength = length;
    }
    constantString->hash = ZrStringHash(global, string, length);
    return constantString;
}

#undef MIX_MAGIC_NUMBER
