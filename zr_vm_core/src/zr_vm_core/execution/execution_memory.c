#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/global.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#endif

/* 在预算轮询与分配器可能并发时读取全局计数；volatile 本身不足以保证跨线程原子性。 */
static TZrUInt64 memory_load(const volatile TZrUInt64 *value) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)InterlockedCompareExchange64((volatile LONG64 *)value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

/* 峰值推进与新窗口复位共用原子条件更新，避免覆盖其他 mutator 的新样本。 */
static TZrBool memory_compare_exchange(volatile TZrUInt64 *value, TZrUInt64 expected, TZrUInt64 next) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)InterlockedCompareExchange64((volatile LONG64 *)value,
                                                  (LONG64)next, (LONG64)expected) == expected;
#else
    return __atomic_compare_exchange_n(value, &expected, next, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

/* 将成功分配的记账与 BeginMemory 的窗口起点串行化，锁只保护统计字段。 */
static void memory_lock(SZrGlobalState *global) {
#if defined(ZR_PLATFORM_WIN)
    while (InterlockedCompareExchange((volatile LONG *)&global->allocationAccountingLock, 1, 0) != 0) {
    }
#else
    while (__atomic_exchange_n(&global->allocationAccountingLock, 1, __ATOMIC_ACQUIRE) != 0) {
    }
#endif
}

/* 发布本次统计更新，让后续预算窗口看到完整的已分配字节数。 */
static void memory_unlock(SZrGlobalState *global) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange((volatile LONG *)&global->allocationAccountingLock, 0);
#else
    __atomic_store_n(&global->allocationAccountingLock, 0, __ATOMIC_RELEASE);
#endif
}

/* 按 VM 调用方传入的旧、新请求大小更新计数；饱和边界防止错误大小导致整数回绕。 */
static TZrUInt64 memory_update(volatile TZrUInt64 *value, TZrSize removed, TZrSize added) {
    TZrUInt64 previous;
    TZrUInt64 next;
    do {
        previous = memory_load(value);
        next = previous >= removed ? previous - removed : 0;
        next = (TZrUInt64)-1 - next >= added ? next + added : (TZrUInt64)-1;
    } while (!memory_compare_exchange(value, previous, next));
    return next;
}

/* GlobalState_New 安装的分配器外层：先让宿主决定所有权，再统计成功的申请或释放。 */
TZrPtr ZrCore_ExecutionBudget_Allocate(TZrPtr userData, TZrPtr pointer,
        TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    SZrGlobalState *global = (SZrGlobalState *)userData;
    /* BUG: 同域两个 mutator 可并发调用；A 完成申请而尚未记账时，B 可先释放
     * 另一块并记账，随后 A 的记账会漏掉两块曾同时存活的峰值，堆预算可能低报。 */
    TZrPtr result = global->upstreamAllocator(global->upstreamAllocationArguments,
                                              pointer, originalSize, newSize, flag);
    TZrUInt64 current;
    TZrUInt64 peak;
    if (newSize != 0 && result == ZR_NULL) {
        return result;
    }
    memory_lock(global);
    current = memory_update(&global->allocatedBytes, pointer != ZR_NULL ? originalSize : 0, newSize);
    peak = memory_load(&global->allocationPeakBytes);
    while (peak < current && !memory_compare_exchange(&global->allocationPeakBytes, peak, current)) {
        peak = memory_load(&global->allocationPeakBytes);
    }
    memory_unlock(global);
    return result;
}

/* 有界项目调用开始时，将峰值窗口归到当前全局存量；返回值可作调用的基线。 */
TZrUInt64 ZrCore_ExecutionBudget_BeginMemory(SZrGlobalState *global) {
    TZrUInt64 current;
    TZrUInt64 peak;
    /* 窗口复位与已完成的分配记账同锁；同一 global 的 mutator 和 GC worker 共用计数。 */
    memory_lock(global);
    current = memory_load(&global->allocatedBytes);
    peak = memory_load(&global->allocationPeakBytes);
    while (!memory_compare_exchange(&global->allocationPeakBytes, peak, current)) {
        peak = memory_load(&global->allocationPeakBytes);
    }
    memory_unlock(global);
    return current;
}

/* Poll 读取当前窗口峰值，以协作式边界决定是否达到堆限制。 */
TZrUInt64 ZrCore_ExecutionBudget_MemoryPeak(const SZrGlobalState *global) {
    return memory_load(&global->allocationPeakBytes);
}
