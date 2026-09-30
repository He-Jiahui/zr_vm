#ifndef ZR_VM_CORE_GC_DOMAIN_INTERNAL_H
#define ZR_VM_CORE_GC_DOMAIN_INTERNAL_H

#include "zr_vm_core/gc_domain.h"

#if defined(ZR_PLATFORM_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

struct SZrGarbageCollector;
struct SZrGlobalState;
struct SZrRawObject;
struct SZrState;

/* 根槽种类决定句柄释放与所有权根移除路径；FREE 槽仍以 generation 拒绝旧句柄。 */
typedef enum EZrGcDomainRootKind {
    ZR_GC_DOMAIN_ROOT_KIND_FREE = 0,
    ZR_GC_DOMAIN_ROOT_KIND_HANDLE = 1,
    ZR_GC_DOMAIN_ROOT_KIND_OWNERSHIP = 2
} EZrGcDomainRootKind;

/* roots 表由域持有；target 只在槽有效且所属域仍活动时供 GC 保活扫描借用。 */
typedef struct SZrGcDomainRootSlot {
    struct SZrRawObject *target;
    TZrUInt32 generation;
    TZrUInt32 retainCount;
    EZrGcDomainRootKind kind;
} SZrGcDomainRootSlot;

/* 暂停握手按状态判定阻塞者：inactive 和 detached 不阻塞，critical 必须等待退出。 */
typedef enum EZrGcDomainMutatorStatus {
    ZR_GC_DOMAIN_MUTATOR_STATUS_ATTACHED_INACTIVE = 0,
    ZR_GC_DOMAIN_MUTATOR_STATUS_RUNNING,
    ZR_GC_DOMAIN_MUTATOR_STATUS_PARKED,
    ZR_GC_DOMAIN_MUTATOR_STATUS_BLOCKING_DETACHED,
    ZR_GC_DOMAIN_MUTATOR_STATUS_NO_SAFEPOINT_CRITICAL
} EZrGcDomainMutatorStatus;

/* 每个附着的 state 对应一条记录；深度属于该 state 的执行/原生作用域，epoch 记录最近接受的暂停代数。 */
typedef struct SZrGcDomainMutatorRecord {
    struct SZrState *state;
    TZrUInt64 mutatorId;
    TZrUInt64 observedEpoch;
    TZrUInt32 executionDepth;
    TZrUInt32 nativeDepth;
    /* Protected by coordinationLock; counts locked scopes while this state is registered. */
    TZrUInt32 mutationDepth;
    EZrGcNativeSafepointMode nativeMode;
    EZrGcDomainMutatorStatus status;
    /* TODO: 该字段仅见写入及物化保存/恢复，未见行为判定读取；核查是否仍承担独立契约。 */
    TZrBool nativeEnteredFromInactive;
} SZrGcDomainMutatorRecord;

/* 跨域传输在 prepare/publish/claim/commit/abort 边界分别记账，供两端域统计。 */
typedef enum EZrGcDomainTransferTelemetryEvent {
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PREPARE = 0,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_PUBLISH,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_OUTBOUND_ABORT,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_CLAIM,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_COMMIT,
    ZR_GC_DOMAIN_TRANSFER_TELEMETRY_INBOUND_ABORT
} EZrGcDomainTransferTelemetryEvent;

/* 一个 global 的 GC 域统一持有根表、mutator 表和暂停协议；登记表及暂停字段由 coordinationLock 保护。
 * mutationLock 另用于并发 major 标记与写屏障；active/identity 的创建销毁需外部生命周期排他。 */
