//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_STRING_CONF_H
#define ZR_STRING_CONF_H
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_hash_conf.h"
// gc object type
#define ZR_VM_SHORT_STRING_MAX  127U    // 短字符串最大长度 不得超过UINT8_MAX
#define ZR_VM_LONG_STRING_FLAG (0XFF) // 长字符串最大长度 不得超过INT32_MAX

#define ZR_NUMER_TO_STRING_LENGTH_MAX 44

#define ZR_STRING_NULL_STRING "<null>"

struct ZR_STRUCT_ALIGN TZrString {
    SZrHashRawObject super;

    // 尾部
    union {
        TZrSize longStringLength;
        struct TZrString *nextShortString;
    };

    TUInt8 shortStringLength;

    TUInt8 stringDataExtend[1];
};

typedef struct TZrString TZrString;

struct ZR_STRUCT_ALIGN SZrStringTable {
    SZrHashSet stringHashSet;
};

typedef struct SZrStringTable SZrStringTable;

#endif //ZR_STRING_CONF_H
