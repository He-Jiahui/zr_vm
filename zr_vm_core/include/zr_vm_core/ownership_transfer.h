#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_H

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

struct SZrState;

/**
 * @brief 承载源域与目标域之间一次移交事务的不透明句柄。
 *
 * 调用方只在 Prepare、Publish、Claim 与 Commit/Abort 之间传递此指针，不读取或复制
 * 私有内容；状态观测使用 GetSnapshot。Free 是独立的终结操作，调用前须建立外部静止点。
 */
typedef struct SZrOwnershipTransferEnvelope SZrOwnershipTransferEnvelope;

/**
 * @brief 描述移交事务的线性生命周期阶段。
 *
 * 正常路径为 PREPARED -> QUEUED -> CLAIMED -> COMMITTED；Abort 可将非终态转为
 * ABORTED。COMMITTED 与 ABORTED 是终态，Free 只销毁这两种状态的信封。
 */
typedef enum EZrOwnershipTransferState {
    ZR_OWNERSHIP_TRANSFER_STATE_PREPARED = 0,
    ZR_OWNERSHIP_TRANSFER_STATE_QUEUED,
    ZR_OWNERSHIP_TRANSFER_STATE_CLAIMED,
    ZR_OWNERSHIP_TRANSFER_STATE_COMMITTED,
    ZR_OWNERSHIP_TRANSFER_STATE_ABORTED
} EZrOwnershipTransferState;

/**
 * @brief 跨域移交 API 使用的稳定结果类别。
 *
 * 操作失败时，非空 diagnostic 会给出对应状态。provider 返回的状态会归一到 provider
 * 失败类别，少数明确保留的分配与解码错误除外。
 */
typedef enum EZrDomainTransferStatus {
    ZR_DOMAIN_TRANSFER_STATUS_OK = 0,
    ZR_DOMAIN_TRANSFER_STATUS_INVALID_ARGUMENT,
    ZR_DOMAIN_TRANSFER_STATUS_FORBIDDEN,
    ZR_DOMAIN_TRANSFER_STATUS_DOMAIN_MISMATCH,
    ZR_DOMAIN_TRANSFER_STATUS_SOURCE_GC_EDGE,
    ZR_DOMAIN_TRANSFER_STATUS_UNSUPPORTED_VALUE,
    ZR_DOMAIN_TRANSFER_STATUS_OBJECT_QUOTA,
    ZR_DOMAIN_TRANSFER_STATUS_BYTE_QUOTA,
    ZR_DOMAIN_TRANSFER_STATUS_DEPTH_QUOTA,
    ZR_DOMAIN_TRANSFER_STATUS_ALLOCATION_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_DECODE_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_PREPARE_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_PROVIDER_COMMIT_FAILED,
    ZR_DOMAIN_TRANSFER_STATUS_STALE_GENERATION,
    ZR_DOMAIN_TRANSFER_STATUS_STATE_CONFLICT
} EZrDomainTransferStatus;

/** @brief 要求失败时丢弃暂存载荷；当前 contract 必须设置此位。 */
#define ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE ((TZrUInt32)1u << 0u)
/** @brief 当前 contract 校验器接受的 flag 位集合。 */
#define ZR_DOMAIN_TRANSFER_FLAG_KNOWN_MASK \
    ZR_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE

/**
 * @brief 结构化跨域图编码阶段使用的资源上限。
 *
 * STRUCTURED_CLONE contract 的三个上限都必须非零；它们约束源端遍历和序列化表示，
 * 不限制目标域后续的 GC 对象分配成本。
 */
typedef struct SZrDomainTransferQuota {
    TZrUInt32 maxObjects;
    TZrUInt64 maxBytes;
    TZrUInt32 maxDepth;
} SZrDomainTransferQuota;

/**
 * @brief 由 provider 管理、并随信封复制的移交令牌。
 *
 * core 不解释这些 word，只在提交或撤销时把令牌交回 provider。回调应将其作为同一
 * 事务的状态，不得递归终结对应信封。
 */
typedef struct SZrDomainTransferProviderToken {
    TZrUInt64 words[4];
} SZrDomainTransferProviderToken;

/**
 * @brief 在源域暂存 provider 管理的资源或 immutable handle。
 *
 * PrepareCrossDomain 同步调用此回调。回调应把后续提交或撤销所需状态写入 token，且
 * 不提前发布目标值；失败时 core 会在 Prepare 返回前同步调用 abort。成功返回信封后，
 * 调用方可重试失败的 Commit，或最终撤销；成功提交/撤销后再 Free。Commit 失败后 core
 * 会保存回调更新的 token，后续 Abort 收到该最新值。
 *
 * @pre sourceState/source 在回调期间有效，outToken 可写。
 * @return token 已初始化为可供后续提交（含失败重试）或最终撤销使用时返回 OK。
 * @warning RESOURCE_MOVE 的成功 prepare 必须消费 source；若返回 OK 时 source->type 仍非 null，
 *          Prepare 会立即调用 abort 并释放 source。
 * TODO: 回调非局部异常退出时由注册方禁止还是由 core 恢复，尚待确认。
 */
