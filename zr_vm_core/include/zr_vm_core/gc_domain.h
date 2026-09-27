#ifndef ZR_VM_CORE_GC_DOMAIN_H
#define ZR_VM_CORE_GC_DOMAIN_H

#include "zr_vm_core/conf.h"

struct SZrRawObject;
struct SZrCallInfo;
struct SZrState;
struct SZrTypeValue;

/** @brief 用域编号和代数共同标识一次有效的 GC 域生命周期；零值表示无域。 */
typedef struct SZrGcDomainIdentity {
    TZrUInt64 id;
    TZrUInt32 generation;
} SZrGcDomainIdentity;

/** @brief 区分同域共享入口的参数、证明、生命周期和根注册失败。 */
typedef enum EZrGcDomainShareFailure {
    ZR_GC_DOMAIN_SHARE_FAILURE_NONE = 0,
    ZR_GC_DOMAIN_SHARE_FAILURE_INVALID_ARGUMENT,
    ZR_GC_DOMAIN_SHARE_FAILURE_FOREIGN_DOMAIN,
    ZR_GC_DOMAIN_SHARE_FAILURE_SEND_PROOF_REQUIRED,
    ZR_GC_DOMAIN_SHARE_FAILURE_SYNC_PROOF_REQUIRED,
    ZR_GC_DOMAIN_SHARE_FAILURE_LIFETIME,
    ZR_GC_DOMAIN_SHARE_FAILURE_ROOT_REGISTRATION
} EZrGcDomainShareFailure;

/**
 * @brief 请求在同一 GC 域的两个 state 之间建立可长期持有的对象引用。
 * @note target 只借用；sharedReference 选择 Sync 证明，否则要求 Send 证明。
 * TODO: proof 目前仅为调用方布尔断言，需在宿主接入时确认可信证明来源和 Send 的交接语义。
 */
typedef struct SZrGcDomainShareRequest {
    struct SZrState *producerState;
    struct SZrState *consumerState;
    struct SZrRawObject *target;
    TZrBool sendProof;
    TZrBool syncProof;
    TZrBool sharedReference;
} SZrGcDomainShareRequest;

/** @brief 共享失败的可选诊断；target 是借用指针，不能凭诊断延长其存活期。 */
typedef struct SZrGcDomainShareDiagnostic {
    EZrGcDomainShareFailure failure;
    SZrGcDomainIdentity producerDomain;
    SZrGcDomainIdentity consumerDomain;
    const struct SZrRawObject *target;
} SZrGcDomainShareDiagnostic;

/**
 * @brief 指向域内根表槽位的代际句柄；复制句柄必须通过 Clone 增加保留计数。
 * @note 同槽的多个句柄共享根；Update 会在共享时分离槽位，Release 只释放自己的保留份额。
 */
typedef struct SZrGcRootHandle {
    SZrGcDomainIdentity domain;
    TZrUInt32 slotIndex;
    TZrUInt32 slotGeneration;
} SZrGcRootHandle;

/**
 * @brief 声明原生调用与 GC 安全点的协作方式。
 * @note GC_AWARE 可停靠；BLOCKING_DETACHED 不阻塞暂停；NO_SAFEPOINT_CRITICAL 会阻塞至离开。
 */
typedef enum EZrGcNativeSafepointMode {
    ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE = 0,
    ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED = 1,
    ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL = 2
} EZrGcNativeSafepointMode;

/**
 * @brief 报告暂停超时或调用者自身处于不可暂停原生区时的阻塞位置。
 * @note blockingState 和 blockingNativeFrame 是借用指针；state 及对应调用帧仍存活时才可解引用。
 */
typedef struct SZrGcDomainPauseDiagnostic {
    TZrBool timedOut;
    TZrUInt64 safepointEpoch;
    TZrUInt64 blockingMutatorId;
    EZrGcNativeSafepointMode blockingNativeMode;
    const struct SZrState *blockingState;
    const struct SZrCallInfo *blockingNativeFrame;
} SZrGcDomainPauseDiagnostic;

