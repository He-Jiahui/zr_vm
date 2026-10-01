#ifndef ZR_VM_CORE_EXCEPTION_INTERNAL_H
#define ZR_VM_CORE_EXCEPTION_INTERNAL_H

#include "zr_vm_core/exception.h"
#include <setjmp.h>

#if defined(__cplusplus) && !defined(ZR_EXCEPTION_WITH_LONG_JUMP)
#define ZR_EXCEPTION_NATIVE_THROW(state, context) throw(context)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block) \
    try { block } catch (...) { \
        if ((context)->status == ZR_THREAD_STATUS_FINE) \
            (context)->status = ZR_THREAD_STATUS_INVALID; \
    }
#else
#define ZR_EXCEPTION_NATIVE_THROW(state, context) longjmp((context)->jumpBuffer, 1)
#define ZR_EXCEPTION_NATIVE_TRY(state, context, block) \
    if (setjmp((context)->jumpBuffer) == 0) { block }
#endif

struct SZrState;
void ZrCore_Exception_RestoreLocalTryRunScopes(struct SZrState *state);

#endif
