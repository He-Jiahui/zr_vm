#include "exception/exception_internal.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"

/* recoverPoint必须为首成员以便从state恢复私有context；只在TryRun调用栈有效 */
typedef struct SZrExceptionTryRunContext {
/* 首成员地址与context起址一致，不可独立分配后交恢复helper */
    SZrExceptionLongJump recoverPoint;
/* 仅保护本恢复点的状态规范化；不能借用 status，否则外来 C++ 异常失去 INVALID 语义。 */
    TZrBool statusNormalizationInProgress;
/* A cleanup boundary inside Error construction must not reopen normalization.
 * Kept separate so End only clears normalization owned by this context. */
    TZrBool inheritedNormalizationSuppression;
/* 不遍历被放弃的回调C栈节点；外层节点必须活到TryRun返回 */
    SZrAotGcRootFrame *savedAotGcRootFrameTop;
/* 与top一并恢复；不凭该数修复正常返回回调的根泄漏 */
    TZrUInt32 savedAotGcRootFrameDepth;
/* 不拥有注册指针；恢复要求域identity、mutatorId仍匹配且外层mutation未被释放 */
    SZrGcDomainScopeSnapshot savedGcScopes;
} SZrExceptionTryRunContext;

/* 与恢复快照相同，仅接受活动 TryRun context 的首成员恢复点。 */
TZrBool ZrCore_Exception_BeginStatusNormalization(SZrExceptionLongJump *recoverPoint) {
    volatile SZrExceptionTryRunContext *context =
            (volatile SZrExceptionTryRunContext *)recoverPoint;
    if (context->statusNormalizationInProgress || context->inheritedNormalizationSuppression) {
        return ZR_FALSE;
    }
    context->statusNormalizationInProgress = ZR_TRUE;
    return ZR_TRUE;
}

void ZrCore_Exception_EndStatusNormalization(SZrExceptionLongJump *recoverPoint) {
    volatile SZrExceptionTryRunContext *context =
            (volatile SZrExceptionTryRunContext *)recoverPoint;
    context->statusNormalizationInProgress = ZR_FALSE;
}

/* 只接收由当前活动 TryRun 建立的首成员恢复点；先还原 AOT 根链再让 GC scopes 发布 mutator 状态。 */
void ZrCore_Exception_RestoreLocalTryRunScopes(SZrState *state) {
    const volatile SZrExceptionTryRunContext *context =
            (const volatile SZrExceptionTryRunContext *)state->exceptionRecoverPoint;
    SZrGcDomainScopeSnapshot snapshot = context->savedGcScopes;
    /* Abandoned C-local roots must disappear before any inactive publication. */
    state->aotGcRootFrameStack = context->savedAotGcRootFrameTop;
    state->aotGcRootFrameDepth = context->savedAotGcRootFrameDepth;
    ZrCore_GcDomain_RestoreScopes(state, &snapshot);
}

/* arguments 只借用到返回；嵌套恢复 previous 与 nestedNativeCalls；正常返回维持 FINE，不代替回调平衡 scopes 或读取 threadStatus。 */
EZrThreadStatus ZrCore_Exception_TryRun(
        SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments) {
    const TZrUInt32 prevNestedNativeCalls = state->nestedNativeCalls;
    const TZrUInt32 prevNestedNativeCallYieldFlag = state->nestedNativeCallYieldFlag;
    volatile TZrBool callbackReturnedNormally = ZR_FALSE;
    /* The recovery status changes after setjmp; keep the containing automatic
     * object volatile as well so its saved fields remain defined after longjmp. */
    volatile SZrExceptionTryRunContext context;
    SZrGcDomainScopeSnapshot snapshot;
    SZrExceptionLongJump *recoverPoint = (SZrExceptionLongJump *)&context.recoverPoint;

/* 回调只看到栈上recoverPoint；GC注册变化不会被快照凭空修复 */
    context.recoverPoint.status = ZR_THREAD_STATUS_FINE;
    context.recoverPoint.previous = state->exceptionRecoverPoint;
    context.statusNormalizationInProgress = ZR_FALSE;
    context.inheritedNormalizationSuppression = ZR_FALSE;
    if (context.recoverPoint.previous != ZR_NULL) {
        const volatile SZrExceptionTryRunContext *outer =
                (const volatile SZrExceptionTryRunContext *)context.recoverPoint.previous;
        context.inheritedNormalizationSuppression =
                (TZrBool)(outer->statusNormalizationInProgress || outer->inheritedNormalizationSuppression);
    }
    context.savedAotGcRootFrameTop = state->aotGcRootFrameStack;
    context.savedAotGcRootFrameDepth = state->aotGcRootFrameDepth;
    ZrCore_GcDomain_CaptureScopes(state, &snapshot);
    context.savedGcScopes = snapshot;
    state->exceptionRecoverPoint = recoverPoint;
/* C longjmp 分支的 Throw(FINE) 与普通返回都可得到 FINE，正常返回标志区分退出方式；C++ catch 将捕获时仍为 FINE 的状态改为 INVALID */
    ZR_EXCEPTION_NATIVE_TRY(state, recoverPoint, {
        tryFunction(state, arguments);
        callbackReturnedNormally = ZR_TRUE;
    });
    state->exceptionRecoverPoint = context.recoverPoint.previous;
    state->nestedNativeCalls = prevNestedNativeCalls;
    state->nestedNativeCallYieldFlag = prevNestedNativeCallYieldFlag;
/* 不再次释放域锁；本地Throw已恢复scopes，旧转发路径无本地锁租约 */
    if (!callbackReturnedNormally) {
        /* Local Throw already restored GC scopes before unwinding. Catch only
         * repeats root restoration; legacy forwarding owns no local lock lease. */
        state->aotGcRootFrameStack = context.savedAotGcRootFrameTop;
        state->aotGcRootFrameDepth = context.savedAotGcRootFrameDepth;
    }
    return context.recoverPoint.status;
}
