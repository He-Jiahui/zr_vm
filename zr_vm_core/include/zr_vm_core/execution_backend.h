#ifndef ZR_VM_CORE_EXECUTION_BACKEND_H
#define ZR_VM_CORE_EXECUTION_BACKEND_H

/*
 * Runtime execution-backend service contract (SSA plan 10.01).
 *
 * The compile request, generation key, and code information are immutable
 * scalar witnesses.  They may be copied into a queue or a diagnostic record;
 * they deliberately contain no VM object, AST, executable address, or
 * callback pointer.  Callback pointers and the service/record pointers below
 * are process-local runtime state only and must never be written to an
 * artifact, CallBinding row, or hot-update package.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 进程内结构的形状标识与固定墓碑容量；版本检查不保证外部数组存储合法。
 */
#define ZR_EXECUTION_BACKEND_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXECUTION_BACKEND_MAGIC ((TZrUInt32)0x314b4245u) /* EBK1 */
#define ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY ((TZrUInt32)16u)

/* Operations are a capability mask; every requested bit must be supported. */
/** @brief 能力集合按请求位的子集匹配，未知位拒绝；这些位不授予后端实际机器码生成能力。
 */
#define ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_OPERATION_CONTROL ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_OPERATION_DIRECT_CALL ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_BACKEND_OPERATION_SIMPLE_MEMBER ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_BACKEND_OPERATION_ARRAY ((TZrUInt32)1u << 4u)
#define ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR | \
     ZR_EXECUTION_BACKEND_OPERATION_CONTROL | \
     ZR_EXECUTION_BACKEND_OPERATION_DIRECT_CALL | \
     ZR_EXECUTION_BACKEND_OPERATION_SIMPLE_MEMBER | \
     ZR_EXECUTION_BACKEND_OPERATION_ARRAY)

/* Metadata registrations required before a code record can be published. */
/** @brief 图注册声明的位集合，完成时要求请求子集和对应非零 hash；平台注册本体仍由后端负责。
 */
#define ZR_EXECUTION_BACKEND_MAP_ROOTS ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_MAP_EH ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_MAP_DEBUG ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_BACKEND_MAP_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_BACKEND_MAP_IMPORTS ((TZrUInt32)1u << 4u)
#define ZR_EXECUTION_BACKEND_MAP_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_MAP_ROOTS | ZR_EXECUTION_BACKEND_MAP_EH | \
     ZR_EXECUTION_BACKEND_MAP_DEBUG | ZR_EXECUTION_BACKEND_MAP_DEOPT | \
     ZR_EXECUTION_BACKEND_MAP_IMPORTS)

/** @brief 请求许可决定回退策略；IMMUTABLE_INPUT 为接纳前提，REQUIRE_MACHINE_CODE 优先禁止任何回退。
 */
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_EXECBC_FALLBACK ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_AOT_FALLBACK ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_REQUIRE_MACHINE_CODE ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_BACKEND_REQUEST_FLAG_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_REQUEST_FLAG_IMMUTABLE_INPUT | \
     ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_EXECBC_FALLBACK | \
     ZR_EXECUTION_BACKEND_REQUEST_FLAG_ALLOW_AOT_FALLBACK | \
     ZR_EXECUTION_BACKEND_REQUEST_FLAG_REQUIRE_MACHINE_CODE)

/** @brief 描述符标志只校验已知位；当前服务仍由宿主显式 ProcessNext 驱动，不按 ASYNC 位启动线程。
 */
#define ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_ASYNC ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_RUNTIME_ONLY ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_KNOWN_MASK \
    (ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_ASYNC | \
     ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_RUNTIME_ONLY)

/** @brief 后端目标类别用于注册选择；NONE/COUNT 是无目标和边界，不可提交编译。
 */
typedef enum EZrExecutionBackendTargetKind {
    ZR_EXECUTION_BACKEND_TARGET_NONE = 0,
    ZR_EXECUTION_BACKEND_TARGET_EXECBC = 1,
    ZR_EXECUTION_BACKEND_TARGET_AOT = 2,
    ZR_EXECUTION_BACKEND_TARGET_HOST_JIT = 3,
    ZR_EXECUTION_BACKEND_TARGET_COUNT
} EZrExecutionBackendTargetKind;

