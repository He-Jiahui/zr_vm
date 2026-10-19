#include "syntax/source_identity.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#endif

static volatile TZrUInt64 source_identity_last;

TZrBool ZrParser_SourceIdentity_Issue(TZrUInt64 *outKey) {
    TZrUInt64 current;

    if (outKey == ZR_NULL) {
        return ZR_FALSE;
    }
    *outKey = 0U;
#if defined(ZR_PLATFORM_WIN)
    current = (TZrUInt64)InterlockedCompareExchange64(
            (volatile LONG64 *)&source_identity_last, 0, 0);
#else
    current = __atomic_load_n(&source_identity_last, __ATOMIC_RELAXED);
#endif
    for (;;) {
        TZrUInt64 next;
        if (current == ~(TZrUInt64)0) {
            return ZR_FALSE;
        }
        next = current + 1U;
#if defined(ZR_PLATFORM_WIN)
        {
            TZrUInt64 observed = (TZrUInt64)InterlockedCompareExchange64(
                    (volatile LONG64 *)&source_identity_last,
                    (LONG64)next, (LONG64)current);
            if (observed == current) {
                *outKey = next;
                return ZR_TRUE;
            }
            current = observed;
        }
#else
        if (__atomic_compare_exchange_n(&source_identity_last, &current, next,
                                        0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            *outKey = next;
            return ZR_TRUE;
        }
#endif
    }
}
