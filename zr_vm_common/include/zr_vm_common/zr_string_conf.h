//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_STRING_CONF_H
#define ZR_STRING_CONF_H

#include <stdio.h>
#include <inttypes.h>
#include <locale.h>

#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_hash_conf.h"
// gc object type
#define ZR_VM_SHORT_STRING_MAX  127U    // 短字符串最大长度 不得超过UINT8_MAX
#define ZR_VM_LONG_STRING_FLAG (0XFF) // 长字符串最大长度 不得超过INT32_MAX

#define ZR_NUMER_TO_STRING_LENGTH_MAX 44

#define ZR_STRING_NULL_STRING "<null>"

#define ZR_STRING_DECIMAL_NUMBER_SET "-0123456789"

#define ZR_STRING_LOCALE_DECIMAL_POINT (localeconv()->decimal_point[0])

#define ZR_STRING_UTF8_SIZE 8

struct ZR_STRUCT_ALIGN TZrString {
    SZrHashRawObject super;

    // 尾部
    union {
        TZrSize longStringLength;
        struct TZrString *nextShortString;
    };

    TUInt8 shortStringLength;
    // short string is raw data
    // long string is a pointer
    TUInt8 stringDataExtend[1];
};

typedef struct TZrString TZrString;

struct ZR_STRUCT_ALIGN SZrStringTable {
    SZrHashSet stringHashSet;
};

typedef struct SZrStringTable SZrStringTable;

#define ZR_STRING_SIGNED_INTEGER_PRINT_FORMAT(STR, LEN, NUMBER)\
snprintf(STR, LEN, "%" PRId64, (TInt64)NUMBER)

#define ZR_STRING_UNSIGNED_INTEGER_PRINT_FORMAT(STR, LEN, NUMBER)\
snprintf(STR, LEN, "%" PRIu64, (TUInt64)NUMBER)

#define ZR_STRING_FLOAT_PRINT_FORMAT(STR, LEN, NUMBER)\
snprintf(STR, LEN, "%.14g", (TFloat64)NUMBER)

#define ZR_STRING_POINTER_PRINT_FORMAT(STR, LEN, POINTER)\
snprintf(STR, LEN, "%p", (void *)POINTER)

#define ZR_STRING_CHAR_PRINT_FORMAT(STR, LEN, CHAR)\
snprintf(STR, LEN, "%c", (TChar)CHAR)

#endif //ZR_STRING_CONF_H
