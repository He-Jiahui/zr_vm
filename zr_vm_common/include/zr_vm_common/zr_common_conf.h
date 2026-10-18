//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_COMMON_CONF_H
#define ZR_COMMON_CONF_H

#if defined(_MSC_VER) && defined(noreturn)
#undef noreturn
#endif

// MSVC 兼容性处理：在公共标准头集合前先包含基础头文件。
#if defined(_MSC_VER)
    // 统一基础定义的包含顺序；头文件搜索路径仍由构建环境配置。
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
 * pointer 为 NULL 时 originalSize 必须为 0；newSize 为 0 时释放旧块。
 * 非零请求失败返回 NULL，并保留旧块及其内容、归属和可释放性；成功重分配保留
 * min(originalSize, newSize) 个字节。返回的新块由调用方持有，使用同一回调和类别释放。 */
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

/**
 * @def ZR_IS_OVER_32_INT
 * @brief ZR_IS_OVER_32_INT：为整数宽度相关的预处理分支提供 unsigned int 能力谓词。
 *
 * ## Summary
 * 为整数宽度相关的预处理分支提供 unsigned int 能力谓词。
 *
 * ## Value model
 * (UINT_MAX >> 30) >= 3 表示 unsigned int 至少有 32 个值位，而非严格大于 32 位。
 *
 * ## Invariants and lifecycle
 * 不得据结果推断指针、size_t 或 TZrInt64 的宽度；只用于能接受该整数常量表达式的条件上下文。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:28：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_vm_conf.h:10：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_BIT_MASK
 * @brief ZR_BIT_MASK：在调用方给出的位索引上构造 int 掩码。
 *
 * ## Summary
 * 在调用方给出的位索引上构造 int 掩码。
 *
 * ## Parameters
 * @param[in] BIT 位索引表达式；仅在移位及结果均符合当前 int 模型时可用；表达式在展开式中出现一次。
 *
 * ## Returns
 * @return 返回 int 移位结果；不进行边界检查或失败恢复。
 *
 * ## Constraints and lifecycle
 * 展开为 (1 << BIT)，结果及移位的合法性由 C int 规则决定。
 * TODO: 尚无仓内活动生产调用证明其高位契约；负值、达到左操作数位宽或不可表示的有符号结果不能视为有效输入。引入调用前在此宏及消费者明确无符号宽度与界限。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:32：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:30：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrSize
 * @brief TZrSize：在内存容器、分配器和宿主接口间传递对象数量或字节长度。
 *
 * ## Summary
 * 在内存容器、分配器和宿主接口间传递对象数量或字节长度。
 *
 * ## Value model
 * size_t 的别名，宽度由当前 C 宿主决定；Rust sys 层对应 usize。
 *
 * ## Invariants and lifecycle
 * 数量与字节数由各接口区分；乘加、转换和持久化前必须在消费端校验。该类型不保证 64 位，也不携带分配或借用所有权。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:35：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:10：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/memory.h:19：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2363：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:3177：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:3178：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:3193：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:3203：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_MAX_SIZE
 * @brief ZR_MAX_SIZE：为内存长度计算提供 size_t 的最大值。
 *
 * ## Summary
 * 为内存长度计算提供 size_t 的最大值。
 *
 * ## Value model
 * 直接使用 SIZE_MAX，与 TZrSize 配套。
 *
 * ## Invariants and lifecycle
 * 作为边界值参与检查，不代表可分配的实际容量；乘加须在达到边界前拒绝溢出。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:36：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/memory.h:19：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/artifact/artifact_text.c:198：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/artifact/artifact_text.c:200：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/artifact/artifact_text.c:246：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:249：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrMemoryOffset
 * @brief TZrMemoryOffset：表示同一内存布局中的有符号地址差或指令相对偏移。
 *
 * ## Summary
 * 表示同一内存布局中的有符号地址差或指令相对偏移。
 *
 * ## Value model
 * ptrdiff_t 别名；与 TZrSize 的无符号长度模型不同。
 *
 * ## Invariants and lifecycle
 * 只有满足 C 指针差前提的地址差才可表示；转为长度前须检查负值和范围。类型本身不保证任意两个地址可相减。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:38：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:353：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:3057：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:4982：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:7864：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:7891：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_MAX_MEMORY_OFFSET
 * @brief ZR_MAX_MEMORY_OFFSET：为既有长度转有符号偏移的计算提供项目内界限表达式。
 *
 * ## Summary
 * 为既有长度转有符号偏移的计算提供项目内界限表达式。
 *
 * ## Value model
 * 将 (SIZE_MAX >> 1) 转为 TZrMemoryOffset。
 *
 * ## Invariants and lifecycle
 * 该表达式不是直接使用 PTRDIFF_MAX；不得据此推断所有 C 实现中 size_t 与 ptrdiff_t 具有相同宽度或任意地址差可表示。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:40：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/memory.c:50：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/memory.c:75：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrPtr
 * @brief TZrPtr：跨内部接口借用或交接无类型对象地址。
 *
 * ## Summary
 * 跨内部接口借用或交接无类型对象地址。
 *
 * ## Value model
 * void *；NULL 与指向有效对象的地址由消费接口区分。
 *
 * ## Invariants and lifecycle
 * 不改变对象寿命或对齐，不自动保活 GC 对象；释放方法、跨线程访问和可写性来自所属接口。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:42：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:360：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/memory.h:19：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:860：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:875：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:645：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:731：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrBytePtr
 * @brief TZrBytePtr：在 IO、缓冲区和原生内存接口中按字节访问存储。
 *
 * ## Summary
 * 在 IO、缓冲区和原生内存接口中按字节访问存储。
 *
 * ## Value model
 * uint8_t *，地址必须与相应长度共同解释。
 *
 * ## Invariants and lifecycle
 * 指针不带容量或结尾信息；借用范围由调用者保证，不能据类型推断 NUL 终止或可释放。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:43：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2314：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:860：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/scripts/test_artifact_golden.c:45：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/scripts/test_artifact_golden.c:47：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/scripts/test_artifact_golden.c:51：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrNativePtr
 * @brief TZrNativePtr：在需要整数载体的原生接口中保存可往返的对象指针表示。
 *
 * ## Summary
 * 在需要整数载体的原生接口中保存可往返的对象指针表示。
 *
 * ## Value model
 * intptr_t 别名；有符号指针宽度整数。
 *
 * ## Invariants and lifecycle
 * 整数值不是 GC 根或可解引用能力；只有来源和寿命仍有效的地址才能恢复并使用。整数算术不自动保留对象指针语义。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:45：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:422：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:429：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_code_handle.c:484：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c:285：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrChar
 * @brief TZrChar：供原生字符序列和 C 字符串接口使用。
 *
 * ## Summary
 * 供原生字符序列和 C 字符串接口使用。
 *
 * ## Value model
 * char 别名；char 的有符号性与字符编码由宿主约定。
 *
 * ## Invariants and lifecycle
 * 不是 Unicode 标量或码点；以字节数、NUL 或显式编码解释文本，不能仅据此类型假设 UTF-8 校验已完成。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:48：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:10：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:293：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c:288：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_binary_stage.c:27：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_a.c:27：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrByte
 * @brief TZrByte：供 VM 原生字节存储和二进制读写使用。
 *
 * ## Summary
 * 供 VM 原生字节存储和二进制读写使用。
 *
 * ## Value model
 * unsigned char 别名。
 *
 * ## Invariants and lifecycle
 * 作为原始字节使用，不携带文本编码或对象所有权。线格式宽度仍须按读写端的实际布局校验。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:49：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2314：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:302：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c:297：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_binary_stage.c:31：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_a.c:31：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrUInt8
 * @brief TZrUInt8：为内部标量、协议字段或跨语言接口提供 8 位无符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 8 位无符号整数载体。
 *
 * ## Value model
 * uint8_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:50：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2299：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2306：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2842：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2843：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrInt8
 * @brief TZrInt8：为内部标量、协议字段或跨语言接口提供 8 位有符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 8 位有符号整数载体。
 *
 * ## Value model
 * int8_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:51：定义、配套消费或失败边界证据。
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:365：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:207：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:208：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:281：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrUInt16
 * @brief TZrUInt16：为内部标量、协议字段或跨语言接口提供 16 位无符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 16 位无符号整数载体。
 *
 * ## Value model
 * uint16_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:52：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:12：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2323：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2342：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2345：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2347：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrInt16
 * @brief TZrInt16：为内部标量、协议字段或跨语言接口提供 16 位有符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 16 位有符号整数载体。
 *
 * ## Value model
 * int16_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:53：定义、配套消费或失败边界证据。
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:366：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5918：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5923：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:9009：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrUInt32
 * @brief TZrUInt32：为内部标量、协议字段或跨语言接口提供 32 位无符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 32 位无符号整数载体。
 *
 * ## Value model
 * uint32_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:54：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:13：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2296：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2303：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:3135：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:3167：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrInt32
 * @brief TZrInt32：为内部标量、协议字段或跨语言接口提供 32 位有符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 32 位有符号整数载体。
 *
 * ## Value model
 * int32_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:55：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:397：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:254：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:610：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:621：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrUInt64
 * @brief TZrUInt64：为内部标量、协议字段或跨语言接口提供 64 位无符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 64 位无符号整数载体。
 *
 * ## Value model
 * uint64_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:56：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:14：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:229：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:597：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:2611：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:3388：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrInt64
 * @brief TZrInt64：为内部标量、协议字段或跨语言接口提供 64 位有符号整数载体。
 *
 * ## Summary
 * 为内部标量、协议字段或跨语言接口提供 64 位有符号整数载体。
 *
 * ## Value model
 * int64_t 的别名；值域和布局由相应 stdint 类型规定。
 *
 * ## Invariants and lifecycle
 * 长度、枚举标签和地址不能仅因可转换到本类型就视为合法；溢出、字节序和 ABI 传参规则由各边界检查。固定宽度不等于整个持久化对象可跨宿主加载。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:57：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:11：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2282：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2283：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2284：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2285：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_INT_MAX
 * @brief ZR_INT_MAX：在原生整数相关运算中提供项目使用的宿主极值。
 *
 * ## Summary
 * 在原生整数相关运算中提供项目使用的宿主极值。
 *
 * ## Value model
 * 直接展开 LLONG_MAX，基于 C long long 家族。
 *
 * ## Invariants and lifecycle
 * 名字不额外证明 long long 与 int64_t 在所有平台完全同构；对外或线格式边界仍检查实际类型和范围。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:59：定义、配套消费或失败边界证据。
 * - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c:305：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/math.h:74：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/math.h:78：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/math.h:83：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_INT_MIN
 * @brief ZR_INT_MIN：在原生整数相关运算中提供项目使用的宿主极值。
 *
 * ## Summary
 * 在原生整数相关运算中提供项目使用的宿主极值。
 *
 * ## Value model
 * 直接展开 LLONG_MIN，基于 C long long 家族。
 *
 * ## Invariants and lifecycle
 * 名字不额外证明 long long 与 int64_t 在所有平台完全同构；对外或线格式边界仍检查实际类型和范围。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:60：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/math.h:79：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_UINT_MAX
 * @brief ZR_UINT_MAX：在原生整数相关运算中提供项目使用的宿主极值。
 *
 * ## Summary
 * 在原生整数相关运算中提供项目使用的宿主极值。
 *
 * ## Value model
 * 直接展开 ULLONG_MAX，基于 C long long 家族。
 *
 * ## Invariants and lifecycle
 * 名字不额外证明 long long 与 int64_t 在所有平台完全同构；对外或线格式边界仍检查实际类型和范围。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:61：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/math.h:27：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/math.h:37：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrFloat32
 * @brief TZrFloat32：在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Summary
 * 在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Value model
 * float 的别名；名称不额外建立 IEEE 格式、大小或舍入承诺。
 *
 * ## Invariants and lifecycle
 * 序列化和外部调用须使用经验证的实际宿主布局；不得据别名推断任意平台上与文件位表示同构。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:63：定义、配套消费或失败边界证据。
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:369：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:374：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:438：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:442：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrFloat
 * @brief TZrFloat：在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Summary
 * 在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Value model
 * float 的别名；名称不额外建立 IEEE 格式、大小或舍入承诺。
 *
 * ## Invariants and lifecycle
 * 序列化和外部调用须使用经验证的实际宿主布局；不得据别名推断任意平台上与文件位表示同构。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:64：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/conversion.h:38：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrFloat64
 * @brief TZrFloat64：在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Summary
 * 在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Value model
 * double 的别名；名称不额外建立 IEEE 格式、大小或舍入承诺。
 *
 * ## Invariants and lifecycle
 * 序列化和外部调用须使用经验证的实际宿主布局；不得据别名推断任意平台上与文件位表示同构。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:65：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:15：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:227：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:231：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:232：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:233：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrDouble
 * @brief TZrDouble：在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Summary
 * 在 VM 值与宿主数值接口之间传递浮点标量。
 *
 * ## Value model
 * double 的别名；名称不额外建立 IEEE 格式、大小或舍入承诺。
 *
 * ## Invariants and lifecycle
 * 序列化和外部调用须使用经验证的实际宿主布局；不得据别名推断任意平台上与文件位表示同构。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:66：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:15：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:379：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:383：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:447：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c:450：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrBool
 * @brief TZrBool：为 C 接口和对象字段提供布尔载体。
 *
 * ## Summary
 * 为 C 接口和对象字段提供布尔载体。
 *
 * ## Value model
 * unsigned char，可保存整个字节值域；ZR_TRUE 和 ZR_FALSE 是约定的 1 与 0。
 *
 * ## Invariants and lifecycle
 * 类型转换或直接字段赋值不承诺 0/1 规范化。具体 setter 或序列化入口可另外检查或规范化；Rust sys 用 u8，不可替换成具有不同合法值集合的 Rust bool。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:68：定义、配套消费或失败边界证据。
 * - zr_vm_rust_binding/rust/zr_vm_rust_binding_sys/src/lib.rs:9：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:449：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2316：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2334：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2357：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c:2382：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrEnum
 * @brief TZrEnum：为显式编号的标签或掩码提供固定宽度整数载体。
 *
 * ## Summary
 * 为显式编号的标签或掩码提供固定宽度整数载体。
 *
 * ## Value model
 * TZrUInt32 别名，与 C enum 的实现选择宽度不同。
 *
 * ## Invariants and lifecycle
 * 不自动验证枚举域或位集合法性；线格式应按实际 reader 类型和 producer 宽度配对，不能把所有 C enum 当作本类型。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:70：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2122：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/value/meta.c:42：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/value/meta.c:50：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrNativeString
 * @brief TZrNativeString：连接使用 NUL 终止文本的原生接口。
 *
 * ## Summary
 * 连接使用 NUL 终止文本的原生接口。
 *
 * ## Value model
 * char *；与带显式长度、可能含内嵌 NUL 的 VM 字符串不同。
 *
 * ## Invariants and lifecycle
 * 只借用还是拥有由消费接口决定；指针必须在读取期间有效。内嵌 NUL 会使依赖 C 字符串终止规则的消费提前结束，不能把它当作任意 VM 字节串。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:73：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:10：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:434：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6894：定义、配套消费或失败边界证据。
 * - zr_vm_aot/tests/scripts/test_utils.c:60：定义、配套消费或失败边界证据。
 * - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c:820：定义、配套消费或失败边界证据。
 */

