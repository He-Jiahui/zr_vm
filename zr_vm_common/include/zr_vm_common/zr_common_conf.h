//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_COMMON_CONF_H
#define ZR_COMMON_CONF_H

// MSVC 兼容性处理：确保标准库头文件能够正确找到
#if defined(_MSC_VER)
    // 在 MSVC 下，先包含基础头文件以确保标准库路径正确
    #include <stdlib.h>
    #include <stddef.h>
#endif

#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define ZR_IS_OVER_32_INT ((UINT_MAX >> 30) >= 3)

#define ZR_BIT_MASK(BIT) (1 << (BIT))

typedef size_t TZrSize;
#define ZR_MAX_SIZE (SIZE_MAX)

typedef ptrdiff_t TZrMemoryOffset;

#define ZR_MAX_MEMORY_OFFSET ((TZrMemoryOffset) (ZR_MAX_SIZE >> 1))

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

#define ZR_INT_MAX LLONG_MAX
#define ZR_INT_MIN LLONG_MIN
#define ZR_UINT_MAX ULLONG_MAX

typedef float TFloat32;
typedef float TFloat;
typedef double TFloat64;
typedef double TDouble;

typedef unsigned char TBool;

typedef TUInt32 TEnum;

typedef char *TNativeString;

union TZrNativeObject {
    // to fulfill the union
    TUInt64 nativeBool;
    // all char saved as 64 bits
    TInt64 nativeChar;
    // all integer saved as 64 bits
    // TUInt8 nativeUInt8;
    // TInt8 nativeInt8;
    // TUInt16 nativeUInt16;
    // TInt16 nativeInt16;
    // TUInt32 nativeUInt32;
    // TInt32 nativeInt32;
    TUInt64 nativeUInt64;
    TInt64 nativeInt64;
    // all float saved as 64 bits
    // TFloat nativeFloat;
    TDouble nativeDouble;
    TZrPtr nativePointer;
};

typedef union TZrNativeObject TZrNativeObject;


#define ZR_NULL NULL
#define ZR_TRUE (1)
#define ZR_FALSE (0)

#if defined(ZR_DEBUG)
#define ZR_ASSERT(CONDITION) assert((CONDITION))
#else
#define ZR_ASSERT(CONDITION) ((void) 0)
#endif

#define ZR_CHECK(STATE, CONDITION, MESSAGE) ((void) STATE, ZR_ASSERT((CONDITION) && (MESSAGE)))

#define ZR_CHECK_EXP(STATE, CONDITION, VALUE) ((void) STATE, ZR_ASSERT((CONDITION)), (VALUE))

#if defined(__GNUC__)
#define ZR_COMPILER_GNU
#elif defined(__clang__)
#define ZR_COMPILER_CLANG
#elif defined(_MSC_VER)
#define ZR_COMPILER_MSVC
#endif

#if defined(ZR_COMPILER_GNU)
#define ZR_STRUCT_ALIGN __attribute__((aligned(alignof(max_align_t))))
#define ZR_ALIGN_SIZE (sizeof(max_align_t))
#define ZR_FORCE_INLINE __attribute__((always_inline)) inline
#define ZR_NO_RETURN __attribute__((noreturn))
#define ZR_FAST_CALL __attribute__((fastcall))
#elif defined(ZR_COMPILER_MSVC)
#define ZR_STRUCT_ALIGN __declspec(align(8))
#define ZR_ALIGN_SIZE (8)
#define ZR_FORCE_INLINE __forceinline
#define ZR_NO_RETURN __declspec(noreturn)
#define ZR_FAST_CALL __declspec(naked) __fastcall
#elif defined(ZR_COMPILER_CLANG)
#define ZR_STRUCT_ALIGN __attribute__((aligned(alignof(max_align_t))))
#define ZR_ALIGN_SIZE (sizeof(max_align_t))
#define ZR_FORCE_INLINE inline
#define ZR_NO_RETURN __attribute__((noreturn))
#define ZR_FAST_CALL __attribute__((fastcall))
#else
#define ZR_STRUCT_ALIGN
#define ZR_ALIGN_SIZE (8)
#define ZR_FORCE_INLINE inline
#define ZR_NO_RETURN
#define ZR_FAST_CALL
#endif

#define ZR_IN
#define ZR_OUT
#define ZR_INOUT

// allocator function
typedef TZrPtr (*FZrAllocator)(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag);

// TryCatch types
typedef jmp_buf TZrExceptionLongJump;

// Debug Signal
typedef sig_atomic_t TZrDebugSignal;
#define ZR_DEBUG_SIGNAL_NONE 0
#define ZR_DEBUG_SIGNAL_TRAP 1
#define ZR_DEBUG_SIGNAL_BREAKPOINT 2


// likely and unlikely to optimize branch prediction
#if defined(ZR_COMPILER_GNU)
#define ZR_LIKELY(CONDITION) (__builtin_expect(((CONDITION) != ZR_FALSE), 1))
#define ZR_UNLIKELY(CONDITION) (__builtin_expect(((CONDITION) != ZR_FALSE), 0))
#else
#define ZR_LIKELY(CONDITION) (CONDITION)
#define ZR_UNLIKELY(CONDITION) (CONDITION)
#endif

#define ZR_ABORT() abort()


// MACRO
#define ZR_MACRO_REGISTER_WRAP(WRAP_START, WRAP_END, ...)                                                              \
    WRAP_START                                                                                                         \
    __VA_ARGS__                                                                                                        \
    WRAP_END

#endif // ZR_COMMON_CONF_H
