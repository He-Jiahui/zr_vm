//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include <string.h>

#include "zr_vm_core/conf.h"
struct SZrGlobalState;
struct SZrState;
#define ZR_STRING_LITERAL(STATE, STR) (ZrStringCreate((STATE), "" STR, (sizeof(STR)/sizeof(char) - 1)))

ZR_FORCE_INLINE TInt32 ZrNativeStringCompare(TNativeString string1, TNativeString string2) {
    return strcmp(string1, string2);
}

ZR_FORCE_INLINE TZrSize ZrNativeStringLength(TNativeString string) {
    return strlen(string);
}

ZR_CORE_API void ZrStringTableNew(struct SZrGlobalState *global);

ZR_CORE_API void ZrStringTableInit(struct SZrState *state);


ZR_CORE_API TZrString *ZrStringCreate(struct SZrState *state, TNativeString string, TZrSize length);

ZR_FORCE_INLINE TZrString *ZrStringCreateFromNative(struct SZrState *state, TNativeString string) {
    return ZrStringCreate(state, string, ZrNativeStringLength(string));
}

ZR_CORE_API TZrString *ZrStringCreateTryHitCache(struct SZrState *state, TNativeString string);

ZR_FORCE_INLINE TNativeString *ZrStringGetNativeStringShort(TZrString *string) {
    ZR_ASSERT(string->shortStringLength < ZR_VM_LONG_STRING_FLAG);
    return (TNativeString *) string->stringDataExtend;
}

ZR_FORCE_INLINE TNativeString *ZrStringGetNativeStringLong(TZrString *string) {
    ZR_ASSERT(string->shortStringLength == ZR_VM_LONG_STRING_FLAG);
    return (TNativeString *) string->stringDataExtend;
}
#endif //ZR_VM_CORE_STRING_H