/** @brief 记录宿主下一执行路径的选择，不携带产物，也不证明 AOT 或 ExecBC 已执行。
 */
typedef enum EZrExecutionBackendFallback {
    ZR_EXECUTION_BACKEND_FALLBACK_NONE = 0,
    ZR_EXECUTION_BACKEND_FALLBACK_EXECBC = 1,
    ZR_EXECUTION_BACKEND_FALLBACK_AOT = 2
} EZrExecutionBackendFallback;

/** @brief 区分接纳、回退、状态拒绝与清理失败；返回状态与诊断中的原回退原因可能不同。
 */
typedef enum EZrExecutionBackendStatus {
    ZR_EXECUTION_BACKEND_STATUS_OK = 0,
    /* A queued or backend-pending operation is a normal non-blocking result. */
    ZR_EXECUTION_BACKEND_STATUS_PENDING,
    ZR_EXECUTION_BACKEND_STATUS_FALLBACK_EXECBC,
    ZR_EXECUTION_BACKEND_STATUS_FALLBACK_AOT,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_MAGIC,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_SCHEMA,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_FLAGS,
    ZR_EXECUTION_BACKEND_STATUS_CAPACITY,
    ZR_EXECUTION_BACKEND_STATUS_ALREADY_REGISTERED,
    ZR_EXECUTION_BACKEND_STATUS_NOT_REGISTERED,
    ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE,
    ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET,
    ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION,
    ZR_EXECUTION_BACKEND_STATUS_IMMUTABLE_INPUT_REQUIRED,
    ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH,
    ZR_EXECUTION_BACKEND_STATUS_STALE_GENERATION,
    ZR_EXECUTION_BACKEND_STATUS_STALE_TICKET,
    ZR_EXECUTION_BACKEND_STATUS_INVALID_STATE,
    ZR_EXECUTION_BACKEND_STATUS_CANCELLED,
    ZR_EXECUTION_BACKEND_STATUS_COMPILE_FAILED,
    ZR_EXECUTION_BACKEND_STATUS_CODE_INVALID,
    ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_FOUND,
    ZR_EXECUTION_BACKEND_STATUS_CODE_NOT_PUBLISHED,
    ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE,
    ZR_EXECUTION_BACKEND_STATUS_ACTIVE_LEASE,
    ZR_EXECUTION_BACKEND_STATUS_RETIRE_FAILED,
    ZR_EXECUTION_BACKEND_STATUS_SHUTTING_DOWN,
    ZR_EXECUTION_BACKEND_STATUS_IN_FLIGHT,
    ZR_EXECUTION_BACKEND_STATUS_RESUME_UNAVAILABLE,
    ZR_EXECUTION_BACKEND_STATUS_RESUME_FAILED,
    ZR_EXECUTION_BACKEND_STATUS_OVERFLOW,
    ZR_EXECUTION_BACKEND_STATUS_COUNT
} EZrExecutionBackendStatus;

/** @brief 作业的接纳到发布状态；失败/取消槽可复用，发布作业要等待关联代码回收才释放。
 */
typedef enum EZrExecutionBackendJobState {
    ZR_EXECUTION_BACKEND_JOB_FREE = 0,
    ZR_EXECUTION_BACKEND_JOB_QUEUED,
    ZR_EXECUTION_BACKEND_JOB_COMPILING,
    ZR_EXECUTION_BACKEND_JOB_READY,
    ZR_EXECUTION_BACKEND_JOB_FAILED,
    ZR_EXECUTION_BACKEND_JOB_CANCELLED,
    ZR_EXECUTION_BACKEND_JOB_PUBLISHED
} EZrExecutionBackendJobState;

/** @brief 代码记录的发布与资源回收阶段；RETIRED 阻止新获取，RECLAIMING 独占清理，RETIRE_FAILED 保留重试记录。
 */