typedef EZrDomainTransferStatus (*FZrDomainTransferProviderPrepare)(
        struct SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source,
        SZrDomainTransferProviderToken *outToken,
        TZrPtr userData);
/**
 * @brief 在已认领的目标域中把 provider token 物化为目标值。
 *
 * core 在不持有信封状态锁时同步调用此回调。失败时 core 会对非空 target 调用
 * ReleaseValue；成功时 target 必须为非空值，RESOURCE_MOVE 还必须是直接 unique 值。
 *
 * @pre core 已验证当前目标域中的认领上下文；回调参数 targetState/token 有效，target
 *      指向可写的 null 初始化值。
 * @return 失败时 target 必须为空或为可由 ReleaseValue 释放的有效值。core 会把回调更新的
 *         token 写回信封，后续 Abort 将收到该值。
 * TODO: 非局部异常退出会跳过 core 的窗口清理；需确认此退出是否禁止或受支持。
 */
typedef EZrDomainTransferStatus (*FZrDomainTransferProviderCommit)(
        struct SZrState *targetState,
        SZrDomainTransferProviderToken *token,
        SZrTypeValue *target,
        TZrPtr userData);
/**
 * @brief 在准备失败或事务撤销时释放 provider 侧暂存状态。
 *
 * 回调同步执行，可能在 ABORTED 已发布后才调用。准备失败时它会在 Prepare 返回前执行；
 * 成功信封的回调可能延迟到 Abort 或 Free。Free 若因权限/状态不符未释放信封，调用方
 * 须保留 userData 直到重试并成功返回。
 * TODO: 回调非局部异常退出时资源回收与事务终结如何恢复，尚待确认。
 */
typedef void (*FZrDomainTransferProviderAbort)(
        SZrDomainTransferProviderToken *token,
        TZrPtr userData);

/**
 * @brief provider-backed envelope 使用的回调集合与协议身份。
 *
 * contract 的 providerToken/providerContractHash 必须与描述符一致，三个回调均不能为空。
 * PrepareCrossDomain 按值复制描述符，但 userData 仍是借用指针；其有效期须覆盖准备
 * 失败时同步 abort，以及成功信封直到成功 Free 的过程。
 */
typedef struct SZrDomainTransferProvider {
    TZrUInt32 providerToken;
    TZrUInt64 providerContractHash;
    FZrDomainTransferProviderPrepare prepare;
    FZrDomainTransferProviderCommit commit;
    FZrDomainTransferProviderAbort abort;
    TZrPtr userData;
} SZrDomainTransferProvider;

/**
 * @brief 一次跨域移交使用的版本化策略与编解码契约。
 *
 * VALUE_COPY 接受普通标量；STRUCTURED_CLONE 还要求所有 quota 非零；IMMUTABLE_HANDLE
 * 与 RESOURCE_MOVE 必须提供身份匹配的 provider。当前 contract 必须设置 DROP_ON_FAILURE。
 * 准备时复制 provider 描述符，userData 仍由注册方持有。
 *
 * @pre 非空 contract 指向调用期间可读的描述符。
 * @return contract 为空，或 schema、kind、provider、quota 不匹配时，准备 API 返回空信封；
 *         仅 diagnostic 非空时报告失败状态。
 * @warning RESOURCE_MOVE 的部分准备失败路径会消费/释放 source；失败后不可假定仍持有
 *          原 unique 值。
 */
typedef struct SZrDomainTransferContract {
    EZrDomainTransferKind kind;
    TZrUInt32 schemaVersion;
    TZrUInt64 schemaHash;
    TZrUInt32 flags;
    TZrUInt32 providerToken;
    TZrUInt64 providerContractHash;
    SZrDomainTransferQuota quota;
    const SZrDomainTransferProvider *provider;
} SZrDomainTransferContract;

/**
 * @brief 单次跨域操作的状态与遍历计数。
 *
 * 相关 API 仅在指针非空时写入诊断。计数描述传输路径报告的工作量；操作失败时不能
 * 据此推断目标图已提交。
 */
typedef struct SZrDomainTransferDiagnostic {
    EZrDomainTransferStatus status;
    TZrUInt32 objectCount;
    TZrUInt64 byteCount;
    TZrUInt32 depth;
} SZrDomainTransferDiagnostic;

