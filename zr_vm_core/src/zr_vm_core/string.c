//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"

#include <string.h>

#include "xxHash/xxhash.h"
#include "zr_vm_core/memory.h"

TUInt64 ZrHashString(const TRawString string, const TZrSize length, const TUInt64 seed) {
    return XXH64(string, length, seed);
}

TConstantString* ZrCreateString(TRawString string, TZrSize length) {
    TConstantString* constantString = ZR_NULL;
    TZrSize totalSize = sizeof(TConstantString);

    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (TConstantString*)ZrMalloc(totalSize);
        ZrMemoryCopy(constantString->stringData, string, length);
        ((TRawString)constantString->stringData)[length] = '\0';
        constantString->shortStringLength = length;
        constantString->nextShortString = ZR_NULL;
    }else {
        totalSize += sizeof(TRawString);
        constantString = (TConstantString*)ZrMalloc(totalSize);
        TRawString* pointer = (TRawString*)&(constantString->stringData);
        *pointer = (TRawString)ZrMalloc(length + 1);
        memcpy(*pointer, string, length);
        *pointer[length] = '\0';
        constantString->shortStringLength = ZR_VM_SHORT_STRING_MAX + 1;
        constantString->longStringLength = length;
    }
    constantString->hash = ZrHashString(string, length, ZR_VM_CONSTANT_STRING_HASH_SEED);
    return constantString;
}

#undef MIX_MAGIC_NUMBER


