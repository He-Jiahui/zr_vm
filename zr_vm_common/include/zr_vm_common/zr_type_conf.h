//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_TYPE_CONF_H
#define ZR_TYPE_CONF_H
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdalign.h>

#define ZR_IS_32_INT ((UINT_MAX >> 30) >= 3)

typedef size_t TZrSize;
typedef ptrdiff_t TZrMemoryOffset;

typedef void *TZrPtr; // 指针兼容类型


// c internal types
typedef uint8_t TUInt8; // 1 byte
typedef int8_t TInt8; // 1 byte
typedef uint16_t TUInt16; // 2 bytes
typedef int16_t TInt16; // 2 bytes
typedef uint32_t TUInt32; // 4 bytes
typedef int32_t TInt32; // 4 bytes
typedef uint64_t TUInt64; // 8 bytes
typedef int64_t TInt64; // 8 bytes

typedef float TFloat;
typedef double TDouble;

typedef unsigned char TBool;

typedef char *TRawString;

#define ZR_NULL NULL

#if defined(__GNUC__)
#define ZR_COMPILER_GNU
#elif defined(__clang__)
#define ZR_COMPILER_CLANG
#elif defined(_MSC_VER)
#define ZR_COMPILER_MSVC
#endif

#if defined(ZR_COMPILER_GNU)
#define ZR_STRUCT_ALIGN __attribute__((aligned(alignof(max_align_t))))
#define ZR_FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(ZR_COMPILER_MSVC)
#define ZR_STRUCT_ALIGN __declspec(align(8))
#define ZR_FORCE_INLINE __forceinline
#elif defined(ZR_COMPILER_CLANG)
#define ZR_STRUCT_ALIGN __attribute__((aligned(alignof(max_align_t))))
#define ZR_FORCE_INLINE inline
#else
#define ZR_STRUCT_ALIGN
#define ZR_FORCE_INLINE inline
#endif

//

typedef union {
    TUInt64 top;
    TZrPtr pointer;
} TStackIndicator;

// gc object
#define ZR_GC_HEADER struct SGcObject *next;

struct ZR_STRUCT_ALIGN SGcObject {
    ZR_GC_HEADER
};

typedef struct SGcObject SGcObject;

// gc object type
#define ZR_VM_SHORT_STRING_MAX  127U    // 短字符串最大长度

struct ZR_STRUCT_ALIGN TZrConstantString {
    ZR_GC_HEADER
    TUInt64 hash;

    // 尾部
    union {
        TZrSize longStringLength;
        struct TZrConstantString *nextShortString;
    };

    TUInt8 shortStringLength;
    TUInt8 stringData[1];
};

typedef struct TZrConstantString TZrConstantString;

struct ZR_STRUCT_ALIGN SZrConstantStringTable {
    TZrConstantString **buckets;
    TZrSize bucketCount;
    TZrSize elementCount;
    TZrSize resizeThreshold;
    TUInt64 seed;
};

typedef struct SZrConstantStringTable SZrConstantStringTable;

// alloc function
typedef TZrPtr (*FZrAlloc)(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize);
#endif //ZR_TYPE_CONF_H
