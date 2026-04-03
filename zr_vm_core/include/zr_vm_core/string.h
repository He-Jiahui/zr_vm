//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_STRING_H
#define ZR_VM_CORE_STRING_H

#include <stdarg.h>
#include <string.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/hash_set.h"
struct SZrGlobalState;
struct SZrState;
struct SZrTypeValue;
#define ZR_STRING_LITERAL(STATE, STR) (ZrCore_String_Create((STATE), "" STR, (sizeof(STR) / sizeof(char) - 1)))

struct ZR_STRUCT_ALIGN SZrString {
    SZrRawObject super;

    // 尾部
    union {
        TZrSize longStringLength;
        struct SZrString *nextShortString;
    };

    TZrUInt8 shortStringLength;
    // short string is raw data
    // long string is a pointer
    TZrUInt8 stringDataExtend[1];
};

typedef struct SZrString SZrString;


struct ZR_STRUCT_ALIGN SZrStringTable {
    SZrHashSet stringHashSet;
    TZrBool isValid;
};

typedef struct SZrStringTable SZrStringTable;

#define ZR_STRING_FORMAT_BUFFER_SIZE (ZR_LOG_DEBUG_FUNCTION_STR_SIZE_MAX + ZR_NUMBER_TO_STRING_LENGTH_MAX + 95)

struct ZR_STRUCT_ALIGN SZrNativeStringFormatBuffer {
    struct SZrState *state;
    TZrBool isOnStack;
    TZrSize length;
    char result[ZR_STRING_FORMAT_BUFFER_SIZE];
};

typedef struct SZrNativeStringFormatBuffer SZrNativeStringFormatBuffer;

ZR_FORCE_INLINE TZrInt32 ZrCore_NativeString_Compare(TZrNativeString string1, TZrNativeString string2) {
    return strcmp(string1, string2);
}


ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Length(TZrNativeString string) { return strlen(string); }

ZR_FORCE_INLINE TZrChar *ZrCore_NativeString_CharFind(TZrNativeString string, TZrChar ch) { return strchr(string, ch); }

ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Span(TZrNativeString string, TZrChar *charset) {
    return strspn(string, charset);
}

ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Concat(TZrNativeString string1, TZrNativeString string2,
                                                   ZR_OUT TZrNativeString result) {
    TZrSize length1 = ZrCore_NativeString_Length(string1);
    TZrSize length2 = ZrCore_NativeString_Length(string2);
    TZrSize length = length1 + length2;
    strcpy(result, string1);
    strcpy(result + length1, string2);
    result[length] = '\0';
    return length;
}


ZR_FORCE_INLINE TZrSize ZrCore_NativeString_Utf8CharLength(TZrChar *buffer, TZrUInt64 uChar) {
    ZR_ASSERT(uChar <= 0x7fffffffu);
    TZrSize length = 1;
    if (uChar <= 0x7fu) {
        // 0xxxxxxx
        buffer[ZR_STRING_UTF8_SIZE - 1] = ZR_CAST_CHAR(uChar);
    } else {
        TZrUInt64 maxFirstByte = 0x3fu;
        // convert uChar to:
        // 110xxxxx 10xxxxxx  (11)
        // 1110xxxx 10xxxxxx 10xxxxxx  (16)
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (21)
        do {
            buffer[ZR_STRING_UTF8_SIZE - (length++)] = ZR_CAST_CHAR(0x80u | (uChar & 0x3fu));
            uChar >>= 6;
            maxFirstByte >>= 1;
        } while (uChar > maxFirstByte);
        buffer[ZR_STRING_UTF8_SIZE - length] = ZR_CAST_CHAR((~maxFirstByte << 1) | uChar);
    }
    return length;
}


ZR_CORE_API TZrNativeString ZrCore_NativeString_VFormat(struct SZrState *state, TZrNativeString format, va_list args);

ZR_CORE_API TZrNativeString ZrCore_NativeString_Format(struct SZrState *state, TZrNativeString format, ...);

ZR_CORE_API void ZrCore_StringTable_New(struct SZrGlobalState *global);

ZR_CORE_API void ZrCore_StringTable_Free(struct SZrGlobalState *global, SZrStringTable *stringTable);

ZR_CORE_API void ZrCore_StringTable_Init(struct SZrState *state);


ZR_CORE_API SZrString *ZrCore_String_Create(struct SZrState *state, TZrNativeString string, TZrSize length);

ZR_FORCE_INLINE SZrString *ZrCore_String_CreateFromNative(struct SZrState *state, TZrNativeString string) {
    return ZrCore_String_Create(state, string, ZrCore_NativeString_Length(string));
}

ZR_CORE_API SZrString *ZrCore_String_CreateTryHitCache(struct SZrState *state, TZrNativeString string);


ZR_FORCE_INLINE TZrNativeString ZrCore_String_GetNativeStringShort(const SZrString *string) {
    ZR_ASSERT(string->shortStringLength < ZR_VM_LONG_STRING_FLAG);
    return (TZrNativeString) string->stringDataExtend;
}

ZR_FORCE_INLINE TZrNativeString *ZrCore_String_GetNativeStringLong(const SZrString *string) {
    ZR_ASSERT(string->shortStringLength == ZR_VM_LONG_STRING_FLAG);
    return (TZrNativeString *) string->stringDataExtend;
}

ZR_FORCE_INLINE TZrNativeString ZrCore_String_GetNativeString(const SZrString *string) {
    if (string->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(string);
    } else {
        return *ZrCore_String_GetNativeStringLong(string);
    }
}

ZR_CORE_API TZrBool ZrCore_String_Equal(SZrString *string1, SZrString *string2);

// for hash table comparison
ZR_FORCE_INLINE TZrBool ZrCore_String_Compare(struct SZrState *state, const SZrString *string1,
                                              const SZrString *string2) {
    ZR_UNUSED_PARAMETER(state);
    return ZrCore_String_Equal((SZrString *) string1, (SZrString *) string2);
}

// this is not thread safe and not reentrant
ZR_CORE_API void ZrCore_String_Concat(struct SZrState *state, TZrSize count);

ZR_CORE_API void ZrCore_String_ConcatSafe(struct SZrState *state, TZrSize count);


// todo: number to string
ZR_CORE_API SZrString *ZrCore_String_FromNumber(struct SZrState *state, struct SZrTypeValue *value);
// todo: object to string

// todo: utf-8 to string

#endif // ZR_VM_CORE_STRING_H
