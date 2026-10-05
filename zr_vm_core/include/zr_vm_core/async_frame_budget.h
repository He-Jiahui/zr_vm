#ifndef ZR_VM_CORE_ASYNC_FRAME_BUDGET_H
#define ZR_VM_CORE_ASYNC_FRAME_BUDGET_H

/* 与具体 VM state、调度器分离的异步帧状态机。调度器可嵌入这些标量记录，
 * 但须经公开转移函数维护帧、等待槽和后台编译任务；当前仓内入口是独立 SSA 测试。 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 帧、等待请求与编译请求共用的 ABI 版本；Validate/Queue 拒绝不匹配值。 */
#define ZR_ASYNC_FRAME_BUDGET_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_ASYNC_FRAME_BUDGET_MAGIC ((TZrUInt32)0x31464241u) /* ABF1 */

/** @brief 跨调度边界传递的失败分类，stage 与身份细节见 diagnostic。 */
typedef enum EZrAsyncFrameDiagnosticCode {
    ZR_ASYNC_FRAME_DIAGNOSTIC_NONE = 0,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_FLAGS,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_STATE,
    ZR_ASYNC_FRAME_DIAGNOSTIC_BORROW_ACROSS_SUSPEND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_STACK_ALIAS_ACROSS_SUSPEND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_LOCK_GUARD_ACROSS_SUSPEND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_NATIVE_CRITICAL,
    ZR_ASYNC_FRAME_DIAGNOSTIC_STATE_MAP_REQUIRED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_BUDGET_EXHAUSTED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_ACTIVE_PIN,
    ZR_ASYNC_FRAME_DIAGNOSTIC_PIN_UNDERFLOW,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_CAPACITY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_FOUND,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_ALREADY_RESUMED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_WAIT_NOT_READY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CAPACITY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_INVALID_SNAPSHOT,
    ZR_ASYNC_FRAME_DIAGNOSTIC_COMPILE_CANCELLED,
    ZR_ASYNC_FRAME_DIAGNOSTIC_STALE_GENERATION,
    ZR_ASYNC_FRAME_DIAGNOSTIC_CONTRACT_MISMATCH,
    ZR_ASYNC_FRAME_DIAGNOSTIC_OUT_OF_MEMORY,
    ZR_ASYNC_FRAME_DIAGNOSTIC_OVERFLOW,
    ZR_ASYNC_FRAME_DIAGNOSTIC_COUNT
} EZrAsyncFrameDiagnosticCode;

/** @brief 稳定的标量诊断，可跨调度边界复制；不持有源位置或对象指针。 */
typedef struct SZrAsyncFrameDiagnostic {
    EZrAsyncFrameDiagnosticCode code;
    TZrUInt32 stage;
    TZrUInt32 state;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt64 expected;
    TZrUInt64 actual;
    TZrUInt64 generation;
} SZrAsyncFrameDiagnostic;

/** @brief 帧生命周期状态；完成、取消及故障后须经 Teardown 收尾。 */
typedef enum EZrAsyncFrameStatus {
    ZR_ASYNC_FRAME_STATUS_IDLE = 0,
    ZR_ASYNC_FRAME_STATUS_RUNNING,
    ZR_ASYNC_FRAME_STATUS_SUSPENDED,
    ZR_ASYNC_FRAME_STATUS_RESUMING,
    ZR_ASYNC_FRAME_STATUS_COMPLETED,
    ZR_ASYNC_FRAME_STATUS_CANCELLED,
    ZR_ASYNC_FRAME_STATUS_FAULTED,
    ZR_ASYNC_FRAME_STATUS_TORN_DOWN,
    ZR_ASYNC_FRAME_STATUS_COUNT
} EZrAsyncFrameStatus;

/* 调度器适配器沿用的旧拼写，不引入额外状态。 */
#define ZR_ASYNC_FRAME_STATUS_TEARDOWN ZR_ASYNC_FRAME_STATUS_TORN_DOWN

/** @brief 暂停用途；预算暂停和等待/编译暂停共享相同状态门禁。 */
typedef enum EZrAsyncFrameSuspendReason {
    ZR_ASYNC_FRAME_SUSPEND_NONE = 0,
    ZR_ASYNC_FRAME_SUSPEND_WAIT,
    ZR_ASYNC_FRAME_SUSPEND_BUDGET,
    ZR_ASYNC_FRAME_SUSPEND_COMPILE,
    ZR_ASYNC_FRAME_SUSPEND_COUNT
} EZrAsyncFrameSuspendReason;

