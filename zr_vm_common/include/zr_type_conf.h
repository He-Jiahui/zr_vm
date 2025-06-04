//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_TYPE_CONF_H
#define ZR_TYPE_CONF_H

#include <limits.h>

#define ZR_IS_32_INT ((UINT_MAX >> 30) >= 3)

typedef unsigned long TZrSize;

typedef void* TZrPtr;

typedef unsigned char TUInt8;
typedef signed char TInt8;
typedef unsigned short TUInt16;
typedef signed short TInt16;
typedef unsigned long TUInt32;
typedef signed long TInt32;
typedef unsigned long long TUInt64;
typedef signed long long TInt64;

//

typedef union {
    TUInt64 top;
    TZrPtr pointer;
} TStackIndicator;

#define ZR_GC_HEADER struct ZGcObject *next;

typedef struct ZGcObject {
    ZR_GC_HEADER;
} ZGcObject;

#endif //ZR_TYPE_CONF_H