typedef struct SZrGcDomain {
    struct SZrGlobalState *global;
    struct SZrGarbageCollector *collector;
    struct SZrState *attachedState;
    SZrGcDomainIdentity identity;
    SZrGcDomainRootSlot *roots;
    TZrSize rootLength;
    TZrSize rootCapacity;
    TZrSize activeRootCount;
    TZrSize ownershipRootCount;
    SZrGcDomainMutatorRecord *mutators;
    TZrSize mutatorLength;
    TZrSize mutatorCapacity;
    TZrUInt64 nextMutatorId;
    TZrUInt64 safepointEpoch;
    struct SZrState *collectorState;
    TZrUInt32 pauseDepth;
    TZrBool pauseRequested;
    TZrUInt64 safepointWaitCount;
    TZrUInt64 safepointWaitTotalUs;
    TZrUInt64 safepointWaitMaxUs;
    TZrUInt64 outboundTransferPrepareCount;
    TZrUInt64 outboundTransferPublishCount;
    TZrUInt64 outboundTransferAbortCount;
    TZrUInt64 outboundTransferObjectCount;
    TZrUInt64 outboundTransferByteCount;
    TZrUInt64 inboundTransferClaimCount;
    TZrUInt64 inboundTransferCommitCount;
    TZrUInt64 inboundTransferAbortCount;
    TZrUInt64 inboundTransferObjectCount;
    TZrUInt64 inboundTransferByteCount;
#if defined(ZR_PLATFORM_WIN)
    CRITICAL_SECTION coordinationLock;
    CRITICAL_SECTION mutationLock;
    CONDITION_VARIABLE coordinationCondition;
#else
    pthread_mutex_t coordinationLock;
    pthread_mutex_t mutationLock;
    pthread_cond_t coordinationCondition;
#endif
    TZrBool coordinationInitialized;
    TZrBool mutationLockInitialized;
    TZrBool active;
} SZrGcDomain;

/* global 创建/销毁域；次 state 只借用域，并须先退出作用域再解除登记。 */
SZrGcDomain *ZrCore_GcDomain_New(
        struct SZrGlobalState *global,
        struct SZrGarbageCollector *collector);
void ZrCore_GcDomain_Free(SZrGcDomain *domain);
void ZrCore_GcDomain_AttachState(SZrGcDomain *domain, struct SZrState *state);
void ZrCore_GcDomain_DetachState(SZrGcDomain *domain, struct SZrState *state);
/* 协调锁保护登记表和暂停状态；等待会临时放开锁，调用方必须重查记录。 */
TZrBool ZrCore_GcDomain_CoordinationInit(SZrGcDomain *domain);
void ZrCore_GcDomain_CoordinationFree(SZrGcDomain *domain);
void ZrCore_GcDomain_Lock(SZrGcDomain *domain);
void ZrCore_GcDomain_Unlock(SZrGcDomain *domain);
void ZrCore_GcDomain_Broadcast(SZrGcDomain *domain);
/* 并发 major 写入与标记共享递归锁，允许屏障在受保护写入中重入。 */
void ZrCore_GcDomain_MutationLock(SZrGcDomain *domain);
void ZrCore_GcDomain_MutationUnlock(SZrGcDomain *domain);
/* ownership transfer 在各阶段向身份仍匹配的两端域写入统计，不拥有传入的对象。 */
void ZrCore_GcDomain_RecordTransferTelemetry(
        struct SZrGlobalState *global,
        SZrGcDomainIdentity identity,
        EZrGcDomainTransferTelemetryEvent event,
        TZrUInt32 objectCount,
        TZrUInt64 byteCount);
/* Begin 返回是否获得 mutationLock，而非请求是否有效；End 必须接收同一令牌。 */
TZrBool ZrCore_GcDomain_MutationBegin(struct SZrState *state);
void ZrCore_GcDomain_MutationEnd(struct SZrState *state, TZrBool locked);
/* 附着时先登记再发布 state->gcDomain；解除时反向撤销且不得仍持有执行作用域。 */
TZrBool ZrCore_GcDomain_RegisterMutator(
        SZrGcDomain *domain,
        struct SZrState *state);
void ZrCore_GcDomain_UnregisterMutator(
        SZrGcDomain *domain,
        struct SZrState *state);
/* 对象戳记与所有权根槽供 GC 扫描和跨域校验共用；根槽归域持有。 */
TZrBool ZrCore_GcDomain_AssignObject(SZrGcDomain *domain, struct SZrRawObject *object);
TZrBool ZrCore_GcDomain_RegisterOwnershipRoot(
        struct SZrState *state,
        struct SZrRawObject *object);
void ZrCore_GcDomain_UnregisterOwnershipRoot(
        struct SZrState *state,
        struct SZrRawObject *object);

#endif