/** @brief Poll 对调度器返回的状态；仅在安全边界实际取消或暂停。 */
typedef enum EZrAsyncFramePollOutcome {
    ZR_ASYNC_FRAME_POLL_RUNNING = 0,
    ZR_ASYNC_FRAME_POLL_SUSPENDED,
    ZR_ASYNC_FRAME_POLL_CANCELLED,
    ZR_ASYNC_FRAME_POLL_COMPLETED,
    ZR_ASYNC_FRAME_POLL_FAULTED
} EZrAsyncFramePollOutcome;

/* 异步许可必须显式声明；借用、栈别名、锁保护和 native 临界区禁止跨暂停。 */
#define ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT ((TZrUInt32)1u << 0u)
#define ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND ((TZrUInt32)1u << 1u)
#define ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID ((TZrUInt32)1u << 2u)
#define ZR_ASYNC_FRAME_FLAG_HAS_BORROW ((TZrUInt32)1u << 3u)
#define ZR_ASYNC_FRAME_FLAG_HAS_STACK_ALIAS ((TZrUInt32)1u << 4u)
#define ZR_ASYNC_FRAME_FLAG_HAS_LOCK_GUARD ((TZrUInt32)1u << 5u)
#define ZR_ASYNC_FRAME_FLAG_NATIVE_CRITICAL ((TZrUInt32)1u << 6u)
#define ZR_ASYNC_FRAME_FLAG_KNOWN_MASK \
    (ZR_ASYNC_FRAME_FLAG_ASYNC_CONTRACT | ZR_ASYNC_FRAME_FLAG_ALLOW_SUSPEND | \
     ZR_ASYNC_FRAME_FLAG_STATE_MAP_VALID | ZR_ASYNC_FRAME_FLAG_HAS_BORROW | \
     ZR_ASYNC_FRAME_FLAG_HAS_STACK_ALIAS | ZR_ASYNC_FRAME_FLAG_HAS_LOCK_GUARD | \
     ZR_ASYNC_FRAME_FLAG_NATIVE_CRITICAL)

/* Poll 登记待处理的预算和取消；等待位保留给未来调度器适配器。 */
#define ZR_ASYNC_FRAME_PENDING_BUDGET ((TZrUInt32)1u << 0u)
#define ZR_ASYNC_FRAME_PENDING_CANCEL ((TZrUInt32)1u << 1u)
#define ZR_ASYNC_FRAME_PENDING_WAIT ((TZrUInt32)1u << 2u)

/**
 * @brief 调度器持有的帧预算与可恢复状态标量。
 * @note Init 后由适配器填入非零 frameId/generation；允许暂停时还须提供
 *       与 generation 一致的 state map。maxWorkUnits 为零表示不设工作量上限。
 * @note pinCount 只阻止 Complete/Fault/Teardown，不阻止 Poll 取消或暂停；不注册 GC 根或 retain 对象。
 *         真实保活由适配器承担；frame、wait registry 与 compile queue 没有自动相互取消或恢复的调用链。
 */
typedef struct SZrAsyncFrameBudget {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    EZrAsyncFrameStatus status;
    EZrAsyncFrameSuspendReason suspendReason;
    TZrUInt32 pendingFlags;
    TZrUInt32 stateMapId;
    TZrUInt32 pinCount;
    TZrUInt64 frameId;
    TZrUInt64 generation;
    TZrUInt64 stateMapGeneration;
    TZrUInt64 maxWorkUnits;
    TZrUInt64 consumedWorkUnits;
    TZrUInt64 pollCount;
    TZrUInt64 suspensionCount;
    volatile TZrUInt32 cancellationRequested;
    TZrUInt32 reserved;
} SZrAsyncFrameBudget;

/** @brief 清空可选诊断输出；空指针无操作。
 * TODO: 当前仅见定义与公开声明，没有仓内实际消费者；接入调度器或仓外 ABI 适配器前，从此入口核查用途、同步与失败 poststate，不据此判为死代码。
 */
