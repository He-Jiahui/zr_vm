#include "gc/gc_domain_internal.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

static SZrGcDomainMutatorRecord *gc_scopes_find_locked(
        SZrGcDomain *domain, const SZrState *state) {
    for (TZrSize index = 0u; index < domain->mutatorLength; ++index) {
        if (domain->mutators[index].state == state)
            return &domain->mutators[index];
    }
    return ZR_NULL;
}

void ZrCore_GcDomain_CaptureScopes(
        SZrState *state, SZrGcDomainScopeSnapshot *snapshot) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    ZrCore_Memory_RawSet(snapshot, 0, sizeof(*snapshot));
    if (state == ZR_NULL || state->gcDomain == ZR_NULL) return;
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_scopes_find_locked(domain, state);
    if (record != ZR_NULL) {
        snapshot->domain = domain;
        snapshot->identity = domain->identity;
        snapshot->mutatorId = record->mutatorId;
        snapshot->executionDepth = record->executionDepth;
        snapshot->nativeDepth = record->nativeDepth;
        snapshot->mutationDepth = record->mutationDepth;
        snapshot->nativeMode = record->nativeMode;
        snapshot->nativeEnteredFromInactive = record->nativeEnteredFromInactive;
    }
    ZrCore_GcDomain_Unlock(domain);
}

void ZrCore_GcDomain_RestoreScopes(
        SZrState *state, const SZrGcDomainScopeSnapshot *snapshot) {
    SZrGcDomain *domain;
    SZrGcDomainMutatorRecord *record;
    if (state == ZR_NULL || snapshot == ZR_NULL || snapshot->domain == ZR_NULL ||
        state->gcDomain != snapshot->domain) return;
    domain = state->gcDomain;
    ZrCore_GcDomain_Lock(domain);
    record = gc_scopes_find_locked(domain, state);
    if (record == ZR_NULL || record->mutatorId != snapshot->mutatorId ||
        !ZrCore_GcDomain_IdentityEquals(domain->identity, snapshot->identity) ||
        record->mutationDepth < snapshot->mutationDepth) {
        ZrCore_GcDomain_Unlock(domain);
        return;
    }

    /* Release only callback-owned recursive levels. Keep coordination locked
     * until the roots and surviving caller scopes are safe to publish. */
    for (TZrUInt32 depth = record->mutationDepth;
         depth > snapshot->mutationDepth; --depth) {
        ZrCore_GcDomain_MutationUnlock(domain);
    }
    record->mutationDepth = snapshot->mutationDepth;
    record->executionDepth = snapshot->executionDepth;
    record->nativeDepth = snapshot->nativeDepth;
    record->nativeMode = snapshot->nativeMode;
    record->nativeEnteredFromInactive = snapshot->nativeEnteredFromInactive;
    if (record->nativeDepth > 0u &&
        record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED;
    } else if (record->nativeDepth > 0u &&
               record->nativeMode == ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL) {
        record->status = ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL;
    } else {
        record->status = record->executionDepth > 0u || record->nativeDepth > 0u
                ? ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING
                : ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE;
    }
    record->observedEpoch = domain->safepointEpoch;
    ZrCore_GcDomain_Broadcast(domain);
    ZrCore_GcDomain_Unlock(domain);
}

/**
 * @file zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
 *
 * ## Summary
 * 为当前 mutator 查找、捕获并恢复 GC-domain 执行、native 与递归 mutation scope；异常 TryRun 在丢弃回调 C 栈时用快照撤销未配对的 scope。
 *
 * ## Constraints and lifecycle
 * 记录通过 domain coordination lock 访问；快照只复制身份与深度/状态值，不保留 mutator-record 指针，也不延长 domain 生命周期。正常回调返回仍须由回调配对结束自己打开的 scope。
 *
 * ## References
 * ZrCore_Exception_TryRun 在 zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:45 调用 CaptureScopes；本地异常恢复在同文件 :26 调用 RestoreScopes。字段布局见 zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h:42-66。
 */

/**
 * @fn static SZrGcDomainMutatorRecord *gc_scopes_find_locked(SZrGcDomain *domain, const SZrState *state)
 *
 * ## Summary
 * 在已加锁的 mutator 表中按 state 地址查找记录，仅供本文件捕获和恢复快照。
 *
 * ## Parameters
 * @param[in] domain 已持有 coordination lock 的 GC domain。
 * @param[in] state 要匹配的 state 地址；调用者须保证其在查找期间有效。
 *
 * ## Returns
 * @return 找到时返回指向表内记录的借用指针，未找到返回 ZR_NULL。指针只能在表锁仍持有时使用。
 *
 * ## Constraints and lifecycle
 * 本 helper 不加锁、不校验空参数；调用方必须先持有 domain 的协调锁，并在解锁前完成读取或写入。表扩容、解除注册或解锁后不得保留该指针。
 *
 * ## References
 * 直接调用者为本文件 ZrCore_GcDomain_CaptureScopes（:22）和 ZrCore_GcDomain_RestoreScopes（:44）。
 */

