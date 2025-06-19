//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_STRING_CONF_H
#define ZR_STRING_CONF_H
#include "zr_vm_common/zr_object_conf.h"
// gc object type
#define ZR_VM_SHORT_STRING_MAX  127U    // 短字符串最大长度

struct ZR_STRUCT_ALIGN TZrString {
    SZrRawObject super;
    TUInt64 hash;

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
    TZrString **buckets;
    TZrSize bucketCount;
    TZrSize elementCount;
    TZrSize capacity;
    TZrSize resizeThreshold;
};

typedef struct SZrStringTable SZrStringTable;

#endif //ZR_STRING_CONF_H
