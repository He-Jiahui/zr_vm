#include "zr_vm_core/execution_budget.h"
#include "zr_vm_core/global.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#endif

static TZrUInt64 memory_load(const volatile TZrUInt64 *value) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)InterlockedCompareExchange64((volatile LONG64 *)value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static TZrBool memory_compare_exchange(volatile TZrUInt64 *value, TZrUInt64 expected, TZrUInt64 next) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrUInt64)InterlockedCompareExchange64((volatile LONG64 *)value,
                                                  (LONG64)next, (LONG64)expected) == expected;
#else
    return __atomic_compare_exchange_n(value, &expected, next, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

static void memory_lock(SZrGlobalState *global) {
#if defined(ZR_PLATFORM_WIN)
    while (InterlockedCompareExchange((volatile LONG *)&global->allocationAccountingLock, 1, 0) != 0) {
    }
#else
    while (__atomic_exchange_n(&global->allocationAccountingLock, 1, __ATOMIC_ACQUIRE) != 0) {
    }
#endif
}

static void memory_unlock(SZrGlobalState *global) {
#if defined(ZR_PLATFORM_WIN)
    (void)InterlockedExchange((volatile LONG *)&global->allocationAccountingLock, 0);
#else
    __atomic_store_n(&global->allocationAccountingLock, 0, __ATOMIC_RELEASE);
#endif
}

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

TZrPtr ZrCore_ExecutionBudget_Allocate(TZrPtr userData, TZrPtr pointer,
        TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    SZrGlobalState *global = (SZrGlobalState *)userData;
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

TZrUInt64 ZrCore_ExecutionBudget_BeginMemory(SZrGlobalState *global) {
    TZrUInt64 current;
    TZrUInt64 peak;
    /* Reset the call window atomically with allocation accounting, including
     * allocations performed by another mutator or a GC worker. */
    memory_lock(global);
    current = memory_load(&global->allocatedBytes);
    peak = memory_load(&global->allocationPeakBytes);
    while (!memory_compare_exchange(&global->allocationPeakBytes, peak, current)) {
        peak = memory_load(&global->allocationPeakBytes);
    }
    memory_unlock(global);
    return current;
}

TZrUInt64 ZrCore_ExecutionBudget_MemoryPeak(const SZrGlobalState *global) {
    return memory_load(&global->allocationPeakBytes);
}
