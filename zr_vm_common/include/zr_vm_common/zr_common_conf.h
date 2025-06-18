//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_COMMON_CONF_H
#define ZR_COMMON_CONF_H
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdalign.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>

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

typedef char *TNativeString;

union TZrNativeObject {
    TBool nativeBool;
    TUInt8 nativeUInt8;
    TInt8 nativeInt8;
    TUInt16 nativeUInt16;
    TInt16 nativeInt16;
    TUInt32 nativeUInt32;
    TInt32 nativeInt32;
    TUInt64 nativeUInt64;
    TInt64 nativeInt64;
    TFloat nativeFloat;
    TDouble nativeDouble;
    TZrPtr nativePointer;
    TNativeString nativeString;
};

typedef union TZrNativeObject TZrNativeObject;


#define ZR_NULL NULL
#define ZR_TRUE (1)
#define ZR_FALSE (0)

#define ZR_ASSERT(CONDITION) assert(CONDITION)

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


// allocator function
typedef TZrPtr (*FZrAllocator)(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize);

// TryCatch types
typedef jmp_buf TZrExceptionLongJump;

// Debug Signal
typedef sig_atomic_t TZrDebugSignal;

#endif //ZR_COMMON_CONF_H