/**
 * @union TZrNativeObject
 * @brief TZrNativeObject：作为带外类型标签的标量槽连接 VM 值存储和原生值转换。
 *
 * ## Summary
 * 作为带外类型标签的标量槽连接 VM 值存储和原生值转换。
 *
 * ## Fields
 * union 中 nativeBool/nativeUInt64 为 uint64，nativeChar/nativeInt64 为 int64，nativeDouble 为 double，nativePointer 为 void*；同一存储的不同视图不能当作独立值。
 *
 * ## Invariants and lifecycle
 * 应按外层值标签读取与写入相符的成员；nativeBool 的槽宽不代表所有写入都规范化为 0/1。指针槽不自动注册根、转移所有权或延长对象寿命；整个 union 不作为通用线格式。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:76：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/value.h:22：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_ASSERT
 * @brief ZR_ASSERT：在启用的调试断言配置中检查内部不变量。
 *
 * ## Summary
 * 在启用的调试断言配置中检查内部不变量。
 *
 * ## Parameters
 * @param[in] CONDITION 应为无必需副作用的检查表达式；只有实际启用标准断言时才求值。
 *
 * ## Returns
 * 成功时继续；禁用时不求值；启用且失败时由 assert 处理。
 *
 * ## Constraints and lifecycle
 * ZR_DEBUG 定义时委托 assert；未定义时展开为 void 0。
 * NDEBUG 仍可能使标准 assert 不求值；不得把必需副作用放进 CONDITION。失败走宿主断言终止，非 VM 普通失败返回或 Throw。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:105：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:104：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/hash_set.c:32：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/hash_set.c:133：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/hash_set.c:146：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/hash_set.c:234：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_CHECK
 * @brief ZR_CHECK：在保留 state 表达式求值的同时附加可禁用的断言和消息。
 *
 * ## Summary
 * 在保留 state 表达式求值的同时附加可禁用的断言和消息。
 *
 * ## Parameters
 * @param[in] STATE 先求值一次后丢弃结果，不要求宏自身访问该对象。
 * @param[in] CONDITION 受断言开关约束的谓词，不承载必需副作用。
 * @param[in] MESSAGE 与 CONDITION 做短路 && 的诊断表达式，不是格式化输出缓冲区。
 *
 * ## Returns
 * 不产生业务返回值；普通继续或宿主断言终止。
 *
 * ## Constraints and lifecycle
 * 逗号表达式先执行 (void)STATE，再展开 ZR_ASSERT(CONDITION && MESSAGE)。
 * STATE 始终求值，CONDITION/MESSAGE 只在实际启用 assert 时参与；MESSAGE 还受 && 短路约束。不是可恢复错误 API，也不因 state 参数提供 VM 保护。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:110：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/stack.c:200：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/stack.c:204：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/stack.c:207：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/stack.c:226：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_CHECK_EXP
 * @brief ZR_CHECK_EXP：在可禁用检查之后求值并交付 VALUE。
 *
 * ## Summary
 * 在可禁用检查之后求值并交付 VALUE。
 *
 * ## Parameters
 * @param[in] STATE 总是求值一次并丢弃结果。
 * @param[in] CONDITION 仅实际启用 assert 时检查。
 * @param[in] VALUE 检查之后求值一次，类型和值由表达式本身决定。
 *
 * ## Returns
 * @return 交付 VALUE 的表达式结果；断言终止时不执行到 VALUE。
 *
 * ## Constraints and lifecycle
 * (void)STATE、ZR_ASSERT(CONDITION)、VALUE 按逗号顺序组成表达式。
 * STATE 和 VALUE 的求值不依赖调试开关；CONDITION 可能完全不求值。返回表达式不附带额外长度、所有权或错误处理。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:112：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_THREAD_LOCAL
 * @brief ZR_THREAD_LOCAL：在编译器、版本和 C/C++ 语言模式间选择线程存储拼写。
 *
 * ## Summary
 * 在编译器、版本和 C/C++ 语言模式间选择线程存储拼写。
 *
 * ## Value model
 * MSC 使用 __declspec(thread)；旧 GNU 使用 __thread；C++11+ 使用 thread_local，较旧 C++ 使用 __thread；其余 C 使用 _Thread_local。
 *
 * ## Invariants and lifecycle
 * 修饰的是声明对象的存储期，不会把指针所指资源变成线程私有，也不提供互斥、跨线程发布或回调自动收尾。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:129：定义、配套消费或失败边界证据。
 * - tests/harness/runtime_support.c:11：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/profile.h:83：定义、配套消费或失败边界证据。
 * - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c:47：定义、配套消费或失败边界证据。
 * - tests/harness/unity_crash_guard.c:11：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_STRUCT_ALIGN
 * @brief ZR_STRUCT_ALIGN：给实际对象声明施加当前编译器分支的布局对齐修饰。
 *
 * ## Summary
 * 给实际对象声明施加当前编译器分支的布局对齐修饰。
 *
 * ## Value model
 * GNU/Clang 用 aligned(alignof(max_align_t))，MSC 用 align(8)，fallback 无属性。
 *
 * ## Invariants and lifecycle
 * 必须在兼容的声明位置使用；该修饰和 ZR_ALIGN_SIZE 的数值不是同一概念。不能据此承诺不同编译器 ABI 或外部 wire layout 相同。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:144：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/call_info.h:49：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/call_info.h:63：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/call_info.h:113：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/callback.h:163：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_ALIGN_SIZE
 * @brief ZR_ALIGN_SIZE：为对象地址散列及内存约定提供当前分支的数值单位。
 *
 * ## Summary
 * 为对象地址散列及内存约定提供当前分支的数值单位。
 *
 * ## Value model
 * GNU/Clang 为 sizeof(max_align_t)；MSC/fallback 为 8。
 *
 * ## Invariants and lifecycle
 * 不是统一的 alignof(max_align_t)；消费者应以本宏实际值计算，不能把名字当作指针对齐或持久化 ABI 的证明。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:145：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/raw_object.h:91：定义、配套消费或失败边界证据。
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:361：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/gc/gc_tlab.c:395：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_struct.c:1675：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_union.c:353：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_FORCE_INLINE
 * @brief ZR_FORCE_INLINE：在性能敏感的声明中选择编译器 inline 提示。
 *
 * ## Summary
 * 在性能敏感的声明中选择编译器 inline 提示。
 *
 * ## Value model
 * GNU/Clang 为 always_inline 加 inline；MSC 为 __forceinline；fallback 为 inline。
 *
 * ## Invariants and lifecycle
 * 不改变被调函数的参数、返回或资源协议；不是跨优化级别保证机器码一定内联的 API 契约。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:146：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/hash.c:13：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/hash_set.c:31：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:32：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/closure/closure.c:44：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_NO_RETURN
 * @brief ZR_NO_RETURN：向编译器标注不会普通返回的受保护跳转或终止接口。
 *
 * ## Summary
 * 向编译器标注不会普通返回的受保护跳转或终止接口。
 *
 * ## Value model
 * GNU/非MSC Clang 用 noreturn 属性；MSC 用 declspec(noreturn)；clang-cl 分支用 _Noreturn；fallback 为空。
 *
 * ## Invariants and lifecycle
 * 必须与真实实现不返回的控制流相符，不能把普通 FALSE/NULL 失败函数随意标成此宏。属性本身不提供清理、异常恢复或线程切换。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:147：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/exception/exception_internal.h:18：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1080：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/module/module_loader.c:429：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/module/module_loader.c:502：定义、配套消费或失败边界证据。
 * - zr_vm_lib_system/src/zr_vm_lib_system/console/console.c:40：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_FAST_CALL
 * @brief ZR_FAST_CALL：为采用平台特定调用约定的声明集中选择修饰符。
 *
 * ## Summary
 * 为采用平台特定调用约定的声明集中选择修饰符。
 *
 * ## Value model
 * GNU/Clang 使用 fastcall 属性；MSC 同时展开 __declspec(naked) 与 __fastcall；fallback 为空。
 *
 * ## Invariants and lifecycle
 * 不是跨架构统一 ABI。尤其 MSC naked 影响序言/尾声生成，不能当作普通优化提示；使用点须独立证明汇编/保存寄存器/栈约束。仓内无调用不排除仓外消费者。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:148：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:154：定义、配套消费或失败边界证据。
 */

