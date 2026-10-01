#ifndef ZR_VM_CORE_OWNERSHIP_TRANSFER_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_TRANSFER_INTERNAL_H

#include "ownership_transfer_cross_domain_internal.h"
#include "zr_vm_core/global.h"

/**
 * @brief 同域移交与跨 GC 域传输共用的私有事务载体。
 * @details ownerGlobal 负责释放信封及其原生字节；sourceDomain/targetDomain 是带 generation 的身份快照，kind/schema 与 provider 标识描述本次协议。payload、graph、providerPayload 或 valueBytes 按传输种类承载待提交资源，hasPayload 表明当前信封仍持有该资源。state 表示公开生命周期，claimantWorkerId/claimEpoch 只标识当前认领；transitionLock 串行化其余可变字段，state 仍通过 acquire/release 原子访问。
 * @note 目标身份在使用时仍须重新验证；终态释放前须先消费或撤销图、provider token 等关联资源。
 * TODO: 公共 API 尚未明确 ownerGlobal 的保活期限；核实信封从 Prepare 到唯一 terminal Free 期间如何保证分配器仍有效。
 * TODO: hasSourceGcEdge 当前仅被快照并在测试中断言为 false，仓内未找到写入点；核实该字段代表的 GC 边语义及预期生产路径。
 */
struct SZrOwnershipTransferEnvelope {
    /* 信封及 valueBytes 均用此 global 分配；source/target 身份快照带 generation，用于拒绝过期目标。 */
    SZrGlobalState *ownerGlobal;
    SZrGcDomainIdentity sourceDomain;
    SZrGcDomainIdentity targetDomain;
    /* kind 决定 payload 表示；schema 与 provider 身份绑定跨域协议的兼容版本。 */
    EZrDomainTransferKind kind;
    TZrUInt32 schemaVersion;
    TZrUInt64 schemaHash;
    TZrUInt32 providerToken;
    TZrUInt64 providerContractHash;
    /* transferId 是快照可见的信封标识；generation 镜像目标身份代数；认领 worker 与 epoch 成对防止旧认领提交或撤销新一轮。 */
    TZrUInt64 transferId;
    TZrUInt64 generation;
    TZrUInt64 claimantWorkerId;
    TZrUInt64 claimEpoch;
    /* 不同传输种类分别以 TypeValue、provider token、结构图或内联布局字节承载载荷。 */
    SZrTypeValue payload;
    SZrDomainTransferProviderToken providerPayload;
    /* provider 描述符按值快照；其 userData 仍由注册方保活至终态 Free 返回。 */
    SZrDomainTransferProvider provider;
    SZrDomainTransferGraph *graph;
    TZrByte *valueBytes;
    TZrUInt32 valueByteCount;
    /* 提交/撤销及诊断使用的序列化工作量快照。 */
    TZrUInt32 serializedObjectCount;
    TZrUInt64 serializedByteCount;
    /* 发布后普通字段须在 transitionLock 下读写；state 以 acquire/release 单独发布生命周期变化。 */
    volatile TZrInt32 state;
    volatile TZrInt32 transitionLock;
    /* hasPayload 标出信封仍持有待提交载荷；hasProvider 标出已复制 provider；commitInProgress 保留无锁提交窗口并阻止并发终结。 */
    TZrBool hasPayload;
    TZrBool hasProvider;
    TZrBool commitInProgress;
    /* 路径标记；hasSourceGcEdge 当前没有写入点，语义仍待核实。 */
    TZrBool isCrossDomain;
    TZrBool hasSourceGcEdge;
};

/**
 * @brief 为 VALUE_COPY 构造路径分配并初始化信封。
 * @pre state 及 state->global 有效，sourceDomain/targetDomain 已由调用方取得并校验。
 * @return 分配失败返回空；成功时信封由 state->global 管理。
 * @note 调用方负责在 terminal 清理后释放信封，并保证该 global 活到释放完成。
 */
SZrOwnershipTransferEnvelope *ZrCore_OwnershipTransfer_InternalNew(
        struct SZrState *state,
        SZrGcDomainIdentity sourceDomain,
        SZrGcDomainIdentity targetDomain);
