//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_TYPE_CONF_H
#define ZR_TYPE_CONF_H

#include <limits.h>
#include <stdint.h>

#define ZR_IS_32_INT ((UINT_MAX >> 30) >= 3)

typedef size_t TZrSize;

typedef void*  TZrPtr;    // 指针兼容类型

// c internal types
typedef uint8_t   TUInt8;    // 1 byte
typedef int8_t    TInt8;     // 1 byte
typedef uint16_t  TUInt16;   // 2 bytes
typedef int16_t   TInt16;    // 2 bytes
typedef uint32_t  TUInt32;   // 4 bytes
typedef int32_t   TInt32;    // 4 bytes
typedef uint64_t  TUInt64;   // 8 bytes
typedef int64_t   TInt64;    // 8 bytes

typedef float TFloat;
typedef double TDouble;

typedef unsigned char TBool;

typedef char * TRawString;

#define ZR_NULL NULL

//

typedef union {
    TUInt64 top;
    TZrPtr pointer;
} TStackIndicator;
// gc object
#define ZR_GC_HEADER struct SGcObject *next;

struct SGcObject {
    ZR_GC_HEADER
};

typedef struct SGcObject SGcObject;

// gc object type
#define ZR_VM_SHORT_STRING_MAX  127U    // 短字符串最大长度
struct TConstantString {
    ZR_GC_HEADER
    TUInt8 shortStringLength;
    TUInt64 hash;
    // 尾部
    union {
        TZrSize longStringLength;
        struct TConstantString* nextShortString;
    };

    TUInt8 stringData[1];
};

typedef struct TConstantString TConstantString;

struct SConstantStringTable {
    TConstantString **buckets;
    TZrSize bucketCount;
    TZrSize elementCount;
    TZrSize resizeThreshold;
    TUInt64 seed;
};

typedef struct SConstantStringTable SConstantStringTable;


#endif //ZR_TYPE_CONF_H
