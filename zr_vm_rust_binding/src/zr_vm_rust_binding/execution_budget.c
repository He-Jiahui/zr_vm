#include "internal.h"

#include <string.h>

TZrUInt64 ZrRustBinding_ExecutionNowMicros(void) {
    return ZrCore_ExecutionBudget_NowMicros();
}

ZrRustBindingCancellationToken *ZrRustBinding_CancellationToken_New(void) {
    return ZrCore_ExecutionCancelToken_New();
}

void ZrRustBinding_CancellationToken_Cancel(ZrRustBindingCancellationToken *token) {
    ZrCore_ExecutionCancelToken_Cancel(token);
}

TZrBool ZrRustBinding_CancellationToken_IsCancelled(const ZrRustBindingCancellationToken *token) {
    return ZrCore_ExecutionCancelToken_IsCancelled(token);
}

void ZrRustBinding_CancellationToken_Free(ZrRustBindingCancellationToken *token) {
    ZrCore_ExecutionCancelToken_Free(token);
}

/* 预算只绑定这一轮 session 导出；退出时无论成功或终止都撤销 state 指针并填写 usage。 */
ZrRustBindingStatus ZrRustBinding_ProjectSession_CallModuleExportWithBudget(
        ZrRustBindingProjectSession *session,
        const TZrChar *moduleName,
        const TZrChar *exportName,
        ZrRustBindingValue *const *arguments,
        TZrSize argumentCount,
        const ZrRustBindingCallBudget *budget,
        ZrRustBindingCallUsage *outUsage,
        ZrRustBindingValue **outResult) {
    SZrExecutionBudget scope;
    SZrState *state;
    ZrRustBindingStatus status;
    TZrUInt64 startedMicros;
    TZrUInt64 finishedMicros;

    if (outUsage != ZR_NULL) {
        memset(outUsage, 0, sizeof(*outUsage));
    }
    if (outResult != ZR_NULL) {
        *outResult = ZR_NULL;
    }
    if (session == ZR_NULL || session->owner == ZR_NULL || session->owner->global == ZR_NULL ||
        moduleName == ZR_NULL || exportName == ZR_NULL || budget == ZR_NULL ||
        outUsage == ZR_NULL || outResult == ZR_NULL || (argumentCount != 0 && arguments == ZR_NULL)) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "session, export, arguments, budget or output is invalid");
    }
    state = session->owner->global->mainThreadState;
    if (state == ZR_NULL || state->executionBudget != ZR_NULL || session->owner->activeCall) {
        return zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_INVALID_ARGUMENT,
                                         "session state is unavailable or already executing a bounded call");
    }
    memset(&scope, 0, sizeof(scope));
    scope.maxInstructions = budget->maxInstructions;
    scope.deadlineMicros = budget->deadlineMicros;
    scope.cancelToken = budget->cancelToken;
    scope.hasInstructionLimit = budget->hasInstructionLimit;
    scope.hasDeadline = budget->hasDeadline;
    scope.maxHeapBytes = budget->maxHeapBytes;
    scope.maxNativeCalls = budget->maxNativeCalls;
    scope.maxGcMicros = budget->maxGcMicros;
    scope.hasHeapLimit = budget->hasHeapLimit;
    scope.hasNativeCallLimit = budget->hasNativeCallLimit;
    scope.hasGcTimeLimit = budget->hasGcTimeLimit;
    scope.peakHeapBytes = ZrCore_ExecutionBudget_BeginMemory(state->global);
    startedMicros = ZrCore_ExecutionBudget_NowMicros();
    session->owner->activeCall = ZR_TRUE;
    status = zr_rust_binding_call_module_export_with_owner(session->owner,
                                                           moduleName,
                                                           exportName,
                                                           arguments,
                                                           argumentCount,
                                                           outResult,
                                                           &scope);
    while (scope.gcDepth > 0u) {
        ZrCore_ExecutionBudget_GcEnd(state);
    }
    (void)ZrCore_ExecutionBudget_Poll(state, ZR_FALSE);
    if (scope.termination != ZR_EXECUTION_TERMINATION_NONE) {
        if (*outResult != ZR_NULL) {
            zr_rust_binding_value_free_impl(*outResult);
            *outResult = ZR_NULL;
        }
        (void)ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        status = zr_rust_binding_set_error(ZR_RUST_BINDING_STATUS_EXECUTION_TERMINATED,
                                           "module export %s.%s terminated (reason %d)",
                                           moduleName, exportName, (int)scope.termination);
    }
    state->executionBudget = ZR_NULL;
    session->owner->activeCall = ZR_FALSE;
    finishedMicros = ZrCore_ExecutionBudget_NowMicros();
    outUsage->executedInstructions = scope.executedInstructions;
    outUsage->elapsedMicros = finishedMicros >= startedMicros ? finishedMicros - startedMicros : 0;
    outUsage->termination = (ZrRustBindingTermination)scope.termination;
    outUsage->peakHeapBytes = scope.peakHeapBytes;
    outUsage->nativeCalls = scope.nativeCalls;
    outUsage->gcMicros = scope.gcMicros;
    return status;
}
