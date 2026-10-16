/* All published debt accesses use the same atomic, saturating protocol. */
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"

_Static_assert(sizeof(TZrMemoryOffset) <= sizeof(TZrInt64), "GC debt fits the public telemetry type");

#if defined(ZR_PLATFORM_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

TZrMemoryOffset ZrCore_GarbageCollector_GetDebtSpace(const SZrGarbageCollector *collector) {
#if defined(ZR_PLATFORM_WIN)
#if defined(_WIN64)
    return (TZrMemoryOffset)InterlockedCompareExchange64(
            (volatile LONG64 *)&collector->gcDebtSize, 0, 0);
#else
    return (TZrMemoryOffset)InterlockedCompareExchange(
            (volatile LONG *)&collector->gcDebtSize, 0, 0);
#endif
#else
    return __atomic_load_n(&collector->gcDebtSize, __ATOMIC_RELAXED);
#endif
}

/* On failure, replace expected with the latest value before retrying. */
static TZrBool gc_debt_compare_exchange(SZrGarbageCollector *collector,
                                        TZrMemoryOffset *expected,
                                        TZrMemoryOffset desired) {
#if defined(ZR_PLATFORM_WIN)
    TZrMemoryOffset observed;
#if defined(_WIN64)
    observed = (TZrMemoryOffset)InterlockedCompareExchange64(
            (volatile LONG64 *)&collector->gcDebtSize, desired, *expected);
#else
    observed = (TZrMemoryOffset)InterlockedCompareExchange(
            (volatile LONG *)&collector->gcDebtSize, desired, *expected);
#endif
    if (observed == *expected) return ZR_TRUE;
    *expected = observed;
    return ZR_FALSE;
#else
    return (TZrBool)__atomic_compare_exchange_n(&collector->gcDebtSize,
            expected, desired, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
#endif
}

void ZrCore_GarbageCollector_AddDebtSpace(SZrGlobalState *global, TZrMemoryOffset size) {
    SZrGarbageCollector *collector = global->garbageCollector;
    TZrMemoryOffset current = ZrCore_GarbageCollector_GetDebtSpace(collector);
    TZrMemoryOffset next;
    do {
        if (size > 0) {
            next = current > ZR_MAX_MEMORY_OFFSET - size
                           ? ZR_MAX_MEMORY_OFFSET : current + size;
        } else {
            /* Negate the nonnegative debt, never the possibly minimum delta. */
            next = size < -current ? 0 : current + size;
        }
    } while (!gc_debt_compare_exchange(collector, &current, next));
}
