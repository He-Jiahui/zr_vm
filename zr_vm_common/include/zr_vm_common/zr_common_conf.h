//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_COMMON_CONF_H
#define ZR_COMMON_CONF_H

#if defined(_MSC_VER) && defined(noreturn)
#undef noreturn
#endif

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

/* TODO: 当前宏无生产调用；若将来接受 31 或更高位，带符号 int 左移可能溢出。
 * 引入调用前须明确掩码宽度并加边界测试。 */
#define ZR_BIT_MASK(BIT) (1 << (BIT))

/* 大小、偏移和线格式整数分开定义：内存长度随宿主位宽，持久化字段采用固定宽度。 */
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

/* 原生字符串惯用 NUL 结束；含内嵌 NUL 的 VM 字符串跨此边界会丢失后续内容。 */
typedef char *TZrNativeString;

/** @brief native 调用与 VM 值转换共享的标量槽；读出的成员必须对应写入时的值类型。 */
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

/* 调试构建保留断言，发布构建不求值 CONDITION；不得把有副作用的操作放入检查表达式。 */
#if defined(ZR_DEBUG)
#define ZR_ASSERT(CONDITION) assert((CONDITION))
#else
#define ZR_ASSERT(CONDITION) ((void) 0)
#endif

#define ZR_CHECK(STATE, CONDITION, MESSAGE) ((void) STATE, ZR_ASSERT((CONDITION) && (MESSAGE)))

#define ZR_CHECK_EXP(STATE, CONDITION, VALUE) ((void) STATE, ZR_ASSERT((CONDITION)), (VALUE))

#if defined(__clang__)
#define ZR_COMPILER_CLANG
#elif defined(__GNUC__)
#define ZR_COMPILER_GNU
#elif defined(_MSC_VER)
#define ZR_COMPILER_MSVC
#endif

/*
 * GCC 4.8 (still used by the supported Windows MinGW toolchain) does not
 * recognize the C11 _Thread_local keyword even when -std=c11 is selected.
 * Keep the storage-class spelling in one common contract so headers and test
 * harnesses do not silently diverge by compiler version or language mode.
 */
#if defined(_MSC_VER)
#define ZR_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) && !defined(__clang__) && \
      ((__GNUC__ < 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ < 9)))
#define ZR_THREAD_LOCAL __thread
#elif defined(__cplusplus)
#if __cplusplus >= 201103L
#define ZR_THREAD_LOCAL thread_local
#else
#define ZR_THREAD_LOCAL __thread
#endif
#else
#define ZR_THREAD_LOCAL _Thread_local
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
#define ZR_FORCE_INLINE __attribute__((always_inline)) inline
#if defined(_MSC_VER)
#define ZR_NO_RETURN _Noreturn
#else
#define ZR_NO_RETURN __attribute__((noreturn))
#endif
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

/** @brief 宿主分配器回调；pointer/originalSize 表示旧块，newSize 为请求大小，flag 传递内存类别。
 *  返回新块由调用方依约持有；newSize 为 0 时按调用方释放协议处理。 */
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
