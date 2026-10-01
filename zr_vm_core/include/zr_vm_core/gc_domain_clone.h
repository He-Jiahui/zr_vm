#ifndef ZR_VM_CORE_GC_DOMAIN_CLONE_H
#define ZR_VM_CORE_GC_DOMAIN_CLONE_H

/**
 * @brief 隔离 GC 域之间的结构化复制入口；目标域只接收新建对象，不接收源域托管指针。
 * @note 底层 ownership_transfer 负责图编码与 Prepare/Publish/Claim/Commit 状态机；
 * 本层检查域身份，并要求调用方显式结束失败的事务。
 */

#include "zr_vm_core/ownership_transfer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 持有编码图和双方域身份的事务包装对象。
 * @note 提交或中止后仍可读缓存快照；成功 Free 后包装对象不可再次使用。
 */
typedef struct SZrGcDomainCloneTransaction SZrGcDomainCloneTransaction;

/** @brief 写入结构化复制传输契约的格式版本；当前仅要求非零。 */
#define ZR_GC_DOMAIN_CLONE_SCHEMA_VERSION ((TZrUInt32)1u)
/** @brief 写入传输契约的固定格式指纹；TODO: 图提交路径尚未比较此值，需确认跨版本传输需求。 */
#define ZR_GC_DOMAIN_CLONE_SCHEMA_HASH UINT64_C(0x434c4f4e455f5631)

/**
 * @brief 预检并编码源值图，创建待发布事务；源值不会被移动或消费。
 * @pre 双方 state 属于不同的活动域，quota 的对象数、字节数和深度上限均非零。
 * @return 失败时返回空并写可选诊断；成功事务持有编码图，调用方须最终 Commit/Abort/Free。
 * @note source 与 quota 仅在调用期间借用；事务结束前 sourceState 须保持存活以释放源侧分配。
 */
ZR_CORE_API SZrGcDomainCloneTransaction *ZrCore_GcDomainClone_Prepare(
        struct SZrState *sourceState,
        struct SZrState *targetState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        SZrDomainTransferDiagnostic *diagnostic);

/** @brief 将预备事务发布到可领取状态；失败时事务仍由调用方持有并需清理。 */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Publish(
        SZrGcDomainCloneTransaction *transaction,
        SZrDomainTransferDiagnostic *diagnostic);
/**
 * @brief 由目标域 worker 及非零 epoch 领取已发布事务，供后续 Commit 校验。
 * @return 目标域代数失效或事务状态冲突时为 false；仍须清理未终结事务。
 */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Claim(
        SZrGcDomainCloneTransaction *transaction,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic);

/**
 * @brief 在目标域重建图并提交已领取事务，成功后目标值归目标域调用方管理。
 * @pre target 已初始化为 null 类型；目标域及领取者身份仍有效。
 * @return 成功后编码信封已释放，包装对象保留快照供 GetSnapshot/Free；失败可重试、Abort 或 Free。
 * @note 目标分配 Throw 前会清理临时根并释放提交窗口，再转抛原始状态；目标仍为 null。
 * 在外层 TryRun 捕获并处理当前异常后，可使用原 worker/epoch 重试已领取事务。
 */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Commit(
        SZrGcDomainCloneTransaction *transaction,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

/**
 * @brief 以源域身份中止未终结事务；目标域关闭后仍可取消已领取事务。
 * @return 成功时信封关闭而包装对象仍可查询快照；若提交正在进行或已终结则返回 false。
 */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Abort(
        SZrGcDomainCloneTransaction *transaction,
        SZrDomainTransferDiagnostic *diagnostic);

/**
 * @brief 释放事务包装对象；若事务未终结，先尝试从源域中止并清理信封。
 * @pre 未终结事务仍需有效 sourceState；成功 Commit/Abort 已关闭信封时可在源域销毁后调用。
 * @note 此函数只对空指针容忍重复调用；释放非空包装对象后不可再使用原指针。
 * TODO: 未终结事务的 Abort 若与并发 Commit 冲突，Free 会保留包装对象并返回；
 * 需确认跨线程 Free/Commit 的外部同步和重试责任，当前仓内仅见顺序调用。
 */
ZR_CORE_API void ZrCore_GcDomainClone_Free(
        SZrGcDomainCloneTransaction *transaction);

/** @brief 取得当前信封或终结后缓存的标量快照；提供输出槽时先清零，无效参数返回 false。 */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_GetSnapshot(
        const SZrGcDomainCloneTransaction *transaction,
        SZrOwnershipTransferSnapshot *outSnapshot);

/**
 * @brief 同步完成准备、发布、领取、提交和失败清理，不向调用方暴露事务包装对象。
 * @pre 双方域有效且不同，workerId/claimEpoch 非零，target 已初始化为 null 类型。
 * @return 成功时 target 接收目标域副本；失败时由此入口尝试 Abort/Free，诊断保留失败阶段。
 * @note Commit 的目标分配 Throw 会先关闭内部事务，再向外层恢复点转抛原始状态。
 */
ZR_CORE_API TZrBool ZrCore_GcDomainClone_Execute(
        struct SZrState *sourceState,
        struct SZrState *targetState,
        const SZrTypeValue *source,
        const SZrDomainTransferQuota *quota,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_DOMAIN_CLONE_H */
