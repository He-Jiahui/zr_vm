//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_MEMORY_H
#define ZR_VM_CORE_MEMORY_H

#include <string.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

/** @brief 把原生块的申请、扩容或释放交给 VM 的宿主分配器，并供全局预算统计观察。
 * @pre global 存活；非空旧块须与 originalSize、type 对应；释放时须提供非空旧块及非零旧大小。
 * @return 原样返回宿主分配器结果；本层不触发 GC，也不抛出内存异常。
 * TODO: FZrAllocator 契约未明确扩容失败时旧块是否保留；string_builder、hash_set 等调用方依赖保留旧块，需统一自定义分配器约束。
 */
ZR_FORCE_INLINE TZrPtr ZrCore_Memory_Allocate(SZrGlobalState *global, TZrPtr pointer, TZrSize originalSize, TZrSize newSize,
                                        EZrMemoryNativeType type) {
    ZR_ASSERT((pointer != ZR_NULL && originalSize != 0) || newSize != 0);
    return global->allocator(global->userAllocationArguments, pointer, originalSize, newSize, type);
}

/** @brief 为编译器和 VM 辅助结构取得不经 GC 重试的原生块，类别为 NONE。
 * @return 分配器返回的块或失败时的 null；调用方持有并负责用同一 global 释放。
 * @note 零字节请求的返回值取决于宿主分配器。
 */
ZR_FORCE_INLINE TZrPtr ZrCore_Memory_RawMalloc(SZrGlobalState *global, TZrSize size) {
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size, ZR_MEMORY_NATIVE_TYPE_NONE);
}

/** @brief 申请带用途标签的原生块，供对象附属缓冲区、GC 元数据等向宿主传递类别。
 * @return 分配器返回的块或失败时的 null；本层不执行 GC 重试。
 * @note type 透传至宿主分配器，调用方负责按原大小和类别释放。
 */
ZR_FORCE_INLINE TZrPtr ZrCore_Memory_RawMallocWithType(SZrGlobalState *global, TZrSize size, EZrMemoryNativeType type) {
    ZR_UNUSED_PARAMETER(type)
    return global->allocator(global->userAllocationArguments, ZR_NULL, 0, size, type);
}

/** @brief 原生申请失败后在允许回收的状态尝试完整 GC，再用同类别重试；由 GcMalloc 的失败分支调用。
 * @return 全局状态未初始化、已在即时 GC 中或重试失败时为 null；此入口本身不增加 GC 债务。
 */
ZR_CORE_API TZrPtr ZrCore_Memory_GcAndMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size);

/** @brief 为 GC 管理对象和辅助节点取得原生存储，并把成功申请计入 GC 债务。
 * @pre state 拥有有效 global/collector，size 大于零。
 * @return 成功时返回块；首次失败时若允许回收则尝试完整 GC 后重试，最终失败转入内存异常处理。
 * @note 成功返回的块尚未归属对象或容器，调用方须及时挂入相应持有结构。
 */
ZR_CORE_API TZrPtr ZrCore_Memory_GcMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size);

/** @brief 为需要异常语义的原生块扩容；首次失败时若允许回收则尝试完整 GC 并重试一次。
 * @pre pointer/originalSize 属于同一 global 的分配器；GC 重试期间旧块仍须有效。
 * @return 成功时返回新块并只对增长部分计 GC 债务；newSize 为零时按宿主释放协议返回，非零申请重试失败则抛内存异常。
 */
ZR_CORE_API TZrPtr ZrCore_Memory_GcReallocate(SZrState *state,
                                              TZrPtr pointer,
                                              TZrSize originalSize,
                                              TZrSize newSize,
                                              EZrMemoryNativeType type);

/** @brief 将已取得的原生存储初始化为指定字节，常用于对象和 GC 元数据初始状态。
 * @pre destination 非空且 byteCount 非零；此操作不分配内存。
 */
ZR_FORCE_INLINE void ZrCore_Memory_RawSet(TZrPtr destination, TZrByte byte, TZrSize byteCount) {
    ZR_ASSERT(destination != ZR_NULL && byteCount != 0);
    memset(destination, byte, byteCount);
}

/** @brief 释放类别为 NONE 的原生块，使预算记账扣除原申请大小。
 * @pre pointer 非空、size 非零，并由同一 global 的分配器取得。
 */
ZR_FORCE_INLINE void ZrCore_Memory_RawFree(SZrGlobalState *global, TZrPtr pointer, TZrSize size) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0, ZR_MEMORY_NATIVE_TYPE_NONE);
}
/** @brief 按原类别释放带标签的原生块；GC 元数据及对象附属数组由持有者调用。
 * @pre pointer 非空、size 非零，global 与 type 均须匹配原分配。
 */
ZR_FORCE_INLINE void ZrCore_Memory_RawFreeWithType(SZrGlobalState *global, TZrPtr pointer, TZrSize size,
                                             EZrMemoryNativeType type) {
    ZR_ASSERT(pointer != ZR_NULL && size != 0);
    global->allocator(global->userAllocationArguments, pointer, size, 0, type);
}
/** @brief 以元素数释放 FUNCTION 类别的数组；函数销毁及编译期临时表共用此换算。
 * @pre POINTER 非空且 SIZE 为非零的已分配元素数。
 */
#define ZR_MEMORY_RAW_FREE_LIST(GLOBAL, POINTER, SIZE)                                                                 \
    ZrCore_Memory_RawFreeWithType((GLOBAL), (POINTER), (SIZE) * sizeof(*(POINTER)), ZR_MEMORY_NATIVE_TYPE_FUNCTION)

/** @brief 复制已分配的原生字节序列；零长度时允许空指针并直接返回。
 * @pre 非零长度时源与目标有效且不重叠，调用方保证目标容量。
 */
ZR_FORCE_INLINE void ZrCore_Memory_RawCopy(TZrPtr destination, TZrPtr source, TZrSize size) {
    if (size == 0) {
        return;
    }
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL);
    memcpy(destination, source, size);
}

/** @brief 按原始字节比较字符串或签名片段；零长度视为相等。
 * @pre 非零长度时两个地址均指向至少 size 字节的可读区域。
 */
ZR_FORCE_INLINE TZrInt32 ZrCore_Memory_RawCompare(TZrPtr destination, TZrPtr source, TZrSize size) {
    if (size == 0) {
        return 0;
    }
    ZR_ASSERT(destination != ZR_NULL && source != ZR_NULL);
    return memcmp(destination, source, size);
}

#endif // ZR_VM_CORE_MEMORY_H