/** @brief 域内 mutator 状态快照；registered 计入未运行者，其余状态计数按当前运行模式划分。 */
typedef struct SZrGcDomainMutatorSnapshot {
    TZrUInt64 safepointEpoch;
    TZrUInt32 registeredMutatorCount;
    TZrUInt32 runningMutatorCount;
    TZrUInt32 parkedMutatorCount;
    TZrUInt32 blockingDetachedMutatorCount;
    TZrUInt32 noSafepointCriticalMutatorCount;
    TZrBool pauseRequested;
} SZrGcDomainMutatorSnapshot;

/** @brief 取得当前活动域的身份；无活动域时返回零身份。 */
ZR_CORE_API SZrGcDomainIdentity ZrCore_GcDomain_GetIdentity(
        const struct SZrState *state);
/** @brief 只比较域编号和代数的值；是否仍活动须另用 IdentityIsCurrent 检查。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_IdentityEquals(
        SZrGcDomainIdentity left,
        SZrGcDomainIdentity right);
/** @brief 检查身份是否仍匹配 state 所附着的活动域。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_IdentityIsCurrent(
        const struct SZrState *state,
        SZrGcDomainIdentity identity);
/**
 * @brief 读取对象上记录的域身份，而不判断该域是否仍有效。
 * @return 对象未标记或参数无效时为 false，并在提供输出槽时将其清零。
 */
ZR_CORE_API TZrBool ZrCore_GcDomain_GetObjectIdentity(
        const struct SZrRawObject *object,
        SZrGcDomainIdentity *outIdentity);
/** @brief 判断对象的域标记是否与 state 的当前活动域一致。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_ObjectBelongsToState(
        const struct SZrState *state,
        const struct SZrRawObject *object);
/** @brief 写入对象边前检查 owner 属于当前域，且非空 target 也属于同域。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_ValidateWrite(
        const struct SZrState *state,
        const struct SZrRawObject *owner,
        const struct SZrRawObject *target);
/** @brief 写入值边前检查 owner；非 GC 值与空值无需目标域检查。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_ValidateValueWrite(
        const struct SZrState *state,
        const struct SZrRawObject *owner,
        const struct SZrTypeValue *value);

/** @brief 进入执行作用域并登记运行中的 mutator；嵌套进入须逐层 Leave。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_MutatorEnter(struct SZrState *state);
/** @brief 离开一层执行作用域；执行与原生深度都归零后转为已附着但未运行。 */
ZR_CORE_API void ZrCore_GcDomain_MutatorLeave(struct SZrState *state);
/** @brief 异常展开时清空执行及原生作用域深度，使域不再等待该 mutator。 */
ZR_CORE_API void ZrCore_GcDomain_MutatorUnwindScopes(
        struct SZrState *state);
/** @brief 执行安全点轮询；暂停请求存在时停靠并等到恢复，实际停靠后返回 true。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_MutatorPoll(struct SZrState *state);
/**
 * @brief 将未附着且共享 ownerState 全局状态的 mutatorState 加入同一域。
 * @return 登记或分配失败时为 false；成功后由调用方负责 Detach。
 */
ZR_CORE_API TZrBool ZrCore_GcDomain_MutatorAttach(
        struct SZrState *ownerState,
        struct SZrState *mutatorState);
/** @brief 将已结束执行的 state 从域登记表移除；调用前须先退出相关作用域。 */
ZR_CORE_API void ZrCore_GcDomain_MutatorDetach(
        struct SZrState *mutatorState);
/** @brief 取得已登记 mutator 的域内编号；不存在时返回零。 */
ZR_CORE_API TZrUInt64 ZrCore_GcDomain_GetMutatorId(
        const struct SZrState *state);
/**
 * @brief 进入原生调用区并登记安全点模式；可能等待域暂停结束或因预算失败。
 * @return 成功进入后调用方必须配对 NativeLeave；嵌套进入需使用同一模式。
 */
ZR_CORE_API TZrBool ZrCore_GcDomain_NativeEnter(
        struct SZrState *state,
        EZrGcNativeSafepointMode mode);
/** @brief 离开一层原生调用区；最后一层离开后可能立即处理待决暂停。 */
ZR_CORE_API void ZrCore_GcDomain_NativeLeave(struct SZrState *state);
/**
 * @brief 请求域内协作暂停，等待其余运行中 mutator 停靠或超时。
 * @return 成功时须由同一 state 配对 End；该 state 可嵌套 Begin。失败时可查可选诊断。
 */