typedef enum EZrExecutionBackendCodeState {
    ZR_EXECUTION_BACKEND_CODE_FREE = 0,
    ZR_EXECUTION_BACKEND_CODE_READY,
    ZR_EXECUTION_BACKEND_CODE_PUBLISHED,
    ZR_EXECUTION_BACKEND_CODE_RETIRED,
    ZR_EXECUTION_BACKEND_CODE_RECLAIMING,
    ZR_EXECUTION_BACKEND_CODE_RETIRE_FAILED
} EZrExecutionBackendCodeState;

/** @brief 单一图查询类别，和 mapRegistrationFlags 的位集合由内部映射连接；不代表实际图存储。
 */
typedef enum EZrExecutionBackendMapKind {
    ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS = 0,
    ZR_EXECUTION_BACKEND_MAP_KIND_EH,
    ZR_EXECUTION_BACKEND_MAP_KIND_DEBUG,
    ZR_EXECUTION_BACKEND_MAP_KIND_DEOPT,
    ZR_EXECUTION_BACKEND_MAP_KIND_IMPORTS,
    ZR_EXECUTION_BACKEND_MAP_KIND_COUNT
} EZrExecutionBackendMapKind;

/** @brief 把代码所属域、模块、代次和后端注册共同限定为失效命名空间；选择注册前请求允许第四项为零。
 */
typedef struct SZrExecutionGenerationKey {
    /* All four fields are required for an invalidation namespace. */
    TZrUInt64 domainIdentity;
    TZrUInt64 moduleIdentity;
    TZrUInt64 generation;
    TZrUInt64 backendRegistrationIdentity;
} SZrExecutionGenerationKey;

/* Kept opaque here so core does not need to include parser/ExecIR storage. */
struct SZrExecIrResumeRequest;

/** @brief 注册后端的标量目标见证；ABI/layout 参与候选选择，完整目标能力继续交 queryTarget 确认。
 */
typedef struct SZrExecutionBackendTarget {
    TZrUInt32 schemaVersion;
    EZrExecutionBackendTargetKind kind;
    TZrUInt32 abiVersion;
    TZrUInt32 reserved;
    TZrUInt64 targetTripleHash;
    TZrUInt64 layoutHash;
    TZrUInt64 capabilityHash;
} SZrExecutionBackendTarget;

/* This diagnostic is scalar so it remains valid after a worker returns. */
/** @brief 可复制的失败位置和身份对照；expected/actual 的含义由当前状态分支决定，不保存 worker 指针。
 */
typedef struct SZrExecutionBackendDiagnostic {
    EZrExecutionBackendStatus status;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt32 expectedState;
    TZrUInt32 actualState;
    TZrUInt64 ticketId;
    TZrUInt64 codeIdentity;
    TZrUInt64 expected;
    TZrUInt64 actual;
    SZrExecutionGenerationKey expectedKey;
    SZrExecutionGenerationKey actualKey;
} SZrExecutionBackendDiagnostic;

/* Immutable scalar input copied into a worker job. */
/** @brief 入队时按值复制的不可变输入见证；contract 与 generationKey 的模块及代次须一致，hash 不代替 IR 正文。
 */
typedef struct SZrExecutionCompileRequest {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    EZrExecutionBackendTargetKind requestedTarget;
    SZrExecutionGenerationKey generationKey;
    SZrExecutionContract contract;
    TZrUInt64 immutableIrHash;
    TZrUInt64 compileInputHash;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt32 requiredOperations;
    TZrUInt32 requiredMapFlags;
} SZrExecutionCompileRequest;

/** @brief 供宿主查询及 worker 完成的标量身份；state 是调用时快照，fallback 票据不具有可查询作业的 ticketId。
 */
typedef struct SZrExecutionCompileTicket {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt64 serviceIdentity;
    TZrUInt64 ticketId;
    SZrExecutionGenerationKey generationKey;
    EZrExecutionBackendTargetKind target;
    EZrExecutionBackendFallback fallback;
    EZrExecutionBackendJobState state;
} SZrExecutionCompileTicket;

/** @brief ProcessNext 在锁内保存的票据与请求快照；回调异步使用时必须复制，不能保留栈上指针。
 */
typedef struct SZrExecutionBackendCompileInvocation {
    SZrExecutionCompileTicket ticket;
    SZrExecutionCompileRequest request;
} SZrExecutionBackendCompileInvocation;

