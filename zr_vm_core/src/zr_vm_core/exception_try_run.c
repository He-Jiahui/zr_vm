#include "exception_internal.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

typedef struct SZrExceptionTryRunContext {
    SZrExceptionLongJump recoverPoint;
    SZrAotGcRootFrame *savedAotGcRootFrameTop;
    TZrUInt32 savedAotGcRootFrameDepth;
    SZrGcDomainScopeSnapshot savedGcScopes;
} SZrExceptionTryRunContext;

void ZrCore_Exception_RestoreLocalTryRunScopes(SZrState *state) {
    const volatile SZrExceptionTryRunContext *context =
            (const volatile SZrExceptionTryRunContext *)state->exceptionRecoverPoint;
    SZrGcDomainScopeSnapshot snapshot = context->savedGcScopes;
    /* Abandoned C-local roots must disappear before any inactive publication. */
    state->aotGcRootFrameStack = context->savedAotGcRootFrameTop;
    state->aotGcRootFrameDepth = context->savedAotGcRootFrameDepth;
    ZrCore_GcDomain_RestoreScopes(state, &snapshot);
}

EZrThreadStatus ZrCore_Exception_TryRun(
        SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments) {
    const TZrUInt32 prevNestedNativeCalls = state->nestedNativeCalls;
    volatile TZrBool callbackReturnedNormally = ZR_FALSE;
    /* The recovery status changes after setjmp; keep the containing automatic
     * object volatile as well so its saved fields remain defined after longjmp. */
    volatile SZrExceptionTryRunContext context;
    SZrGcDomainScopeSnapshot snapshot;
    SZrExceptionLongJump *recoverPoint = (SZrExceptionLongJump *)&context.recoverPoint;

    context.recoverPoint.status = ZR_THREAD_STATUS_FINE;
    context.recoverPoint.previous = state->exceptionRecoverPoint;
    context.savedAotGcRootFrameTop = state->aotGcRootFrameStack;
    context.savedAotGcRootFrameDepth = state->aotGcRootFrameDepth;
    ZrCore_GcDomain_CaptureScopes(state, &snapshot);
    context.savedGcScopes = snapshot;
    state->exceptionRecoverPoint = recoverPoint;
    ZR_EXCEPTION_NATIVE_TRY(state, recoverPoint, {
        tryFunction(state, arguments);
        callbackReturnedNormally = ZR_TRUE;
    });
    state->exceptionRecoverPoint = context.recoverPoint.previous;
    state->nestedNativeCalls = prevNestedNativeCalls;
    if (!callbackReturnedNormally) {
        /* Local Throw already restored GC scopes before unwinding. Catch only
         * repeats root restoration; legacy forwarding owns no local lock lease. */
        state->aotGcRootFrameStack = context.savedAotGcRootFrameTop;
        state->aotGcRootFrameDepth = context.savedAotGcRootFrameDepth;
    }
    return context.recoverPoint.status;
}