/**
 * @brief 释放已完成事务清理的私有信封及其 VALUE_COPY 字节。
 * @pre 辅助 graph/provider 资源已提交或撤销，且没有并发访问者；ownerGlobal 仍有效。
 * @note 不执行 graph 释放或 provider abort；生命周期由公开 Free 路径先完成。
 */
void ZrCore_OwnershipTransfer_InternalDelete(
        SZrOwnershipTransferEnvelope *envelope);
/**
 * @brief 校验跨域契约的协议字段、标志、配额和 provider 组合。
 * @pre contract 在检查期间可读；空指针或不支持的组合返回 false。
 */
TZrBool ZrCore_OwnershipTransfer_InternalContractIsValid(
        const SZrDomainTransferContract *contract);
/**
 * @brief 判断 state 是否仍代表信封记录的目标域身份。
 * @note 这是目标域检查，不授予源域取消权限；域 generation 必须仍匹配。
 */
TZrBool ZrCore_OwnershipTransfer_InternalTargetMatches(
        const struct SZrState *state,
        const SZrOwnershipTransferEnvelope *envelope);
/**
 * @brief 可选地填入一次传输诊断的状态和工作量快照。
 * @note diagnostic 为空时不写入；非空时调用者提供可写存储。
 */
void ZrCore_OwnershipTransfer_InternalDiagnosticSet(
        SZrDomainTransferDiagnostic *diagnostic,
        EZrDomainTransferStatus status,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount,
        TZrUInt32 depth);
/**
 * @brief 独占信封普通元数据，直到配对 Unlock。
 * @pre envelope 有效；锁不可递归，持锁区不得调用会重入同一信封的回调。
 */
void ZrCore_OwnershipTransfer_InternalLock(
        SZrOwnershipTransferEnvelope *envelope);
/**
 * @brief 释放当前线程持有的信封元数据锁并发布之前的字段更新。
 * @pre 当前线程已对同一 envelope 成功 Lock，且只解锁一次。
 */
void ZrCore_OwnershipTransfer_InternalUnlock(
        SZrOwnershipTransferEnvelope *envelope);
/**
 * @brief acquire 读取信封生命周期状态。
 * @pre envelope 有效；与锁内元数据快照配合时调用方须持有 transitionLock。
 */
TZrInt32 ZrCore_OwnershipTransfer_InternalStateLoad(
        const SZrOwnershipTransferEnvelope *envelope);
/**
 * @brief release 发布信封生命周期状态。
 * @pre envelope 有效；可见信封的状态转换须与其普通字段更新在同一锁区内完成。
 */
void ZrCore_OwnershipTransfer_InternalStateStore(
        SZrOwnershipTransferEnvelope *envelope,
        EZrOwnershipTransferState state);
/**
 * @brief 提交已认领的 provider 载荷并将成功结果线性化为终态。
 * @pre 外层跨域提交已验证目标域；workerId/claimEpoch 必须匹配当前 CLAIMED 信封，target 是空的可写槽。
 * @note provider 回调在不持信封锁时运行，返回后重验认领；返回 true 才消费载荷。
 * TODO: provider commit 回调若非局部退出会跳过认领收尾；核实注册契约是否禁止抛出及其异常边界。
 */
TZrBool ZrCore_OwnershipTransfer_InternalCommitProvider(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);
/**
 * @brief 将已认领的结构图重建到目标域并在线性化成功后转移图所有权。
 * @pre 外层跨域提交已验证目标域；workerId/claimEpoch 必须匹配当前 CLAIMED 信封，target 是空的可写槽。
 * @note 失败时保留原图供重试或 Abort；重建期间不持信封锁，完成后重验原图及认领身份。
 * 目标分配 Throw 会先清理临时根并复位 commitInProgress，再向调用方转抛原始状态。
 */
TZrBool ZrCore_OwnershipTransfer_InternalCommitGraph(
        SZrOwnershipTransferEnvelope *envelope,
        struct SZrState *targetState,
        TZrUInt64 workerId,
        TZrUInt64 claimEpoch,
        SZrTypeValue *target,
        SZrDomainTransferDiagnostic *diagnostic);

#endif