/**
 * @brief 在一次锁保护的读取中取得信封标量快照。
 *
 * GetSnapshot 复制身份、状态、计数和布尔观测值，不延长信封或源/目标值的生命期；如
 * 需在 Free 后报告结果，应提前缓存快照。
 * TODO: hasSourceGcEdge 当前没有写入点，测试仅断言其为 false；核实字段语义和生产用途。
 */
typedef struct SZrOwnershipTransferSnapshot {
    EZrOwnershipTransferState state;
    EZrDomainTransferKind kind;
    SZrGcDomainIdentity sourceDomain;
    SZrGcDomainIdentity targetDomain;
    TZrUInt64 transferId;
    TZrUInt64 generation;
    TZrUInt64 claimantWorkerId;
    TZrUInt64 claimEpoch;
    TZrUInt32 serializedObjectCount;
    TZrUInt64 serializedByteCount;
    TZrBool hasPayload;
    TZrBool hasSourceGcEdge;
} SZrOwnershipTransferSnapshot;

/**
 * @brief 为同一 GC domain 的 worker 移交准备一个直接 unique 值。
 *
 * source 会移入新的 PREPARED 信封。调用方随后发布信封，让 worker 认领，再提交到同域
 * 目标槽或撤销；事务终结后仍须调用 Free。
 *
 * @pre 非空 source 指向调用期间有效且可写的值存储。
 * @return state、state->global 或 source 为空，targetDomain 不是 state 的当前身份、source
 *         不符合直接 unique 条件或分配失败时返回 null。
 */
ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareSameDomain(
        struct SZrState *state,
        SZrGcDomainIdentity targetDomain,
        SZrTypeValue *source);

/**
 * @brief 为另一 GC domain 准备标量、对象图或 provider 管理的载荷。
 *
 * 此调用复制策略和 provider 描述符，并先准备所选载荷表示，之后才可发布。目标域须以
 * 当前身份认领，再调用对应跨域提交 API；失败可通过 AbortCrossDomain 取消。
 *
 * @pre 非空 sourceState/source/contract 在同步准备期间有效；contract 指向可读描述符。
 * @return sourceState/source/contract 为空，或域身份、contract、载荷、provider 不符合所选
 *         kind 时返回 null；仅 diagnostic 非空时写入失败原因。
 * @warning RESOURCE_MOVE 的部分准备失败路径会消费或释放 source。
 */
ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareCrossDomain(
        struct SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        const SZrDomainTransferContract *contract,
        SZrTypeValue *source,
        SZrDomainTransferDiagnostic *diagnostic);
/**
 * @brief 将布局描述的值字节快照到跨域 VALUE_COPY 信封。
 *
 * 这是 PrepareCrossDomain 的字节存储路径：校验 VALUE_COPY 布局后将 sourceStorage 复制
 * 到信封自有内存，之后沿用相同的 publish/claim 生命周期。
 *
 * @pre sourceStorage 在调用期间保持可读，且至少覆盖 layout->byteSize 字节。
 * @return contract/layout/schema 校验或分配失败时返回 null；非空 diagnostic 记录原因。
 * @warning 此 API 不接管 sourceStorage。
 * BUG: BITWISE 父布局包裹 MOVE_ONLY 子布局时，浅层 raw-copy 校验可能越过子布局的
 *      所有权语义；证据与调用链见实现 coverage 记录。
 */
ZR_CORE_API SZrOwnershipTransferEnvelope *
ZrCore_OwnershipTransfer_PrepareCrossDomainValueCopy(
        struct SZrState *sourceState,
        SZrGcDomainIdentity targetDomain,
        const SZrDomainTransferContract *contract,
        const SZrTypeLayout *layout,
        const void *sourceStorage,
        SZrDomainTransferDiagnostic *diagnostic);