ZR_CORE_API TZrBool ZrCore_GcDomain_StopTheWorldBegin(
        struct SZrState *state,
        TZrUInt32 timeoutMilliseconds,
        SZrGcDomainPauseDiagnostic *outDiagnostic);
/** @brief 结束同一 state 持有的一层暂停；仅最外层 End 唤醒其他 mutator。 */
ZR_CORE_API void ZrCore_GcDomain_StopTheWorldEnd(struct SZrState *state);
/** @brief 在域锁下复制 mutator 计数和暂停代数；无域时输出全零快照。 */
ZR_CORE_API void ZrCore_GcDomain_GetMutatorSnapshot(
        const struct SZrState *state,
        SZrGcDomainMutatorSnapshot *outSnapshot);
/** @brief 通知域条件变量上的等待者重新检查状态；本身不解除暂停。 */
ZR_CORE_API void ZrCore_GcDomain_WakeMutators(struct SZrState *state);

/**
 * @brief 为本域对象创建独立 GC 根，并将根的释放责任交给输出句柄持有者。
 * @pre outHandle 尚未持有根；入口会覆盖该槽，复用前应先 Release。
 * @return 失败且输出槽非空时将其清为无效，不接管 target。
 */
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Create(
        struct SZrState *state,
        struct SZrRawObject *target,
        SZrGcRootHandle *outHandle);
/**
 * @brief 增加 source 根槽的保留计数并产生可独立 Release 的句柄。
 * @pre outHandle 是未持有根且不同于 source 的输出槽。
 */
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Clone(
        struct SZrState *state,
        const SZrGcRootHandle *source,
        SZrGcRootHandle *outHandle);
/**
 * @brief 改换句柄目标；共享根槽时先分配新槽，不改变其他 clone 的目标。
 * @return 非本域目标或分配失败时为 false，原句柄仍指向旧目标。
 */
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Update(
        struct SZrState *state,
        SZrGcRootHandle *handle,
        struct SZrRawObject *target);
/**
 * @brief 校验域、槽位及代数后返回当前根对象的借用指针。
 * @note 输出指针不延长对象寿命，也不能在可能搬迁对象的 GC 后继续缓存。
 */
ZR_CORE_API TZrBool ZrCore_GcRootHandle_Resolve(
        const struct SZrState *state,
        const SZrGcRootHandle *handle,
        struct SZrRawObject **outTarget);
/** @brief 释放本句柄的根保留份额并清空句柄；须用所属域的 state。 */
ZR_CORE_API void ZrCore_GcRootHandle_Release(
        struct SZrState *state,
        SZrGcRootHandle *handle);

/** @brief 查询活动根槽数；多个 clone 共用同一槽时只计一次。 */
ZR_CORE_API TZrSize ZrCore_GcDomain_GetRootCount(
        const struct SZrState *state);
/** @brief 查询由 unique/resource 所有权系统登记的专用根槽数。 */
ZR_CORE_API TZrSize ZrCore_GcDomain_GetOwnershipRootCount(
        const struct SZrState *state);
/** @brief 判断对象是否仍占有所属域的有效所有权根槽。 */
ZR_CORE_API TZrBool ZrCore_GcDomain_IsOwnershipRoot(
        const struct SZrState *state,
        const struct SZrRawObject *object);
/**
 * @brief 在同一域内通过 consumer 根句柄共享 producer 对象；不移动对象所有权。
 * @pre outHandle 未持有根，且 request 的证明标志已由可信调用方验证。
 * @return 失败时可选诊断指出拒绝原因，非空输出槽被清空；成功后调用方负责 Release。
 * TODO: 仓内尚无公开入口调用者，需在宿主接入时核实 proof 来源和句柄复用约束。
 */
ZR_CORE_API TZrBool ZrCore_GcDomain_ShareValue(
        const SZrGcDomainShareRequest *request,
        SZrGcDomainShareDiagnostic *diagnostic,
        SZrGcRootHandle *outHandle);

/** @brief 同域共享入口的兼容名称；展开为 ZrCore_GcDomain_ShareValue。 */
#define ZrCore_Domain_ShareValue ZrCore_GcDomain_ShareValue

#endif
