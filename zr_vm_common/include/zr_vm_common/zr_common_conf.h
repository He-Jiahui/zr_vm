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
#include <stdlib.h>

#define ZR_IS_32_INT ((UINT_MAX >> 30) >= 3)

#define ZR_BIT_MASK(BIT) (1 << (BIT))

typedef size_t TZrSize;
#define ZR_MAX_SIZE (SIZE_MAX)

typedef ptrdiff_t TZrMemoryOffset;

typedef void *TZrPtr; // 指针兼容类型
typedef uint8_t *TBytePtr;

typedef intptr_t TZrNativePtr;

// c internal types
typedef char TChar;
typedef unsigned char TByte;
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

#if defined(ZR_DEBUG)
#define ZR_ASSERT(CONDITION) assert((CONDITION))
#else
#define ZR_ASSERT(CONDITION) ((void)0)
#endif

#define ZR_CHECK(STATE, CONDITION, MESSAGE) \
    ((void)STATE, ZR_ASSERT((CONDITION) && (MESSAGE)))

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
#define ZR_NO_RETURN __attribute__((noreturn))
#elif defined(ZR_COMPILER_MSVC)
#define ZR_STRUCT_ALIGN __declspec(align(8))
#define ZR_FORCE_INLINE __forceinline
#define ZR_NO_RETURN __declspec(noreturn)
#elif defined(ZR_COMPILER_CLANG)
#define ZR_STRUCT_ALIGN __attribute__((aligned(alignof(max_align_t))))
#define ZR_FORCE_INLINE inline
#define ZR_NO_RETURN __attribute__((noreturn))
#else
#define ZR_STRUCT_ALIGN
#define ZR_FORCE_INLINE inline
#define ZR_NO_RETURN
#endif


// allocator function
typedef TZrPtr (*FZrAllocator)(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag);

// TryCatch types
typedef jmp_buf TZrExceptionLongJump;

// Debug Signal
typedef sig_atomic_t TZrDebugSignal;


// likely and unlikely to optimize branch prediction
#if defined(ZR_COMPILER_GNU)
#define ZR_LIKELY(CONDITION) (__builtin_expect(((CONDITION) != ZR_FALSE), 1))
#define ZR_UNLIKELY(CONDITION) (__builtin_expect(((CONDITION) != ZR_FALSE), 0))
#else
#define ZR_LIKELY(CONDITION) (CONDITION)
#define ZR_UNLIKELY(CONDITION) (CONDITION)
#endif

#define ZR_ABORT() abort()


#endif //ZR_COMMON_CONF_H
