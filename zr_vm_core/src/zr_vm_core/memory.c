//
// Created by HeJiahui on 2025/6/16.
//
#include "zr_vm_core/memory.h"

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/gc.h"


/* GcMalloc 的兜底入口：仅在全局可用且未进入即时回收时，尝试完整 GC 后重试原生申请。 */
TZrPtr ZrCore_Memory_GcAndMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size) {
    ZR_UNUSED_PARAMETER(type);
    SZrGlobalState *global = state->global;
    if (ZrCore_GlobalState_IsInitialized(global) && !global->garbageCollector->isImmediateGcFlag) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        return ZrCore_Memory_RawMallocWithType(global, size, type);
    }
    return ZR_NULL;
}

/* 扩容的第二次尝试沿用同一旧块；即时 GC 内禁止再次启动完整回收。 */
static TZrPtr zr_core_memory_gc_and_reallocate(SZrState *state,
                                               TZrPtr pointer,
                                               TZrSize originalSize,
                                               TZrSize newSize,
                                               EZrMemoryNativeType type) {
    SZrGlobalState *global = state->global;

    if (ZrCore_GlobalState_IsInitialized(global) && !global->garbageCollector->isImmediateGcFlag) {
        ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
        return ZrCore_Memory_Allocate(global, pointer, originalSize, newSize, type);
    }
    return ZR_NULL;
}

/* 对对象和哈希 pair 等 GC 路径统一处理 OOM；债务仅在取得新块后增加。 */
TZrPtr ZrCore_Memory_GcMalloc(SZrState *state, EZrMemoryNativeType type, TZrSize size) {
    ZR_ASSERT(size > 0);
    SZrGlobalState *global = state->global;
    TZrPtr pointer = ZrCore_Memory_RawMallocWithType(global, size, type);
    if (ZR_UNLIKELY(pointer == ZR_NULL)) {
        pointer = ZrCore_Memory_GcAndMalloc(state, type, size);
        if (pointer == ZR_NULL) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
    }
    /* BUG: 同域多个 mutator 可同时创建对象；gcDebtSize 非原子且此处无共同锁，
     * 并发成功分配会竞争读写，丢失债务或使后续 GC 调度依据失真。 */
    /* BUG: AddDebtSpace 可先将债务饱和至 ZR_MAX_MEMORY_OFFSET；此时成功分配
     * 仍用有符号加法，产生溢出未定义行为，可能使安全点跳过应执行的 GC。 */
    global->garbageCollector->gcDebtSize += (TZrMemoryOffset) size;
    return pointer;
}

/* 原生块增长先走宿主分配器，失败时按状态尝试完整 GC；零大小释放不进入重试。 */
TZrPtr ZrCore_Memory_GcReallocate(SZrState *state,
                                  TZrPtr pointer,
                                  TZrSize originalSize,
                                  TZrSize newSize,
                                  EZrMemoryNativeType type) {
    SZrGlobalState *global;
    TZrPtr result;

    ZR_ASSERT(state != ZR_NULL);
    global = state->global;
    result = ZrCore_Memory_Allocate(global, pointer, originalSize, newSize, type);
    if (ZR_UNLIKELY(result == ZR_NULL && newSize > 0)) {
        result = zr_core_memory_gc_and_reallocate(state, pointer, originalSize, newSize, type);
        if (result == ZR_NULL) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
    }
    if (result != ZR_NULL && newSize > originalSize) {
        /* TODO: 此公开扩容入口暂无仓内调用；启用前须核查这里与 GcMalloc
         * 同样缺少并发同步和债务饱和保护的条件路径。 */
        global->garbageCollector->gcDebtSize += (TZrMemoryOffset)(newSize - originalSize);
    }
    return result;
}
