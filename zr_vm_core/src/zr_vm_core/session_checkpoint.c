#include "session_checkpoint_private.h"

/* 会话回滚在同一对象身份上恢复支持的逻辑状态；原快照继续保留，允许后续再次回滚。 */
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
    /* 暂停同 GC 域的 mutator 后先完成所有可失败的暂存、句柄解析与写屏障容量预留；
     * 失败只释放暂存计划，不先改写当前线程边界或提交快照对象图。 */
    restored = checkpoint_prepare_restore_plan(
            state, checkpoint, &plan, &roots, &needsReset);
    if (restored) {
        if (needsReset) {
            /* 预检已排除会触发清理的活动作用域及被丢弃栈槽的 ownership；
             * 将归一化推迟到全部可失败步骤之后，提交阶段不再分配或回调 guest。 */
            (void)ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
        }
        checkpoint_commit_restore_plan(state, checkpoint, plan, roots);
    }
    ZrCore_GcDomain_StopTheWorldEnd(state);
    return restored;
}