/**
 * @fn void ZrCore_GcDomain_CaptureScopes(SZrState *state, SZrGcDomainScopeSnapshot *snapshot)
 *
 * ## Summary
 * 在 state 所属 GC domain 的协调锁下复制 TryRun 入口的 scope 状态，供异常展开后恢复调用者原有深度。
 *
 * ## Parameters
 * @param[in] state 当前 state；允许为空或尚未附着 domain，此时输出保持清零。
 * @param[out] snapshot 必须指向可写快照；函数先将其清零，再在找到已登记 mutator 时填入身份和 scope 字段。
 *
 * ## Returns
 * 无返回值；快照通过 snapshot 交付，未找到记录时是全零值。
 *
 * ## Constraints and lifecycle
 * snapshot 不可为空。快照保存 domain 指针但不 retain domain，也不保存表内记录指针；它只可在 domain 仍存活且对应注册关系有效的同步 TryRun 生命周期内使用。只捕获 mutatorId、domain identity、三个深度、native mode 和 nativeEnteredFromInactive，不捕获整个记录或所有 epoch 数据。 Capture 与 Restore 必须在使用该 state 的同一宿主线程及同步 TryRun 栈内配对；实现不检查线程 ID。恢复可能释放 MutationBegin 获得的递归原生锁，不能跨线程代为解锁；coordination lock 只保护表访问，不保活 domain 或替调用方序列化 state 的执行/注册生命周期（gc_domain_mutator.c:293–313/:354–396）。
 *
 * ## References
 * 生产调用者是 zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:45；实现通过 gc_scopes_find_locked 在本文件 :22 查找记录。快照类型定义见 zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h:57-66。
 */

/**
 * @fn void ZrCore_GcDomain_RestoreScopes(SZrState *state, const SZrGcDomainScopeSnapshot *snapshot)
 *
 * ## Summary
 * 在本地 TryRun 异常展开时撤销回调比入口快照多持有的 mutation 层，并恢复匹配 mutator 的 scope 深度及其派生状态。
 *
 * ## Parameters
 * @param[in] state 当前 state；空 state、未附着 domain 或 domain 不匹配时不执行恢复。
 * @param[in] snapshot 捕获时的借用快照；空快照、失效身份或不兼容深度会被静默忽略。
 *
 * ## Returns
 * 无返回值；成功时更新 mutator 记录、发布当前 safepoint epoch 并广播协调条件；校验失败时保持现状并返回。
 *
 * ## Constraints and lifecycle
 * 快照的 domain 必须仍存活。函数在锁内重新查记录，并要求 state、mutatorId、domain identity 匹配且当前 mutationDepth 不小于快照深度；不能恢复已由其他路径释放的外层 mutation 层。它只撤销额外递归层，不代替正常返回时回调配对调用 Leave/End。 Capture 与 Restore 必须在使用该 state 的同一宿主线程及同步 TryRun 栈内配对；实现不检查线程 ID。恢复可能释放 MutationBegin 获得的递归原生锁，不能跨线程代为解锁；coordination lock 只保护表访问，不保活 domain 或替调用方序列化 state 的执行/注册生命周期（gc_domain_mutator.c:293–313/:354–396）。
 *
 * ## References
 * 本地 TryRun 恢复入口是 zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:26；异常派发经 zr_vm_core/src/zr_vm_core/exception/exception.c:566 的本地恢复 helper 到达该入口。记录由 gc_scopes_find_locked（本文件 :44）重取。
 */

/**
 * @brief 恢复额外递归 mutation 层并发布恢复后的 mutator 状态。
 *
 * ## Summary
 * RestoreScopes 先只释放当前回调额外取得的 mutation-lock 层，再在协调锁仍持有时恢复记录字段、重算 status、更新 observedEpoch 并广播，避免其他 mutator 观察到只恢复了一半的状态。
 *
 * ## Constraints and lifecycle
 * 保持 mutationDepth 与递归锁释放次数一致；释放次数由当前深度减去入口快照深度决定。此顺序依赖调用前的深度校验及域锁保护。mutationDepth 小于快照深度时函数在进入本块前拒绝恢复。
 *
 * ## References
 * 实现位于 ZrCore_GcDomain_RestoreScopes 本函数内 :54-77；锁操作由 ZrCore_GcDomain_MutationUnlock 配套完成，状态发布使用 ZrCore_GcDomain_Broadcast。
 */