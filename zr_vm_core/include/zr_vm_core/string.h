//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include <stdarg.h>
#include <string.h>

#include "convertion.h"
#include "zr_vm_core/conf.h"
struct SZrGlobalState;
struct SZrState;
#define ZR_STRING_LITERAL(STATE, STR) (ZrStringCreate((STATE), "" STR, (sizeof(STR)/sizeof(char) - 1)))


#define ZR_STRING_FORMAT_BUFFER_SIZE (ZR_LOG_DEBUG_FUNCTION_STR_SIZE_MAX + ZR_NUMER_TO_STRING_LENGTH_MAX + 95)

struct ZR_STRUCT_ALIGN SZrNativeStringFormatBuffer {
    struct SZrState *state;
    TBool isOnStack;
    TZrSize length;
    char result[ZR_STRING_FORMAT_BUFFER_SIZE];
};

typedef struct SZrNativeStringFormatBuffer SZrNativeStringFormatBuffer;

ZR_FORCE_INLINE TInt32 ZrNativeStringCompare(TNativeString string1, TNativeString string2) {
    return strcmp(string1, string2);
}

ZR_FORCE_INLINE TZrSize ZrNativeStringLength(TNativeString string) {
    return strlen(string);
}

ZR_FORCE_INLINE TChar *ZrNativeStringCharFind(TNativeString string, TChar ch) {
    return strchr(string, ch);
}

ZR_FORCE_INLINE TZrSize ZrNativeStringSpan(TNativeString string, TChar *charset) {
    return strspn(string, charset);
}

ZR_FORCE_INLINE TZrSize ZrNativeStringUtf8CharLength(TChar *buffer, TUInt64 uChar) {
    ZR_ASSERT(uChar <= 0x7fffffffu);
    TZrSize length = 1;
    if (uChar <= 0x7fu) {
        // 0xxxxxxx
        buffer[ZR_STRING_UTF8_SIZE - 1] = ZR_CAST_CHAR(uChar);
    } else {
        TUInt64 maxFirstByte = 0x3fu;
        // convert uChar to:
        // 110xxxxx 10xxxxxx  (11)
        // 1110xxxx 10xxxxxx 10xxxxxx  (16)
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (21)
        do {
            buffer[ZR_STRING_UTF8_SIZE - (length++)] = ZR_CAST_CHAR(0x80u | (uChar & 0x3fu));
            uChar >>= 6;
            maxFirstByte >>= 1;
        } while (uChar > maxFirstByte);
        buffer[ZR_STRING_UTF8_SIZE - length] = ZR_CAST_CHAR((~maxFirstByte<<1) | uChar);
    }
    return length;
}


ZR_CORE_API TNativeString ZrNativeStringVFormat(struct SZrState *state, TNativeString format, va_list args);

ZR_CORE_API TNativeString ZrNativeStringFormat(struct SZrState *state, TNativeString format, ...);

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

ZR_CORE_API void ZrStringConcat(struct SZrState *state, TZrSize count);

// todo: raw object to string
ZR_CORE_API void ZrStringConvertFromRawObject(struct SZrState *state, SZrRawObject *object);

// todo: number to string

// todo: object to string

// todo: utf-8 to string

#endif //ZR_VM_CORE_STRING_H