/**
 * @brief 将已准备信封推进到可认领阶段。
 *
 * Publish 执行 PREPARED -> QUEUED，但不唤醒 worker，也不把指针放入传输队列；调用方须
 * 在成功后用队列自身的同步机制发布信封指针。
 *
 * @return envelope 为空，或载荷未准备/信封不再为 PREPARED 时返回 false；满足迁移条件时
 *         返回 true。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Publish(
        SZrOwnershipTransferEnvelope *envelope);
/**
 * @brief 在目标域把队列中的信封认领给一个 worker 与 epoch。
 *
 * 成功后记录精确的 worker/epoch；后续提交或已认领状态的撤销都须提供同一组合。这是
 * Publish 后目标侧取得事务权限的门槛。
 *
 * @return envelope/targetState 为空、目标域代数已过期、worker/epoch 为零或状态非 QUEUED
 *         时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Claim(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch);
/**
 * @brief 将已认领的同域载荷交付到目标值槽。
 *
 * 只有原 claimant worker/epoch 才能消费非跨域信封中的载荷。跨域信封应使用
 * CommitCrossDomain 或 CommitCrossDomainValueCopy。
 *
 * @pre 非空 target 指向可写存储。
 * @return envelope/targetState/target 为空，或信封类型、域身份、认领凭证、状态不匹配时
 *         返回 false；成功时进入 COMMITTED。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Commit(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target);
/**
 * @brief 在目标域解码图载荷，或调用 provider 交付已认领载荷。
 *
 * 按准备时的 kind 选择标量解码、图重建或 provider commit。成功后消费信封载荷并发布
 * COMMITTED；普通解码或 provider 失败则保留载荷供撤销。
 *
 * @pre 非空 target 指向可写存储且 *target 为 null 初始化值。
 * @return envelope/targetState/target 为空，或域代数、kind、认领凭证、状态及解码/provider
 *         校验失败时返回 false；仅 diagnostic 非空时写入失败原因。
 * BUG: 图重建经 Object_NewCustomized、RawObject_New、GcMalloc 分配失败可 Throw 并非局部
 *       退出，跳过 commitInProgress 复位；Abort 门禁拒绝后，Free 的非终态保护会保留信封。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_CommitCrossDomain(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);
/**
 * @brief 将已认领的布局字节快照写入目标存储。
 *
 * 验证目标布局/schema、目标域和认领凭证后，才复制信封拥有的字节。它与交付
 * SZrTypeValue 载荷的 CommitCrossDomain 分开。
 *
 * @pre envelope、targetLayout、targetStorage 指针有效；targetStorage 可写并覆盖
 *      targetLayout->byteSize 字节。
 * @return 域、布局/schema、认领凭证或状态校验失败时返回 false；非空 diagnostic 写入原因。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_CommitCrossDomainValueCopy(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        const SZrTypeLayout *targetLayout,
        void *targetStorage,
        SZrDomainTransferDiagnostic *diagnostic);
/**
 * @brief 撤销未认领或由原 claimant 精确认领的同域移交。
 *
 * 认领前传 workerId=0、claimEpoch=0；认领后只能由原 worker/epoch 撤销。跨域信封转交
 * AbortCrossDomain 按跨域权限处理。
 *
 * @return envelope/targetState 为空，或域身份、认领凭证、状态不匹配时返回 false；成功时
 *         丢弃载荷并进入 ABORTED。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_Abort(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch);
/**
 * @brief 从获授权的一端撤销跨域移交。
 *
 * 认领前源域可用零 worker/epoch 撤销；认领后，两端都须提供精确 claimant 凭证。目标域
 * 销毁后源域仍保有清理权；commitInProgress 使撤销与提交线性化。
 *
 * @return 域权限、凭证或状态不匹配时返回 false；成功清理暂存资源后进入 ABORTED。
 *         diagnostic 非空时写入结果。
 */
ZR_CORE_API TZrBool ZrCore_OwnershipTransfer_AbortCrossDomain(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *state,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrDomainTransferDiagnostic *diagnostic);
/**
 * @brief 读取锁保护的一致快照，供诊断或包装层缓存。
 *
 * outSnapshot 为空时不操作；envelope 为空时将输出清零。快照只是独立标量数据，不能
 * 作为信封或载荷的生命期句柄。
 */
ZR_CORE_API void ZrCore_OwnershipTransfer_GetSnapshot(
        SZrOwnershipTransferEnvelope *envelope,
        SZrOwnershipTransferSnapshot *outSnapshot);
/**
 * @brief 在移交事务终结后销毁信封。
 *
 * 必要时先尝试获授权的撤销，只销毁 COMMITTED 或 ABORTED 信封。包装层须先缓存所需
 * 快照，并在调用前建立外部静止点。
 *
 * @pre 对非空参数，调用方保证没有其他线程持有或并发访问 envelope。
 * @note state 或 envelope 为空时不执行任何操作；非空参数仍须在其有效期内调用。
 * @note 权限、状态或撤销检查未通过时信封仍留存，调用方须保留句柄以便重试。
 * TODO: source GlobalState 负责释放信封与 VALUE_COPY 字节；尚未明确谁保证它存活到
 *       唯一一次成功 Free。
 */
ZR_CORE_API void ZrCore_OwnershipTransfer_Free(
        struct SZrState *state,
        SZrOwnershipTransferEnvelope *envelope);

#endif