/* Runtime-only code metadata. No executable address is stored here. */
/** @brief 产物的身份、大小及已注册图 hash 声明；和请求一致才可接收，代码/图本体由后端管理，不存入口地址。
 */
typedef struct SZrExecutionBackendCodeInfo {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    SZrExecutionGenerationKey generationKey;
    SZrExecutionContract contract;
    TZrUInt64 immutableIrHash;
    TZrUInt64 compileInputHash;
    TZrUInt64 codeIdentity;
    TZrUInt32 codeSize;
    TZrUInt32 mapRegistrationFlags;
    TZrUInt64 rootMapHash;
    TZrUInt64 ehMapHash;
    TZrUInt64 debugMapHash;
    TZrUInt64 deoptMapHash;
    TZrUInt64 runtimeImportsHash;
} SZrExecutionBackendCodeInfo;

/** @brief 完成回调沿用 CodeInfo 的标量格式；接收元数据不转移可执行地址到持久化契约。
 */
typedef SZrExecutionBackendCodeInfo SZrExecutionBackendCompiledCode;

/** @brief QueryTicket 复制的观察快照，记录实际状态及最后状态；不持有作业槽或代码寿命。
 */
typedef struct SZrExecutionCompileTicketView {
    TZrUInt64 ticketId;
    EZrExecutionBackendJobState state;
    EZrExecutionBackendTargetKind target;
    EZrExecutionBackendFallback fallback;
    EZrExecutionBackendStatus lastStatus;
    TZrUInt64 codeIdentity;
    SZrExecutionGenerationKey generationKey;
} SZrExecutionCompileTicketView;

/** @brief 执行租约内的代码元数据与回收阻塞计数快照；观察结果不增加租约。
 */
typedef struct SZrExecutionCodeView {
    SZrExecutionBackendCodeInfo info;
    EZrExecutionBackendCodeState state;
    TZrUInt32 leaseCount;
    TZrUInt32 dependencyLeaseCount;
} SZrExecutionCodeView;

/* A lease is the only supported way to ask a backend for an entry address. */
/** @brief 执行租约的进程内凭据，用多重身份抵御槽复用；复制值不增加计数，依赖租约必须先归还。
 */
typedef struct SZrExecutionCodeHandle {
    TZrUInt32 magic;
    TZrUInt32 slotIndex;
    TZrUInt64 serviceIdentity;
    TZrUInt64 codeIdentity;
    SZrExecutionGenerationKey generationKey;
    TZrBool leased;
    TZrBool dependencyLeased;
    TZrUInt16 reserved;
} SZrExecutionCodeHandle;

/** @brief 注册结果的服务和注册身份，供宿主保存 owner 名称；记录不持有额外租约。
 */
typedef struct SZrExecutionBackendRegistration {
    TZrUInt64 serviceIdentity;
    TZrUInt64 registrationIdentity;
    EZrExecutionBackendTargetKind backendKind;
} SZrExecutionBackendRegistration;

typedef struct SZrExecutionBackendService SZrExecutionBackendService;
typedef struct SZrExecutionBackendDescriptor SZrExecutionBackendDescriptor;

/** @brief 在提交线程锁外确认目标能力；失败交服务决定回退，回调不等于编译。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendQueryTarget)(
        const SZrExecutionBackendTarget *target,
        TZrUInt32 requiredOperations,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 由 ProcessNext 锁外调用；OK 填同步产物，PENDING 由后端稍后 Complete，异步所需 invocation 须复制。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendCompileAsync)(
        const SZrExecutionBackendCompileInvocation *invocation,
        TZrPtr userData,
        SZrExecutionBackendCompiledCode *completedCode,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 取消编译中的票据；OK/CANCELLED 使作业终止，PENDING 或失败保留取消意图供迟到 Complete。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendCancelCompile)(
        const SZrExecutionCompileTicket *ticket,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在执行租约保活期间查询进程内入口；成功须返回非零地址，其 ABI 与机器码正确性由后端负责。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendLookupEntry)(
        const SZrExecutionBackendCodeInfo *code,
        TZrNativePtr *entryAddress,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 查询指定已声明图的非零 hash；不返回图存储，省略回调时服务读取 CodeInfo 中的 hash。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendQueryMap)(
        const SZrExecutionBackendCodeInfo *code,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 清理代码前撤销后端管理的图注册；已接收记录仅撤图成功后才继续退休，失败保留重试状态。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendUnregisterMaps)(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 撤图后释放或退休后端代码资源；未接收产物清理会在撤图失败时仍尝试此回调。
 */