ZR_CORE_API void ZrCore_AsyncFrameBudget_DiagnosticClear(
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 返回静态诊断名称；未知值返回 unknown。
 * TODO: 当前仅见定义与公开声明，没有仓内实际消费者；接入调度器或仓外 ABI 适配器前，从此入口核查用途、同步与失败 poststate，不据此判为死代码。
 */
ZR_CORE_API const TZrChar *ZrCore_AsyncFrameBudget_DiagnosticName(
        EZrAsyncFrameDiagnosticCode code);
/** @brief 返回静态帧状态名称；未知值返回 unknown。
 * TODO: 当前仅见定义与公开声明，没有仓内实际消费者；接入调度器或仓外 ABI 适配器前，从此入口核查用途、同步与失败 poststate，不据此判为死代码。
 */
ZR_CORE_API const TZrChar *ZrCore_AsyncFrameBudget_StatusName(
        EZrAsyncFrameStatus status);
/** @brief 初始化帧 ABI 与空闲状态；不分配资源。 */
ZR_CORE_API void ZrCore_AsyncFrameBudget_Init(SZrAsyncFrameBudget *frame);
/** @brief 校验标识、代际、标志及 state map 一致性。 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Validate(
        const SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 从 IDLE 进入 RUNNING 并重置用量；保留此前收到的取消请求。 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Begin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 检查异步许可、活跃资源与 state map 边界；失败填写诊断。 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_CanSuspend(
        const SZrAsyncFrameBudget *frame,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic);
/**
 * @brief 累计工作量并兑现可安全处理的取消或预算暂停。
 * @note native 临界区只登记待处理事件；无 state map 边界时继续运行。
 * @note RUNNING累积/饱和用量；取消safe边界优先且不调用CanSuspend；FAULTED outcome不保证frame.status已置FAULTED。
 */
ZR_CORE_API EZrAsyncFramePollOutcome ZrCore_AsyncFrameBudget_Poll(
        SZrAsyncFrameBudget *frame,
        TZrUInt64 workUnits,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 将可暂停帧按指定原因置为 SUSPENDED；须通过 CanSuspend 的边界检查。 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Suspend(
        SZrAsyncFrameBudget *frame,
        EZrAsyncFrameSuspendReason reason,
        TZrBool atStateMapBoundary,
        TZrBool inNativeCritical,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 标记帧完成；活跃 pin 阻止终结。 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Complete(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 将非终态帧标为故障；活跃 pin 阻止终结。
 * TODO: 当前仅见定义与公开声明，没有仓内实际消费者；接入调度器或仓外 ABI 适配器前，从此入口核查用途、同步与失败 poststate，不据此判为死代码。
 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Fault(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 增加 Complete/Fault/Teardown 的门禁计数；过量时拒绝。
 * @note 只增加标量计数，不GC注册/root/retain；调用者真正的资源保活职责未在此实现。
 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Pin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 减少一份门禁计数；零计数时拒绝，不释放外部对象。
 * @note 先Validate再拒绝0计数；成功只减标量，不释放外部对象。
 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Unpin(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/**
 * @brief 请求在下一个安全边界取消；Begin 保留启动前的请求。
 * TODO: 是否允许跨线程调用尚未由适配器规定。实现只原子写取消位，
 *       同时读写普通 status/pendingFlags；需在接入调度器前明确同步约束并补并发测试。
 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_RequestCancel(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 恢复暂停帧；若已请求取消，则改为 CANCELLED。
 * @note 正常恢复不重置consumedWorkUnits/maxWorkUnits；已取消置CANCELLED但保留suspendReason/pendingFlags。
 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Resume(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 回收非运行帧；活跃 pin 被拒绝，重复 teardown 可成功。
 * @note 只置标量终态/清pending与cancel，不释放对象/等待registry/compilequeue；活跃pin拒绝。
 */
ZR_CORE_API TZrBool ZrCore_AsyncFrameBudget_Teardown(
        SZrAsyncFrameBudget *frame,
        SZrAsyncFrameDiagnostic *diagnostic);

/** @brief 等待槽状态；唤醒、取消、超时仅有一个终态获胜。 */
typedef enum EZrAsyncWaitState {
    ZR_ASYNC_WAIT_STATE_FREE = 0,
    ZR_ASYNC_WAIT_STATE_REGISTERING,
    ZR_ASYNC_WAIT_STATE_WAITING,
    ZR_ASYNC_WAIT_STATE_READY,
    ZR_ASYNC_WAIT_STATE_CANCELLED,
    ZR_ASYNC_WAIT_STATE_TIMED_OUT,
    ZR_ASYNC_WAIT_STATE_RESUMED,
    ZR_ASYNC_WAIT_STATE_COUNT
} EZrAsyncWaitState;

/** @brief 注册表借用的单个槽位；token/generation 防止旧 handle 误认新等待。 */
typedef struct SZrAsyncWaitSlot {
    volatile TZrUInt32 state;
    volatile TZrUInt32 resumeCount;
    TZrUInt64 token;
    TZrUInt64 frameId;
    TZrUInt64 generation;
    TZrUInt64 deadlineMicros;
} SZrAsyncWaitSlot;

/** @brief 持有槽位数组的注册表；Deinit 前须先停止并释放所有等待操作。
 * @note reserved 由 Init 清零，当前等待入口未读取或验证；保留位的仓外 ABI 用途仍待适配器核查。
 */
typedef struct SZrAsyncWaitRegistry {
    volatile TZrUInt32 lock;
    TZrUInt64 nextToken;
    SZrAsyncWaitSlot *slots;
    TZrUInt32 capacity;
    TZrUInt32 reserved;
} SZrAsyncWaitRegistry;

/** @brief 指向注册表槽位的可复制身份；Release 前须结束同槽所有访问并保持 registry 有效。 */
typedef struct SZrAsyncWaitHandle {
    SZrAsyncWaitRegistry *registry;
    TZrUInt32 slotIndex;
    TZrUInt64 token;
    TZrUInt64 generation;
} SZrAsyncWaitHandle;

/**
 * @brief 一次等待注册的标量输入；包装入口使用内含的 registry/outHandle，
 *        显式 Begin 使用对应形参，所用对象均由调用方保持有效。
 * @note deadlineMicros 仅随槽位保存，超时事件由调度器显式调用 Timeout。
 * @note reserved0 当前未被 Begin 读取；deadlineMicros 只存档。
 * TODO: 调度器接入 BeginAsyncWait/Timeout 时须明确保留位与期限约定，并建立帧暂停/恢复联动。
 */
typedef struct SZrAsyncWaitRequest {
    TZrUInt32 schemaVersion;
    SZrAsyncWaitRegistry *registry;
    SZrAsyncWaitHandle *outHandle;
    TZrUInt64 frameId;
    TZrUInt64 generation;
    TZrUInt64 deadlineMicros;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrBool conditionReady;
    TZrBool allowSuspend;
    TZrBool holdsBorrow;
    TZrBool holdsStackAlias;
    TZrBool holdsLockGuard;
    TZrBool inNativeCritical;
    TZrBool reserved0;
} SZrAsyncWaitRequest;

/** @brief 借用调用方槽位数组建立等待注册表；拒绝零容量。
 * @note 借用数组首次/静止初始化，不分配资源。
 */
ZR_CORE_API TZrBool ZrCore_AsyncWaitRegistry_Init(
        SZrAsyncWaitRegistry *registry,
        SZrAsyncWaitSlot *slots,
        TZrUInt32 capacity,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 清空注册表状态；仅在所有访问与 handle 生命周期结束后调用。
 * @note 所有访问停止后清槽，不自动恢复帧，也不释放借用slots。
 */
ZR_CORE_API void ZrCore_AsyncWaitRegistry_Deinit(
        SZrAsyncWaitRegistry *registry);

/** @brief 用请求内的 registry/outHandle 注册等待；可能与唤醒并发。
 * @note 包装从request选registry/output；不连接CompileQueue，不自动frame Suspend。
 */
ZR_CORE_API TZrBool ZrCore_Execution_BeginAsyncWait(
        const SZrAsyncWaitRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 显式传入注册表与输出 handle 的等待注册入口。
 * @note 显式形参选择registry/output，不要求request嵌入对象相等；请求reserved0未读取。
 * TODO: 当前仅见定义与公开声明，没有仓内实际消费者；接入调度器或仓外 ABI 适配器前，从此入口核查用途、同步与失败 poststate，不据此判为死代码。
 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Begin(
        SZrAsyncWaitRegistry *registry,
        const SZrAsyncWaitRequest *request,
        SZrAsyncWaitHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 注册后重新检查条件；已获胜的唤醒/取消/超时不会被覆盖。 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Recheck(
        SZrAsyncWaitHandle *handle,
        TZrBool conditionReady,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 尝试将 REGISTERING/WAITING 原子转成 READY。 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Wake(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 尝试让等待由取消赢得终态。 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Cancel(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 调度器显式触发超时，尝试赢得终态。
 * @note 显式事件，不读取时钟或deadline；与Wake/Cancel争同一终态。
 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Timeout(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 对 READY/CANCELLED/TIMED_OUT 槽位只恢复一次。 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Resume(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief RESUMED 后释放槽位并清空 handle；不可与任何同槽句柄访问并发。
 * @note 先结束所有同槽访问；RESUMED才可清槽；Release不代替worker/frame resume。
 */
ZR_CORE_API TZrBool ZrCore_AsyncWait_Release(
        SZrAsyncWaitHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 查询匹配 handle 的槽位状态；失效时返回 FREE。 */
ZR_CORE_API EZrAsyncWaitState ZrCore_AsyncWait_State(
        const SZrAsyncWaitHandle *handle);
/** @brief 查询匹配槽位的恢复次数；失效时返回零。 */
ZR_CORE_API TZrUInt32 ZrCore_AsyncWait_ResumeCount(
        const SZrAsyncWaitHandle *handle);

/** @brief 后台编译任务状态；RUNNING 的取消要等 worker Complete 确认。 */
typedef enum EZrCompileJobState {
    ZR_COMPILE_JOB_FREE = 0,
    ZR_COMPILE_JOB_QUEUED,
    ZR_COMPILE_JOB_RUNNING,
    ZR_COMPILE_JOB_COMPLETED,
    ZR_COMPILE_JOB_CANCELLED,
    ZR_COMPILE_JOB_DISCARDED,
    ZR_COMPILE_JOB_COUNT
} EZrCompileJobState;

/* 预热任务由请求显式标记；其余位由 Queue 拒绝。 */
#define ZR_COMPILE_QUEUE_REQUEST_WARMUP ((TZrUInt32)1u << 0u)
#define ZR_COMPILE_QUEUE_REQUEST_KNOWN_MASK ZR_COMPILE_QUEUE_REQUEST_WARMUP

/** @brief 队列拥有的任务记录；snapshot 是从请求复制的 IR 字节。 */
typedef struct SZrCompileJobRecord {
    volatile TZrUInt32 state;
    volatile TZrUInt32 cancellationRequested;
    TZrUInt64 jobId;
    TZrUInt64 requestedGeneration;
    TZrUInt64 moduleHash;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 resultHash;
    TZrByte *snapshot;
    TZrSize snapshotLength;
    TZrUInt32 flags;
    TZrUInt32 reserved;
} SZrCompileJobRecord;

/** @brief 借用固定记录数组并拥有各记录快照；Deinit 须在 worker 停止后进行。
 * @note activeCount 包括未 Release 的终态；领取按槽索引扫描，不承诺 FIFO。
 *         IR 是宿主 malloc 字节副本，不保活任何请求外的 VM/GC 对象或实际编译器状态。
 */
typedef struct SZrCompileQueue {
    volatile TZrUInt32 lock;
    TZrUInt64 nextJobId;
    SZrCompileJobRecord *records;
    TZrUInt32 capacity;
    TZrUInt32 activeCount;
    TZrSize maxSnapshotBytes;
} SZrCompileQueue;

/** @brief 用 slotIndex/jobId 验证记录身份；调用者保管 handle 对象。 */
typedef struct SZrCompileJobHandle {
    SZrCompileQueue *queue;
    TZrUInt32 slotIndex;
    TZrUInt64 jobId;
} SZrCompileJobHandle;

/**
 * @brief 任务请求；queue/outHandle 必须等于显式 Queue 参数，IR 由队列复制。
 * @note generation、模块/签名/布局哈希在 Complete 时用于拒绝过期结果。
 */
typedef struct SZrCompileQueueRequest {
    TZrUInt32 schemaVersion;
    SZrCompileQueue *queue;
    SZrCompileJobHandle *outHandle;
    const TZrByte *irSnapshot;
    TZrSize irLength;
    TZrUInt64 requestedGeneration;
    TZrUInt64 moduleHash;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt32 flags;
    TZrBool reserved0;
    TZrBool reserved1;
    TZrBool reserved2;
    TZrBool reserved3;
} SZrCompileQueueRequest;

/* 队列锁串行化记录转移。worker 可复制 handle 值，但同一 handle 对象不能与 Release
 * 并发访问；GetSnapshot 的借用字节在 Complete 前有效。运行中的取消保持 RUNNING，
 * 由 worker Complete 确认后才允许 Release 释放快照；Deinit 只用于静止队列。 */

/** @brief 借用固定记录数组创建队列，并设置单任务快照字节上限。
 * @note 首次/已静止存储初始化；不覆盖尚持有快照的活动队列；失败不修改queue/records。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_Init(
        SZrCompileQueue *queue,
        SZrCompileJobRecord *records,
        TZrUInt32 capacity,
        TZrSize maxSnapshotBytes,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 释放队列仍持有的快照；须在所有 worker 与 handle 停止后调用。
 * @note 先结束所有worker/handle/其他调用；释放快照但不释放借用records；不通过Cancel/Complete握手。
 */
ZR_CORE_API void ZrCore_CompileQueue_Deinit(SZrCompileQueue *queue);
/** @brief 用请求中的 queue/outHandle 排入任务。
 * @note 使用request内queue/outHandle；空request等前置拒绝不先清diagnostic，Queue路径才清。
 */
ZR_CORE_API TZrBool ZrCore_Execution_QueueCompilation(
        const SZrCompileQueueRequest *request,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 复制 IR 快照并原子发布 QUEUED 任务；满队列或分配失败不发布记录。
 * @note 输出须可写且不覆盖仍需保管的活动handle；源在调用内稳定；失败不发布记录/输出，诊断更新。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_Queue(
        SZrCompileQueue *queue,
        const SZrCompileQueueRequest *request,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 按相同身份约束排入带 WARMUP 标志的任务。
 * @note 前置失败不先清diagnostic；复制请求加WARMUP，不改变领取优先级；身份必须完全一致。
 * TODO: 当前仅见定义与公开声明，没有仓内实际消费者；接入调度器或仓外 ABI 适配器前，从此入口核查用途、同步与失败 poststate，不据此判为死代码。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_QueueWarmup(
        SZrCompileQueue *queue,
        const SZrCompileQueueRequest *request,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief worker 认领一个 QUEUED 任务，转成 RUNNING 并得到 handle。
 * @note 从最低槽扫描而非FIFO；失败保持outHandle，取消终态仍占槽直到Release。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_ClaimNext(
        SZrCompileQueue *queue,
        SZrCompileJobHandle *outHandle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 取消排队任务，或对运行任务只记录待取消请求。
 * @note QUEUED立即CANCELLED；RUNNING只置取消位；CANCELLED/DISCARDED幂等，COMPLETED拒绝；不触发wait/frame取消。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_Cancel(
        SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 借用队列中的不可变 IR 快照；worker 须在 Complete 前读完。
 * @note 只QUEUED/RUNNING借用；失败不写两个输出；QUEUED借用须防并发Cancel+Release，worker应先Claim。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_GetSnapshot(
        const SZrCompileJobHandle *handle,
        const TZrByte **outSnapshot,
        TZrSize *outLength,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief worker 按代际及哈希确认结果；过期或已取消任务转为 DISCARDED。
 * @note 只有RUNNING可确认；取消/哈希不匹配置DISCARDED仍占槽；成功仅存resultHash而非安装机器码；调用后worker结束借用。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_Complete(
        SZrCompileJobHandle *handle,
        TZrUInt64 currentGeneration,
        TZrUInt64 moduleHash,
        TZrUInt64 signatureHash,
        TZrUInt64 layoutHash,
        TZrUInt64 resultHash,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 仅对终态任务释放快照与槽位，并清空调用方 handle。
 * @note 仅终态回收snapshot并清identity/handle；同一handle对象不可与Release并发；其他handle副本由jobId失效。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_Release(
        SZrCompileJobHandle *handle,
        SZrAsyncFrameDiagnostic *diagnostic);
/** @brief 查询匹配 handle 的记录状态；失效时返回 FREE。
 * @note 不匹配/无效返回FREE，不区分未提交和已过期；queue存储仍须存活。
 */
ZR_CORE_API EZrCompileJobState ZrCore_CompileQueue_State(
        const SZrCompileJobHandle *handle);
/** @brief 查询匹配任务的预热标志；失效时返回假。
 * @note 匹配记录才读取预热位；标记不是调度优先级；失效返回false。
 */
ZR_CORE_API TZrBool ZrCore_CompileQueue_IsWarmup(
        const SZrCompileJobHandle *handle);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_ASYNC_FRAME_BUDGET_H */
