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
typedef uint8_t *TZrBytePtr;

typedef intptr_t TZrNativePtr;

// c internal types
typedef char TZrChar;
typedef unsigned char TZrByte;
typedef uint8_t TZrUInt8; // 1 byte
typedef int8_t TZrInt8; // 1 byte
typedef uint16_t TZrUInt16; // 2 bytes
typedef int16_t TZrInt16; // 2 bytes
typedef uint32_t TZrUInt32; // 4 bytes
typedef int32_t TZrInt32; // 4 bytes
typedef uint64_t TZrUInt64; // 8 bytes
typedef int64_t TZrInt64; // 8 bytes

#define ZR_INT_MAX LLONG_MAX
#define ZR_INT_MIN LLONG_MIN
#define ZR_UINT_MAX ULLONG_MAX

typedef float TZrFloat32;
typedef float TZrFloat;
typedef double TZrFloat64;
typedef double TZrDouble;

typedef unsigned char TZrBool;

typedef TZrUInt32 TZrEnum;

typedef char *TZrNativeString;

union TZrNativeObject {
    // to fulfill the union
    TZrUInt64 nativeBool;
    // all char saved as 64 bits
    TZrInt64 nativeChar;
    // all integer saved as 64 bits
    // TZrUInt8 nativeUInt8;
    // TZrInt8 nativeInt8;
    // TZrUInt16 nativeUInt16;
    // TZrInt16 nativeInt16;
    // TZrUInt32 nativeUInt32;
    // TZrInt32 nativeInt32;
    TZrUInt64 nativeUInt64;
    TZrInt64 nativeInt64;
    // all float saved as 64 bits
    // TZrFloat nativeFloat;
    TZrDouble nativeDouble;
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
typedef TZrPtr (*FZrAllocator)(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag);

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