typedef EZrExecutionBackendStatus (*FZrExecutionBackendRetire)(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 注册不再被作业、代码和回调引用后销毁后端上下文；服务不自行 free userData。
 */
typedef void (*FZrExecutionBackendDestroy)(TZrPtr userData);

/** @brief 进程内后端派发和资源清理协议；通常锁内保存快照并登记回调后锁外调用；Complete 拒绝或过期产物的锁外交接存在下述清理计数 TODO，宿主关闭与 worker 时序尚待核实。
 */
typedef struct SZrExecutionBackendVTable {
    FZrExecutionBackendQueryTarget queryTarget;
    FZrExecutionBackendCompileAsync compileAsync;
    FZrExecutionBackendCancelCompile cancelCompile;
    FZrExecutionBackendLookupEntry lookupEntry;
    FZrExecutionBackendQueryMap queryMap;
    FZrExecutionBackendUnregisterMaps unregisterMaps;
    FZrExecutionBackendRetire retire;
    FZrExecutionBackendDestroy destroy;
} SZrExecutionBackendVTable;

/** @brief 注册时按值复制的后端能力与回调集合；userData 为后端管理的进程内上下文，flags 当前只验证已知位，不驱动作业调度。
 */
struct SZrExecutionBackendDescriptor {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrExecutionBackendTargetKind backendKind;
    TZrUInt32 flags;
    SZrExecutionBackendTarget target;
    TZrUInt32 supportedOperations;
    TZrUInt32 reserved;
    SZrExecutionBackendVTable vtable;
    TZrPtr userData;
};

/* Caller-owned arrays make capacity/OOM behaviour deterministic and avoid
 * hidden allocations in the frame-thread path. These pointers are runtime
 * state and are never copied into persistent contracts. */
/** @brief 宿主定长数组中的注册槽；active 与单调注册身份共同决定 owner 是否仍可派发。
 */
typedef struct SZrExecutionBackendRegistrationRecord {
    TZrUInt64 registrationIdentity;
    TZrBool active;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    SZrExecutionBackendDescriptor descriptor;
} SZrExecutionBackendRegistrationRecord;

/** @brief 队列保存请求、票据与完成结果的副本；registrationSlot 指向后端 owner，codeSlot 在产物接收前为无效槽。
 */
typedef struct SZrExecutionCompileJob {
    SZrExecutionCompileTicket ticket;
    SZrExecutionCompileRequest request;
    SZrExecutionBackendCompiledCode result;
    EZrExecutionBackendStatus lastStatus;
    TZrUInt32 registrationSlot;
    TZrUInt32 codeSlot;
    TZrBool cancelRequested;
    TZrBool resultValid;
    TZrUInt16 reserved;
} SZrExecutionCompileJob;

/** @brief 代码表保存产物和清理进度；双租约计数及 mapsRegistered 决定何时、从哪一步重试撤图退休。
 */
typedef struct SZrExecutionCodeRecord {
    SZrExecutionBackendCodeInfo info;
    EZrExecutionBackendCodeState state;
    TZrUInt32 registrationSlot;
    TZrUInt32 jobSlot;
    TZrUInt32 leaseCount;
    TZrUInt32 dependencyLeaseCount;
    TZrBool mapsRegistered;
    TZrBool reserved0;
    TZrUInt16 reserved1;
} SZrExecutionCodeRecord;

/** @brief 由宿主实施帧恢复；服务只转交借用 request 和诊断，不提供 VM 状态图重建实现。
 */
typedef TZrBool (*FZrExecutionBackendResumeInterpreter)(
        const struct SZrExecIrResumeRequest *request,
        TZrPtr userData,
        SZrExecIrDiagnostic *diagnostic);

/** @brief 宿主拥有的定长状态机，不拥有 worker 或数组分配；服务锁保护表状态，在途计数保护已登记回调；Complete 未接收产物清理的解锁至登记间隙保留 TODO，不保证该交接已阻挡 owner 销毁。
 */
struct SZrExecutionBackendService {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    volatile TZrUInt32 lock;
    TZrUInt64 serviceIdentity;
    TZrUInt64 nextRegistrationIdentity;
    TZrUInt64 nextTicketId;
    SZrExecutionBackendRegistrationRecord *registrations;
    TZrUInt32 registrationCapacity;
    TZrUInt32 registrationCount;
    SZrExecutionCompileJob *jobs;
    TZrUInt32 jobCapacity;
    TZrUInt32 jobCount;
    SZrExecutionCodeRecord *codes;
    TZrUInt32 codeCapacity;
    TZrUInt32 codeCount;
    FZrExecutionBackendResumeInterpreter resumeInterpreter;
    TZrPtr resumeUserData;
    TZrBool shuttingDown;
    TZrBool destroyed;
    TZrBool shutdownDestroyCalled;
    TZrUInt8 reserved;
    /* queryTarget runs outside the lock but still pins its registration. */
    volatile TZrUInt32 queryTargetInFlight;
    /* Every callback that owns a copied descriptor/userData pair is counted;
     * shutdown/finalization cannot destroy a registration while it is live. */
    volatile TZrUInt32 backendCallbackInFlight;
    /* Tombstones make InvalidateGeneration a prevention boundary, not only a
     * best-effort sweep of records that happened to exist at call time. */
    SZrExecutionGenerationKey invalidatedKeys[ZR_EXECUTION_BACKEND_INVALIDATION_CAPACITY];
    TZrUInt32 invalidatedKeyCount;
    TZrUInt32 reservedInvalidation;
};

/** @brief TODO: 诊断/键/状态辅助接口及 Deinit、Unregister、Cancel 当前未见仓内上游调用；宿主 ABI 接入时在 test_ssa_backend_service 补充注销、取消和解绑后 Deinit 的实际调用链，确认仓外契约。
 */
/* Diagnostics and scalar key helpers. */
/** @brief 清除可选的标量诊断，供宿主复用输出存储；空指针可直接传入。
 */
ZR_CORE_API void ZrCore_ExecutionBackend_DiagnosticClear(
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 检查可用于精确失效的完整四元键；请求选择后端前允许的零注册身份不属于完整键。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_GenerationKeyIsValid(
        const SZrExecutionGenerationKey *key);
/** @brief 比较域、模块、代次和注册身份；相等只表示标量相等，不证明键有效。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_GenerationKeyEqual(
        const SZrExecutionGenerationKey *left,
        const SZrExecutionGenerationKey *right);
/** @brief 取得静态状态名称用于诊断展示；未知值和 COUNT 返回统一错误名称，调用方不释放返回字符串。
 */
ZR_CORE_API const TZrChar *ZrCore_ExecutionBackend_StatusName(
        EZrExecutionBackendStatus status);

/* Explicit service instance API. */
/** @brief 绑定宿主提供的定长注册、作业和代码数组，并清零其内容。
 * @pre 数组及 service 必须为独立、足够大且可写的存储，覆盖最终关闭与 Deinit；初始化和重初始化由宿主独占执行。
 * @note 不分配内存、不创建 worker；serviceIdentity 应区分仍可能返回的旧 ticket。resumeUserData 由宿主维持寿命。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Init(
        SZrExecutionBackendService *service,
        TZrUInt64 serviceIdentity,
        SZrExecutionBackendRegistrationRecord *registrations,
        TZrUInt32 registrationCapacity,
        SZrExecutionCompileJob *jobs,
        TZrUInt32 jobCapacity,
        SZrExecutionCodeRecord *codes,
        TZrUInt32 codeCapacity,
        FZrExecutionBackendResumeInterpreter resumeInterpreter,
        TZrPtr resumeUserData,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在 FinalizeShutdown 已标记 destroyed 后撤销服务的借用指针；未完成关闭时保持原状。
 * @pre 宿主已停止并发访问，并先解绑默认 facade；本函数不释放数组，也不等待 worker。
 */
ZR_CORE_API void ZrCore_ExecutionBackendService_Deinit(
        SZrExecutionBackendService *service);
/** @brief 复制进程内后端描述符，分配单调注册身份供请求选择与精确失效。
 * @note userData 不被深拷贝；后端必须保持回调和上下文有效，直到 Unregister 或 FinalizeShutdown 调用 destroy。注册成功不代表机器码可用。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Register(
        SZrExecutionBackendService *service,
        const SZrExecutionBackendDescriptor *descriptor,
        SZrExecutionBackendRegistration *registration,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在没有引用作业、代码记录或在途回调时撤销注册，并在服务锁外调用 destroy。
 * @return IN_FLIGHT 或 ACTIVE_LEASE 表示仍需先排空相关状态；失败时保留注册。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Unregister(
        SZrExecutionBackendService *service,
        TZrUInt64 registrationIdentity,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 校验标量不可变请求、选择后端并复制入队；在调用线程上于锁外查询目标能力。
 * @return PENDING 表示接纳，不表示编译完成；fallback ticket 没有可查询作业身份，diagnostic.status 保留回退原因。
 * @note 不启动 worker；宿主随后驱动 ProcessNext。查询回调仍可能耗时，不能据此推导实时无阻塞保证。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_CompileAsync(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 由宿主调度器取首个排队作业，标为 COMPILING 后在锁外派发编译回调。
 * @note 同步 OK 走 Complete；PENDING 的后端随后提交完成结果。服务不保存 invocation 指针，异步后端须自行复制所需内容。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ProcessNext(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 接收 worker 产物并校验完整代次、执行契约、输入 hash 与图声明，建立尚未发布的 READY 记录。
 * @note 被取消、失配或重复身份的结果走后端清理；过期 ticket 仅在仍能找到注册时尝试清理。失败后通过 QueryTicket 读取实际作业状态，不能仅依赖传入 ticket.state。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Complete(
        SZrExecutionBackendService *service,
        SZrExecutionCompileTicket *ticket,
        const SZrExecutionBackendCompiledCode *code,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 显式允许 READY 代码被 AcquireCode 取得，同时退休同一完整代次和 targetToken 的旧发布记录。
 * @note 旧记录仍由已有租约保活；这里只检查已接收的图注册声明，不执行代码或注册平台 GC 图。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Publish(
        SZrExecutionBackendService *service,
        SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在锁内复制当前作业状态快照，供宿主观察排队、编译、完成和发布。
 * @note 失败或取消槽可被下一请求复用；旧 ticket 随后返回 STALE_TICKET。快照本身不持有代码租约。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryTicket(
        const SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionCompileTicketView *view,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 取消排队作业或向编译中后端发送取消请求；READY 和 PUBLISHED 不接受此入口。
 * @note 后端返回 PENDING 或失败时作业可能仍在编译，cancelRequested 保留；迟到产物仍须通过 Complete 交回清理。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Cancel(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 记录完整四元键的失效墓碑，取消匹配作业并退休匹配代码，阻止之后同键入队。
 * @return OK 表示墓碑及状态标记已处理，不表示异步取消、租约释放或代码回收已完成；墓碑容量耗尽则不新增失效键。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_InvalidateGeneration(
        SZrExecutionBackendService *service,
        const SZrExecutionGenerationKey *key,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 从当前 PUBLISHED 作业取得执行租约，并写入带服务、槽位、代码身份和完整键的句柄。
 * @pre 输出句柄不得覆盖尚未归还的租约；复制句柄不增加计数，不应作为独立租约重复归还。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_AcquireCode(
        SZrExecutionBackendService *service,
        const SZrExecutionCompileTicket *ticket,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在已持有执行租约的句柄上追加一次依赖租约，保留图与相关后端资源。
 * @note 同一句柄只能追加一次；退休记录上的现有执行租约仍可使用此入口。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_AcquireDependencyLease(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 归还该句柄的依赖租约，保留执行租约供后续 ReleaseCode。
 * @note 不直接回收代码；无依赖租约时返回 INVALID_STATE。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ReleaseDependencyLease(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 归还执行租约并清零句柄；代码退休和回收由失效及 CollectRetired 分别驱动。
 * @note 记录上任何依赖租约尚存时均返回 ACTIVE_LEASE，包括其他句柄持有的依赖租约。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_ReleaseCode(
        SZrExecutionBackendService *service,
        SZrExecutionCodeHandle *handle,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在执行租约有效时复制代码状态和两类计数，供宿主观察退休延迟。
 * @note 快照不新增租约，也不包含可执行地址。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryCode(
        const SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        SZrExecutionCodeView *view,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 凭有效执行租约在锁外向注册后端查询非零入口地址。
 * @pre 在使用返回地址的整个期间保留该租约；查询回调的计数只覆盖回调，不代替执行租约。
 * @note 服务不验证地址对应的机器码或调用 ABI；缺回调和零地址分别拒绝。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_LookupEntry(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 凭有效执行租约查询已声明注册的图 hash；无查询回调时读取记录中的标量 hash。
 * @note 非零 hash 不证明 GC、unwind 或 deopt 图内容正确，也不返回图的存储指针。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_QueryMap(
        SZrExecutionBackendService *service,
        const SZrExecutionCodeHandle *handle,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 扫描退休或回收失败记录，在两类租约均归零后于锁外撤图，再退休代码。
 * @note 活跃租约被跳过；其他失败保留 RETIRE_FAILED 与图撤销进度以便重试，outCollected 可包含失败前已成功回收的数量。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_CollectRetired(
        SZrExecutionBackendService *service,
        TZrUInt32 *outCollected,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 停止新注册和入队，取消排队作业、请求编译取消并退休代码。
 * @return IN_FLIGHT 时保持关闭阶段，待 worker 或回调结束后重试；OK 仍可能有活跃代码租约，需要归还后 FinalizeShutdown。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_Shutdown(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 在关闭阶段排空回调及编译、回收代码，随后在锁外销毁各注册并标记 destroyed。
 * @note ACTIVE_LEASE 或 RETIRE_FAILED 保留后端清理入口供重试；成功不释放宿主数组，最后由宿主 Deinit。
 */
ZR_CORE_API EZrExecutionBackendStatus ZrCore_ExecutionBackendService_FinalizeShutdown(
        SZrExecutionBackendService *service,
        SZrExecutionBackendDiagnostic *diagnostic);
/** @brief 将借用的恢复请求在锁外转交宿主提供的解释器回调，并把未定位的失败补为 materialization 诊断。
 * @note 本层只调度，不检查状态图或重建 VM 帧；回调未安装则返回 unsupported。宿主须保持请求及恢复上下文有效。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackendService_ResumeInterpreter(
        SZrExecutionBackendService *service,
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic);

/** @brief TODO: 仓内未发现默认 facade 的实际宿主安装/解绑链；接入时从 SetDefaultService 与 Deinit 核对并发寿命协议，不能只依赖默认指针锁。
 */
/* Optional process-local default facade matching the plan's short API names. */
/** @brief 设置或清空进程内默认服务的借用指针，供短接口派发。
 * @pre 宿主协调替换、关闭和释放；默认锁只保护指针读写，不延长 service 寿命。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_SetDefaultService(
        SZrExecutionBackendService *service);
/** @brief 向默认服务注册描述符，以 bool 隐去详细状态；需要失败原因时使用显式 service API。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_Register(
        const SZrExecutionBackendDescriptor *backend);
/** @brief 向默认服务提交请求；PENDING、ExecBC 或 AOT 回退均返回 true，须继续查看 ticket 而非认为机器码已生成。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_CompileAsync(
        const SZrExecutionCompileRequest *request,
        SZrExecutionCompileTicket *ticket);
/** @brief 向默认服务发起精确键失效，以 bool 返回处理状态；不会等待租约回收。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_InvalidateGeneration(
        const SZrExecutionGenerationKey *key);
/** @brief 把恢复请求转交当前默认服务；服务缺失或恢复回调失败时返回 false 并填可选诊断。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBackend_ResumeInterpreter(
        const struct SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_EXECUTION_BACKEND_H */
