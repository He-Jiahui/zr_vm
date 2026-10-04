#include "session_checkpoint_private.h"

TZrBool ZrCore_SessionCheckpoint_Rollback(SZrState *state, const SZrSessionCheckpoint *checkpoint) {
    SZrGcDomainPauseDiagnostic pauseDiagnostic;
    ZrCheckpointRestorePlan *plan = ZR_NULL;
    ZrCheckpointRestoreRoots roots;
    TZrBool needsReset = ZR_FALSE;
    TZrBool restored;

    if (state == ZR_NULL || checkpoint == ZR_NULL || checkpoint->state != state ||
        state->executionBudget != ZR_NULL || state->nestedNativeCalls != 0u ||
        state->nestedNativeCallYieldFlag != 0u ||
        state->aotGcRootFrameDepth != 0u || state->aotGcRootFrameStack != ZR_NULL ||
        state->stackClosureValueList != ZR_NULL ||
        state->stackBase.valuePointer == ZR_NULL ||
        state->toBeClosedValueList.valuePointer != state->stackBase.valuePointer ||
        state->exceptionRecoverPoint != ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&pauseDiagnostic, 0, sizeof(pauseDiagnostic));
    if (!ZrCore_GcDomain_StopTheWorldBegin(state, 1000u, &pauseDiagnostic)) {
        return ZR_FALSE;
    }
    restored = checkpoint_prepare_restore_plan(
            state, checkpoint, &plan, &roots, &needsReset);
    if (restored) {
        if (needsReset) {
            /* Preflight proved ResetThread has no owned scopes and cannot allocate or fail. */
            (void)ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        }
        checkpoint_commit_restore_plan(state, checkpoint, plan, roots);
    }
    ZrCore_GcDomain_StopTheWorldEnd(state);
    return restored;
}