/**
 * @typedef FZrAllocator
 * @brief FZrAllocator：作为 global state 及其内存包装层的宿主分配回调，统一分配、重分配和释放。
 *
 * ## Summary
 * 作为 global state 及其内存包装层的宿主分配回调，统一分配、重分配和释放。
 *
 * ## Parameters
 * @param[in,out] userData 分配器上下文的借用地址；是否可为 NULL 由具体回调决定，必须活到该 global 的最后释放。
 * @param[in,out] pointer NULL 或该分配器仍拥有的旧块；非零请求失败时所有权和内容仍归原持有者。
 * @param[in] originalSize 旧块实际登记的大小；pointer==NULL 时为 0。
 * @param[in] newSize 非零为目标字节数，0 为释放旧块；乘加边界由调用者校验。
 * @param[in] flag 内存类别，必须与该块的重分配/释放约定匹配。
 *
 * ## Returns
 * @return 非零请求成功交付新块，失败返回 NULL 且旧块不失效；newSize==0 执行释放，不能把返回 NULL 当作释放失败。
 *
 * ## Constraints and lifecycle
 * 回调由 core 保存并派发；userData/pointer/size/flag 的含义必须在同一分配器家族内保持一致。
 * pointer 为 NULL 时 originalSize 为 0；非零请求失败必须保留原块及其内容和可释放性。成功重分配保留 min(originalSize,newSize) 字节。free 使用同一 callback/userData/类别；core 的 accounting 包装不自动序列化宿主回调，若多线程调用则宿主负责相应同步。GlobalState_New 的普通失败分支显式退还已取得资源；Throw/abort 的更高层清理由调用边界决定。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:182：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:363：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:405：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:778：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_memory.c:65：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/memory.h:19：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:360：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/runtime/global.c:734：定义、配套消费或失败边界证据。
 * - zr_vm_lib_task/src/zr_vm_lib_task/runtime/runtime_workers.c:38：定义、配套消费或失败边界证据。
 * - tests/compileTime/test_comptime_definition_environment.c:83：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrExceptionLongJump
 * @brief TZrExceptionLongJump：承载 C 异常恢复边界保存的本地跳转环境。
 *
 * ## Summary
 * 承载 C 异常恢复边界保存的本地跳转环境。
 *
 * ## Value model
 * jmp_buf 别名；恢复点与当前线程中尚未返回的 setjmp 动态范围绑定。
 *
 * ## Invariants and lifecycle
 * 不得跨原生线程或跳回已结束的栈帧；非局部跳转绕过普通 C 收尾代码，资源释放必须由真实受保护接口安排。C++ 分支可采用另一恢复机制，不能据此类型推断统一析构保证。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:185：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/exception.h:29：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/exception/exception_internal.h:18：定义、配套消费或失败边界证据。
 */

/**
 * @typedef TZrDebugSignal
 * @brief TZrDebugSignal：为指令分派和调试钩子之间的待处理信号提供标量载体。
 *
 * ## Summary
 * 为指令分派和调试钩子之间的待处理信号提供标量载体。
 *
 * ## Value model
 * sig_atomic_t；值由 ZR_DEBUG_SIGNAL_* 约定及消费方解释。
 *
 * ## Invariants and lifecycle
 * 该 C 类型不等于任意线程间同步原语；实际 volatile 字段、锁和调用协议仍须遵守。信号值不拥有调试器或回调对象。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:188：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/call_info.h:53：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:270：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:271：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:282：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:665：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_LIKELY
 * @brief ZR_LIKELY：在分支热点中表达预期方向，保持条件只在展开中出现一次。
 *
 * ## Summary
 * 在分支热点中表达预期方向，保持条件只在展开中出现一次。
 *
 * ## Parameters
 * @param[in] CONDITION 条件表达式，按所选分支求值一次；应按实际使用上下文处理结果类型。
 *
 * ## Returns
 * @return GNU 分支交付 builtin_expect 的真假值，其它分支保留 CONDITION 的值和类型。
 *
 * ## Constraints and lifecycle
 * GNU 分支以 CONDITION != ZR_FALSE 规范化为真假后传给 __builtin_expect，预期值为 1；其它分支直接返回 CONDITION 表达式。
 * Clang 先选 ZR_COMPILER_CLANG，因此不走仅 GNU 的此分支；非 GNU 下条件类型和值不被本宏规范化。只作预测，不保证线程同步或改变失败处理。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:196：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:195：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:57：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:194：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:695：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:755：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_UNLIKELY
 * @brief ZR_UNLIKELY：在分支热点中表达预期方向，保持条件只在展开中出现一次。
 *
 * ## Summary
 * 在分支热点中表达预期方向，保持条件只在展开中出现一次。
 *
 * ## Parameters
 * @param[in] CONDITION 条件表达式，按所选分支求值一次；应按实际使用上下文处理结果类型。
 *
 * ## Returns
 * @return GNU 分支交付 builtin_expect 的真假值，其它分支保留 CONDITION 的值和类型。
 *
 * ## Constraints and lifecycle
 * GNU 分支以 CONDITION != ZR_FALSE 规范化为真假后传给 __builtin_expect，预期值为 0；其它分支直接返回 CONDITION 表达式。
 * Clang 先选 ZR_COMPILER_CLANG，因此不走仅 GNU 的此分支；非 GNU 下条件类型和值不被本宏规范化。只作预测，不保证线程同步或改变失败处理。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:197：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:195：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/memory.c:43：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/base/memory.c:66：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:59：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:681：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_ABORT
 * @brief ZR_ABORT：在无法继续的宿主层路径终止进程。
 *
 * ## Summary
 * 在无法继续的宿主层路径终止进程。
 *
 * ## Returns
 * 没有普通返回值；终止行为由宿主 abort 实现。
 *
 * ## Constraints and lifecycle
 * 直接调用 C abort()，不进行 VM 异常分发。
 * 不是普通失败返回、TryRun 或清理入口；不得依赖普通作用域尾部释放及 atexit 作为此宏的完成保证。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:203：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1086：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1105：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1127：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/debug/debug.c:1136：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_MACRO_REGISTER_WRAP
 * @brief ZR_MACRO_REGISTER_WRAP：把成组注册项放在调用方给出的起止 token 之间。
 *
 * ## Summary
 * 把成组注册项放在调用方给出的起止 token 之间。
 *
 * ## Parameters
 * @param[in] WRAP_START 序列前的 token 或起始声明。
 * @param[in] WRAP_END 序列后的 token 或结束声明。
 * @param[in] ... 在二者之间展开的注册项 token；可由配套注册宏进一步展开。
 *
 * ## Returns
 * 无函数返回值；向消费点交付 token 序列。
 *
 * ## Constraints and lifecycle
 * 顺序拼接 WRAP_START、__VA_ARGS__、WRAP_END，不自身迭代或执行运行时代码。
 * 参数必须组成合法预处理 token；枚举、标签地址表等不同展开上下文由消费者负责。宏参数在替换体各出现一次，不代表每个嵌套注册项只执行一次。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:207：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_meta_conf.h:53：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:301：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:302：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:312：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_meta_conf.h:60：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_MSVC__noreturn_standard_header_order MSVC::noreturn_standard_header_order
 * @brief MSVC::noreturn_standard_header_order：在包含公共基础定义前处理 MSVC 环境下已有 noreturn 宏及基础标准头顺序。
 *
 * ## Summary
 * 在包含公共基础定义前处理 MSVC 环境下已有 noreturn 宏及基础标准头顺序。
 *
 * ## Value model
 * _MSC_VER 下若 noreturn 宏已定义则 undef；随后先包含 stdlib.h/stddef.h，再进入公共标准头集合。
 *
 * ## Invariants and lifecycle
 * 这是预处理环境适配，不调整编译器搜索路径；不会恢复被 undef 的外来宏，也不证明任意 MSVC SDK 组合可构建。clang-cl 同样可能定义 _MSC_VER。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:8：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_common_conf.h:15：定义、配套消费或失败边界证据。
 */
