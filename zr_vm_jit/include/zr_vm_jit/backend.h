#ifndef ZR_VM_JIT_BACKEND_H
#define ZR_VM_JIT_BACKEND_H

/**
 * @file
 * @brief 可选主机基线 JIT 的 C ABI 契约 facade。
 *
 * ## Summary
 * core 保持 C11；本头文件只暴露值类型、core 代码记录句柄及 facade 函数，
 * 由独立 C++ target 承载适配器。当前 target 只验证目标、导入、图声明和
 * 记录生命周期，不生成机器码；Compile 与 descriptor 回调据实返回不可用或 fallback。
 *
 * ## Constraints and lifecycle
 * 所有输入结构均为进程内 ABI，不是磁盘格式。代码 identity/hash 可参与契约核对，
 * 但本接口不持久化函数地址。成功 Acquire 的单份 lease 必须配对 Release；
 * 调用方不得复制后把句柄副本当作另一份 lease。当前实现以进程级互斥量串行化
 * singleton 及其记录操作，回调仅在 descriptor 被同步调用期间使用传入值。
 *
 * ## References
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_Compile
 * @see ZrJit_Host_TryShutdown
 */

#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_core/execution_backend.h"
#include "zr_vm_core/host_baseline_jit.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief CompileRequest、StateMapFacts 与 BackendInfo 共用的进程内 schema 版本。 */
#define ZR_JIT_HOST_SCHEMA_VERSION ((TZrUInt32)1u)
/** @brief 进程内 CompileRequest / BackendInfo 的识别值，不是序列化文件 magic。 */
#define ZR_JIT_HOST_MAGIC ((TZrUInt32)0x314a485au) /* ZHJ1 */

/** @brief 请求允许回落到 ExecBC；注册选项也须允许 fallback 才能采用。 */
#define ZR_JIT_HOST_OPTION_ALLOW_FALLBACK ((TZrUInt32)1u << 0u)
/** @brief 请求要求机器码；provider 不可用时不得以 fallback 满足此要求。 */
#define ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE ((TZrUInt32)1u << 1u)
/** @brief 当前请求允许的 flags 掩码；校验拒绝此掩码之外的位。 */
#define ZR_JIT_HOST_OPTION_KNOWN_MASK \
    (ZR_JIT_HOST_OPTION_ALLOW_FALLBACK | ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE)

/** @brief StateMapFacts 中 roots 登记声明位。 */
#define ZR_JIT_HOST_MAP_ROOTS ((TZrUInt32)1u << 0u)
/** @brief StateMapFacts 中 unwind 登记声明位。 */
#define ZR_JIT_HOST_MAP_UNWIND ((TZrUInt32)1u << 1u)
/** @brief StateMapFacts 中 debug 登记声明位。 */
#define ZR_JIT_HOST_MAP_DEBUG ((TZrUInt32)1u << 2u)
/** @brief StateMapFacts 中 deopt 登记声明位。 */
#define ZR_JIT_HOST_MAP_DEOPT ((TZrUInt32)1u << 3u)
/** @brief 四类图声明位掩码；公开校验要求四个位全部存在。 */
#define ZR_JIT_HOST_MAP_KNOWN_MASK \
    (ZR_JIT_HOST_MAP_ROOTS | ZR_JIT_HOST_MAP_UNWIND | \
     ZR_JIT_HOST_MAP_DEBUG | ZR_JIT_HOST_MAP_DEOPT)

/**
 * ## Summary
 * @brief Facade 契约操作结果。
 *
 * ## Value model
 * 值区分输入/生命周期拒绝、provider 不可用和建议上层选择 ExecBC/AOT；
 * 它不表示 core manager 状态或机器指令执行结果。
 *
 * ## Constraints and lifecycle
 * 这是稳定的进程内 C ABI 枚举。未知值传给 StatusName 时得到通用名称；
 * 不持有资源，也无需初始化或释放。
 *
 * ## References
 * @see ZrJit_Host_StatusName
 * @see ZrJit_Host_Compile
 */
typedef enum EZrJitHostStatus {
    ZR_JIT_HOST_STATUS_OK = 0,
    ZR_JIT_HOST_STATUS_PENDING,
    ZR_JIT_HOST_STATUS_DISABLED,
    ZR_JIT_HOST_STATUS_ALREADY_REGISTERED,
    ZR_JIT_HOST_STATUS_NOT_REGISTERED,
    ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE,
    ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
    ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH,
    ZR_JIT_HOST_STATUS_INVALID_FLAGS,
    ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED,
    ZR_JIT_HOST_STATUS_TARGET_MISMATCH,
    ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN,
    ZR_JIT_HOST_STATUS_IMPORT_INVALID,
    ZR_JIT_HOST_STATUS_OPERATION_UNSUPPORTED,
    ZR_JIT_HOST_STATUS_STATE_MAP_INVALID,
    ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
    ZR_JIT_HOST_STATUS_WX_REQUIRED,
    ZR_JIT_HOST_STATUS_CODE_INVALID,
    ZR_JIT_HOST_STATUS_CAPACITY,
    ZR_JIT_HOST_STATUS_STALE_HANDLE,
    ZR_JIT_HOST_STATUS_ACTIVE_LEASE,
    ZR_JIT_HOST_STATUS_INVALID_STATE,
    ZR_JIT_HOST_STATUS_OVERFLOW,
    ZR_JIT_HOST_STATUS_FALLBACK_EXECBC,
    ZR_JIT_HOST_STATUS_FALLBACK_AOT,
    ZR_JIT_HOST_STATUS_COUNT
} EZrJitHostStatus;

/** @brief 与 EZrJitHostStatus 同一 ABI 类型的简写别名。 */
typedef EZrJitHostStatus EZrJitStatus;
#define ZR_JIT_STATUS_OK ZR_JIT_HOST_STATUS_OK
#define ZR_JIT_STATUS_BACKEND_UNAVAILABLE ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE
#define ZR_JIT_STATUS_INVALID_ARGUMENT ZR_JIT_HOST_STATUS_INVALID_ARGUMENT
#define ZR_JIT_STATUS_TARGET_UNSUPPORTED ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED
#define ZR_JIT_STATUS_TARGET_MISMATCH ZR_JIT_HOST_STATUS_TARGET_MISMATCH
#define ZR_JIT_STATUS_IMPORT_FORBIDDEN ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN
#define ZR_JIT_STATUS_STATE_MAP_INVALID ZR_JIT_HOST_STATUS_STATE_MAP_INVALID

/**
 * ## Summary
 * @brief 进程内 facade 单例的生命周期状态。
 *
 * ## Value model
 * REGISTERED 是成功 QueryInfo 当前唯一输出的状态；SHUTTING_DOWN 是 TryShutdown
 * 排空并拆除 manager 时在全局锁内设置的内部阶段；SHUTDOWN 是定义的终态枚举值，当前
 * 不对外返回，关闭成功后直接释放 singleton。
 *
 * ## Constraints and lifecycle
 * 该状态由 facade 持有并在全局互斥锁下读写；IsRegistered 仅判断是否已注册，
 * QueryInfo 成功时仅报告 REGISTERED。调用方不得直接构造或修改实现对象。
 *
 * ## References
 * @see ZrJit_Host_IsRegistered
 * @see ZrJit_Host_TryShutdown
 */
typedef enum EZrJitHostState {
    ZR_JIT_HOST_STATE_UNINITIALIZED = 0,
    ZR_JIT_HOST_STATE_REGISTERED,
    ZR_JIT_HOST_STATE_SHUTTING_DOWN,
    ZR_JIT_HOST_STATE_SHUTDOWN
} EZrJitHostState;

/**
 * ## Summary
 * @brief Facade 或 core 转译错误的可选值诊断。
 *
 * ## Fields
 * status 是 facade 结果；coreStatus 保留对应 core 状态，facade 自身错误时为 OK。
 * sourceIndex 指向请求中报告问题的来源编号；expected/actual 是 32 位值，
 * expectedHash/actualHash 是 64 位摘要。
 *
 * ## Constraints and lifecycle
 * 无指针且不拥有资源，可由调用方栈上分配；每个 API 在诊断非空时独立写入。
 * 不应假定每个字段在每种失败上都会填充。operation 字段当前不由 facade 写入；
 * TODO: 确认它的公开语义及应由哪些验证入口赋值，核查 ValidateCompileRequest 与 Compile。
 *
 * ## References
 * @see ZrJit_Host_DiagnosticInit
 * @see ZrJit_Host_ValidateCompileRequest
 */
typedef struct SZrJitHostDiagnostic {
    EZrJitHostStatus status; /**< 本次 facade 调用的结果。 */
    EZrHostJitStatus coreStatus; /**< 转译的 core 结果；facade 自身拒绝时为 OK。 */
    TZrUInt32 sourceIndex; /**< 失败关联的请求来源索引；未提供时保持零。 */
    TZrUInt32 operation; /**< 保留的操作诊断字段；当前实现不赋值，见结构说明中的 TODO。 */
    TZrUInt32 expected; /**< 失败相关的 32 位期望值，非 hash。 */
    TZrUInt32 actual; /**< 失败相关的 32 位实际值，非 hash。 */
    TZrUInt64 expectedHash; /**< 失败相关的 64 位期望摘要。 */
    TZrUInt64 actualHash; /**< 失败相关的 64 位实际摘要。 */
} SZrJitHostDiagnostic;

/**
 * ## Summary
 * @brief 四类状态图及其 frame layout 的进程内注册声明。
 *
 * ## Fields
 * registrationFlags 与每类 entryCount/hash 成对；当前公开校验要求 roots、
 * unwind、debug、deopt 四个位全部设置且各自 count/hash 非零。
 * frameLayoutHash 是与发布布局核对的独立摘要。
 *
 * ## Constraints and lifecycle
 * 调用方借入该值供一次校验或准备操作使用，并负责给出真实登记证据；
 * 校验器只检查字段之间的声明关系，不读取图条目或证明平台资源已注册。
 * TODO: ValidateCompileRequest 目前只把 frameLayoutHash 与 publication 对照；
 * PrepareWithMaps 还逐一核对四个 map hash。需在统一编译请求契约时确认两入口
 * 是否应接受相同的一致性条件，核查 ValidateCompileRequest 与 prepare_locked。
 *
 * ## References
 * @see ZrJit_Host_ValidateStateMaps
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_Compile
 */
typedef struct SZrJitStateMapFacts {
    TZrUInt32 schemaVersion; /**< 必须等于 ZR_JIT_HOST_SCHEMA_VERSION。 */
    TZrUInt32 registrationFlags; /**< 四种图登记位；未知位或缺少任一已知位均拒绝。 */
    TZrUInt32 rootEntryCount; /**< roots 图声明的条目数；登记时必须非零。 */
    TZrUInt32 unwindEntryCount; /**< unwind 图声明的条目数；登记时必须非零。 */
    TZrUInt32 debugEntryCount; /**< debug 图声明的条目数；登记时必须非零。 */
    TZrUInt32 deoptEntryCount; /**< deopt 图声明的条目数；登记时必须非零。 */
    TZrUInt64 rootMapHash; /**< roots 图摘要；登记时必须非零。 */
    TZrUInt64 unwindMapHash; /**< unwind 图摘要；登记时必须非零。 */
    TZrUInt64 debugMapHash; /**< debug 图摘要；登记时必须非零。 */
    TZrUInt64 deoptMapHash; /**< deopt 图摘要；登记时必须非零。 */
    TZrUInt64 frameLayoutHash; /**< 帧布局摘要；必须非零并匹配 publication.layoutHash。 */
} SZrJitStateMapFacts;

/**
 * ## Summary
 * @brief Compile 阶段消费的进程内请求快照。
 *
 * ## Fields
 * magic/schemaVersion 识别当前进程内布局；flags 与 operationMask 控制请求；
 * publication 和 stateMaps 是待核验的调用方声明；sourceIndex 用于诊断，
 * reserved 必须为零。
 *
 * ## Constraints and lifecycle
 * 该结构按值借用，不是持久化格式；ValidateCompileRequest 不分配资源。
 * 完整请求验证成功不代表机器码已产生，也不代表四个 map hash 已与 publication
 * 逐一比较；当前 Compile 只选择 fallback 或报告 provider 不可用。
 *
 * ## References
 * @see ZrJit_Host_ValidateCompileRequest
 * @see ZrJit_Host_Compile
 */
typedef struct SZrJitHostCompileRequest {
    TZrUInt32 magic; /**< 必须等于 ZR_JIT_HOST_MAGIC。 */
    TZrUInt32 schemaVersion; /**< 必须等于 ZR_JIT_HOST_SCHEMA_VERSION。 */
    TZrUInt32 flags; /**< 仅允许 ZR_JIT_HOST_OPTION_KNOWN_MASK 中的位。 */
    TZrUInt32 operationMask; /**< 非零且仅包含已知操作，并是 publication.operationMask 子集。 */
    SZrHostJitPublicationFacts publication; /**< core 发布校验所需的值声明；不转移资源。 */
    SZrJitStateMapFacts stateMaps; /**< 本次请求的四图及布局声明。 */
    TZrUInt32 sourceIndex; /**< 供 facade 诊断定位请求来源。 */
    TZrUInt32 reserved; /**< ABI 保留位；当前必须为零。 */
} SZrJitHostCompileRequest;

/**
 * ## Summary
 * @brief 注册后的能力与容量值快照。
 *
 * ## Fields
 * target、state 和 backendIdentity 描述当前单例；fallbackAvailable 与
 * machineCodeAvailable 分别表示两种执行路径；codeRecordCapacity 是 manager
 * 固定记录槽数量。supportedOperations 是 facade 声明的操作位集合。
 *
 * ## Constraints and lifecycle
 * 仅 QueryInfo 在已注册时填充；输出不拥有 singleton，也不会延长其生命周期。
 * supportedOperations 不表示 provider 已生成代码；当前 machineCodeAvailable 恒为 false。
 *
 * ## References
 * @see ZrJit_Host_QueryInfo
 * @see ZrJit_Host_IsMachineCodeAvailable
 */
typedef struct SZrJitHostBackendInfo {
    TZrUInt32 magic; /**< 输出结构识别值。 */
    TZrUInt32 schemaVersion; /**< 输出结构版本。 */
    EZrJitHostState state; /**< 查询瞬间的 facade 状态快照。 */
    TZrBool machineCodeAvailable; /**< 当前 provider 的机器码能力；当前恒为 false。 */
    TZrBool fallbackAvailable; /**< 当前注册选项是否允许 fallback。 */
    TZrUInt16 reserved; /**< 输出时清零的 ABI 保留字段。 */
    SZrHostJitTargetContract target; /**< 注册时借用值的副本。 */
    TZrUInt32 supportedOperations; /**< 契约层声明的操作集合，不等同于可执行能力。 */
    TZrUInt32 codeRecordCapacity; /**< 固定记录数组容量，不是机器码字节预算。 */
    TZrUInt64 backendIdentity; /**< 进程内目标身份摘要，不是持久化或函数地址标识。 */
} SZrJitHostBackendInfo;

/**
 * ## Summary
 * @brief 把诊断输出初始化为 facade 成功态。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空时所有字段清零，status/coreStatus 设为 OK。
 *
 * ## Returns
 * 无返回值；空指针是 no-op。
 *
 * ## Constraints and lifecycle
 * 仅清理调用方提供的值对象，不分配资源；可在下一次 API 调用前复用诊断存储。
 *
 * ## References
 * @see SZrJitHostDiagnostic
 * @see ZrJit_Host_ValidateCompileRequest
 */
ZR_API void ZrJit_Host_DiagnosticInit(SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 将 facade 状态转换为稳定的静态诊断字符串。
 *
 * ## Parameters
 * @param[in] status 任意 EZrJitHostStatus 值；未知值映射到通用名称。
 *
 * ## Returns
 * @return 返回 facade 静态存储中的只读字符串；调用方不得修改或释放，生命周期覆盖进程。
 *
 * ## Constraints and lifecycle
 * 纯值查询，不要求先注册、不读取 singleton，且可并发调用。
 *
 * ## References
 * @see EZrJitHostStatus
 */
ZR_API const TZrChar *ZrJit_Host_StatusName(EZrJitHostStatus status);
/**
 * ## Summary
 * @brief 在准备代码记录或验证编译请求前检查四类状态图声明。
 *
 * ## Parameters
 * @param[in] facts 待核对的值快照；不能为空。校验 schema、已知且完整的登记位、
 *                  每类非零条目数/hash 以及非零 frameLayoutHash。
 * @param[out] diagnostic 可空；失败时填入 facade 状态、相关字段和类别索引。
 *
 * ## Returns
 * @return 成功返回 OK；空输入、版本/flags 错误或 bit/count/hash 不自洽时返回相应拒绝状态。
 *
 * ## Constraints and lifecycle
 * 仅验证声明本身，不读取图内容、不检查平台注册状态，也不分配或取得资源；
 * diagnostic 为空时返回码仍保留具体状态，但无法返回字段细节。
 *
 * ## References
 * @see SZrJitStateMapFacts
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_ValidateCompileRequest
 */
ZR_API EZrJitHostStatus ZrJit_Host_ValidateStateMaps(
        const SZrJitStateMapFacts *facts,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 对请求执行无副作用的 Compile 前置校验。
 *
 * ## Parameters
 * @param[in] request 进程内请求快照；不能为空，flags 与 operationMask 必须合法，
 *                    publication 须通过 core 校验，stateMaps 须自洽，
 *                    frameLayoutHash 须匹配 publication.layoutHash，reserved 须为零。
 * @param[out] diagnostic 可空；非空时状态图失败保留 helper 细分状态；空时此类失败
 *                       统一返回 STATE_MAP_INVALID，无法取得类别级细节。
 *
 * ## Returns
 * @return 返回 OK 表示这些已实现的前置检查通过，不表示 provider 存在、机器码已生成，
 * 也不表示四个 map hash 已逐一与 publication 比较。
 *
 * ## Constraints and lifecycle
 * 只借用 request 至本次调用结束，不保存指针或分配资源。当前实现对 frame layout
 * 做跨结构比较；TODO: 四个 map hash 与 publication 的一致性契约尚未统一，
 * 核查本函数与内部 prepare_locked 的差异后再决定是否扩大验证。
 *
 * ## References
 * @see SZrJitHostCompileRequest
 * @see ZrJit_Host_ValidateStateMaps
 * @see ZrJit_Host_Compile
 */
ZR_API EZrJitHostStatus ZrJit_Host_ValidateCompileRequest(
        const SZrJitHostCompileRequest *request,
        SZrJitHostDiagnostic *diagnostic);

/**
 * ## Summary
 * @brief 验证配置并注册本进程唯一的 host-JIT facade。
 *
 * ## Parameters
 * @param[in] options core host-JIT 选项快照；ENABLE 须开启，目标须为当前支持的
 *                    host 架构，记录预算和导入上限须有效。内容仅在调用期间借用。
 * @param[out] diagnostic 可空的 ExecIR 诊断；失败时报告选项/目标拒绝或容量/分配错误。
 *
 * ## Returns
 * @return 成功建立 singleton 和 core manager 时返回 ZR_TRUE；无效配置、重复注册、目标不匹配
 * 或资源准备失败返回 ZR_FALSE。成功不代表 ORC provider 或机器码可用。
 *
 * ## Constraints and lifecycle
 * 注册/关闭由进程级互斥量串行化；每进程只有一个实例。调用方须保留其目标声明
 * 与配置语义，但 facade 会复制选项和固定记录数组。失败时不会发布半初始化单例；
 * 捕获到分配失败时返回容量或 backend-unavailable 诊断。
 *
 * ## References
 * @see ZrJit_Host_QueryInfo
 * @see ZrJit_Host_GetDescriptor
 * @see ZrJit_Host_TryShutdown
 */
ZR_API TZrBool ZrJit_Host_Register(const SZrHostJitOptions *options,
        SZrExecIrDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 尝试排空记录并注销 singleton。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；失败时保留 active lease、manager 状态或 core 错误。
 *
 * ## Returns
 * @return 无实例时幂等返回 OK；无记录且 manager 已解除借用时释放 singleton 并返回 OK。
 * 若仍有 lease 返回 ACTIVE_LEASE；非租用记录仍未退休/回收时返回 INVALID_STATE；
 * 其他 manager 错误保留对应 facade/core 状态。
 *
 * ## Constraints and lifecycle
 * 整个尝试在进程级互斥量下运行；会先收集可回收 retired 记录并删除对应 proof。
 * 返回失败时注册实例仍可用，调用方须先 Evict/Release/Collect 后再重试。
 *
 * ## References
 * @see ZrJit_Host_Shutdown
 * @see ZrJit_Host_Evict
 * @see ZrJit_Host_Release
 * @see ZrJit_Host_Collect
 */
ZR_API EZrJitHostStatus ZrJit_Host_TryShutdown(
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 执行一次无返回值的关闭尝试。
 *
 * ## Returns
 * 无返回值；内部调用 TryShutdown 并丢弃诊断。若仍有记录或 lease，实例会继续注册。
 *
 * ## Constraints and lifecycle
 * 不循环等待、不强制释放 lease。需要确认是否完成关闭或安排重试时应直接调用
 * TryShutdown 并读取状态码。
 *
 * ## References
 * @see ZrJit_Host_TryShutdown
 */
ZR_API void ZrJit_Host_Shutdown(void);
/**
 * ## Summary
 * @brief 查询 singleton 是否处于 REGISTERED 状态。
 *
 * ## Returns
 * @return 已注册并可接受记录操作时返回 ZR_TRUE，否则返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 查询在进程级互斥量下取瞬时快照；返回后状态仍可能被另一线程关闭。
 *
 * ## References
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_TryShutdown
 */
ZR_API TZrBool ZrJit_Host_IsRegistered(void);
/**
 * ## Summary
 * @brief 查询当前 singleton 是否报告机器码 provider 可用。
 *
 * ## Returns
 * @return 当前实现恒返回 ZR_FALSE；未注册时也返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 这是能力瞬时快照，不与 IsRegistered 等价，也不证明 fallback 路径状态。
 * 查询由进程级互斥量保护。
 *
 * ## References
 * @see ZrJit_Host_IsRegistered
 * @see ZrJit_Host_QueryInfo
 */
ZR_API TZrBool ZrJit_Host_IsMachineCodeAvailable(void);
/**
 * ## Summary
 * @brief 读取当前 singleton 的目标与能力元数据快照。
 *
 * ## Parameters
 * @param[out] info 非空输出结构；入口先清零，再于成功时写入完整快照。
 * @param[out] diagnostic 可空；未注册时保留 facade 错误。
 *
 * ## Returns
 * @return 已注册时返回 OK；空输出或未注册时返回相应错误，失败输出保持清零。
 *
 * ## Constraints and lifecycle
 * 读写由进程级互斥量串行化；返回值是拷贝，不借出 HostImpl 或延长 singleton 生命周期。
 * supportedOperations 是声明掩码，不代表有可执行机器码。
 *
 * ## References
 * @see SZrJitHostBackendInfo
 * @see ZrJit_Host_Register
 */
ZR_API EZrJitHostStatus ZrJit_Host_QueryInfo(
        SZrJitHostBackendInfo *info,
        SZrJitHostDiagnostic *diagnostic);

/**
 * ## Summary
 * @brief 复制一份 core backend service 可消费的进程内 callback descriptor。
 *
 * ## Parameters
 * @param[out] descriptor 非空输出结构；入口先清零，成功时写入目标、操作掩码和 vtable。
 * @param[out] diagnostic 可空；失败时写入 facade 错误。
 *
 * ## Returns
 * @return 已注册时返回 ZR_TRUE；空输出或未注册时返回 ZR_FALSE，失败输出为零。
 *
 * ## Constraints and lifecycle
 * descriptor 的 userData 为空，复制后不借用 HostImpl 指针；查询回调在执行时重新
 * 获取 singleton 并持锁。拷贝不负责注册到 core service，也不延长 JIT 注册状态；
 * 关闭后 queryTarget 会重查注册状态并返回不可用，其他回调依各自参数和 stub 规则返回；
 * 任何回调均不提供机器码地址。
 *
 * ## References
 * @see SZrExecutionBackendDescriptor
 * @see ZrJit_Host_TryShutdown
 * @see query_target
 */
ZR_API TZrBool ZrJit_Host_GetDescriptor(
        SZrExecutionBackendDescriptor *descriptor,
        SZrJitHostDiagnostic *diagnostic);

/**
 * ## Summary
 * @brief 按符号 ID 与签名摘要验证一次导入白名单查询。
 *
 * ## Parameters
 * @param[in] manifest 调用期借用的 core manifest 与数组；不能为空且须符合 core schema。
 * @param[in] symbolId 要查找的符号身份。
 * @param[in] signatureHash 该符号的期望签名摘要，须精确匹配。
 * @param[out] diagnostic 可空；失败时包含 core/facade 状态和已知差异值。
 *
 * ## Returns
 * @return 白名单中存在 ID 且签名相符时返回 OK；缺失、重复或签名错误按 core 结果拒绝；
 * 已注册时还会检查 manifest.count 不超过 maxImports。
 *
 * ## Constraints and lifecycle
 * 只做只读验证，不解析符号地址、不返回 callable pointer，也不保存 manifest。
 * 条数上限仅在当前 singleton 存在时应用；调用方仍须保持输入在调用期间有效。
 *
 * ## References
 * @see ZrCore_HostJit_ValidateImports
 * @see ZrJit_Host_Register
 */
ZR_API EZrJitHostStatus ZrJit_Host_ValidateImport(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrJitHostDiagnostic *diagnostic);

/**
 * ## Summary
 * @brief 为生命周期验证合成四图声明并准备 core 记录。
 *
 * ## Parameters
 * @param[in] facts core publication 声明；本次调用借用，不转移所有权。
 * @param[out] handle 独立的空输出句柄；不得覆盖仍须 Release 的 lease。
 * @param[out] diagnostic 可空；失败时报告验证、容量或生命周期错误。
 *
 * ## Returns
 * @return core manager 成功准备时返回 OK 并交付未租用的 prepared handle；失败时返回具体状态，
 * 输出不代表有效的新准备句柄。
 *
 * ## Constraints and lifecycle
 * 此便捷入口仅从 facts 合成计数为 1 的图声明，不读取真实图条目；使用实际图证据时
 * 调用 PrepareWithMaps。Prepare 的输出槽须为空，复用前调用方须先保存并 Release 旧 lease。
 * 全仓暂无此便捷入口的直接调用方。
 *
 * ## References
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_Publish
 * @see SZrHostJitCodeHandle
 */
ZR_API EZrJitHostStatus ZrJit_Host_Prepare(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 校验调用方图声明后在 core manager 中准备一条未租用记录。
 *
 * ## Parameters
 * @param[in] facts publication 的目标、身份和摘要声明；调用期间只读借用。
 * @param[in] stateMaps 四类图登记声明；字段必须完整，frame layout 及四个图摘要须与 facts 匹配。
 * @param[out] handle 独立空槽；不得覆盖 active lease 或其他仍需 Release 的句柄。
 * @param[out] diagnostic 可空；失败时写入 facade/core 拒绝原因。
 *
 * ## Returns
 * @return 成功返回 OK 并写入 core prepared handle 与内部 proof；失败返回具体状态且不承诺产生新记录。
 *
 * ## Constraints and lifecycle
 * 调用需先 Register；manager 与 proof 操作在进程级互斥量下串行化。输出 handle 不是
 * lease；发布、退休、收集与关闭分别使用配套 API。输出槽不得覆盖旧 lease，复用前先
 * 保存可 Release 句柄并归还它。
 *
 * ## References
 * @see ZrJit_Host_ValidateStateMaps
 * @see ZrJit_Host_Publish
 * @see ZrJit_Host_Release
 * @see ZrCore_HostJit_Code_Prepare
 */
ZR_API EZrJitHostStatus ZrJit_Host_PrepareWithMaps(
        const SZrHostJitPublicationFacts *facts,
        const SZrJitStateMapFacts *stateMaps,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 将 facade 已登记 proof 的 prepared 记录发布为 active。
 *
 * ## Parameters
 * @param[in] handle Prepare 返回的未租用句柄；仅借用并由 core manager 核对身份。
 * @param[out] diagnostic 可空；失败时保留 core/facade 状态。
 *
 * ## Returns
 * @return proof、目标与 core 发布校验通过时返回 OK；不匹配或句柄过期时返回拒绝状态。
 *
 * ## Constraints and lifecycle
 * 已发布记录仍不是可执行代码，也不会自动取得 lease。调用期间持有全局锁；
 * prepared handle 保持由调用方管理，Evict/Collect 决定其记录何时回收。
 *
 * ## References
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_Acquire
 * @see ZrJit_Host_Evict
 */
ZR_API EZrJitHostStatus ZrJit_Host_Publish(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 为当前 active 记录取得一份必须配对归还的 lease。
 *
 * ## Parameters
 * @param[out] handle 空的、独立输出句柄；不得覆盖已有 lease。失败时保留原值。
 * @param[out] diagnostic 可空；失败时报告未注册、无 active 记录或 core 状态。
 *
 * ## Returns
 * @return 成功时输出拥有一份 lease 的 handle 并返回 OK；失败时没有取得新 lease。
 *
 * ## Constraints and lifecycle
 * 每次成功 Acquire 只增加一份 core lease；句柄不可复制为额外拥有权，使用完必须
 * 调 Release。退休可以与已有 lease 并存，记录存储需一直保留到 lease 归还。
 * facade 不检测输出是否已有 lease；这是调用者必须遵守的 core manager 前置条件。
 *
 * ## References
 * @see ZrCore_HostJit_Code_AcquireActive
 * @see ZrJit_Host_Release
 * @see ZrJit_Host_Evict
 */
ZR_API EZrJitHostStatus ZrJit_Host_Acquire(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 归还传入句柄代表的那一份 lease。
 *
 * ## Parameters
 * @param[in,out] handle 由成功 Acquire 取得的唯一拥有句柄；仅在 core 成功时清空。
 * @param[out] diagnostic 可空；失败时保留错误，handle 仍由调用方持有。
 *
 * ## Returns
 * @return 成功归还返回 OK；无效、已释放或过期句柄返回对应 core/facade 错误。
 *
 * ## Constraints and lifecycle
 * 成功后不得再次使用该 lease；Release 本身不收集 retired 记录。归还后调用方可
 * 显式 Collect，或重试 TryShutdown。
 *
 * ## References
 * @see ZrJit_Host_Acquire
 * @see ZrJit_Host_Collect
 * @see ZrJit_Host_TryShutdown
 */
ZR_API EZrJitHostStatus ZrJit_Host_Release(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 按 codeIdentity 退休记录并阻止后续 Acquire。
 *
 * ## Parameters
 * @param[in] codeIdentity Prepare 时提供且由 core manager 管理的记录身份。
 * @param[out] diagnostic 可空；失败时报告无效状态或找不到记录。
 *
 * ## Returns
 * @return 找到可退休记录时返回 OK；身份未知或 core manager 拒绝时返回具体错误。
 *
 * ## Constraints and lifecycle
 * retirement 不减少既有 lease，也不立即清除 facade proof；有 lease 时必须等 Release
 * 后由 Collect 回收。操作由进程级互斥量保护。
 *
 * ## References
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_Release
 * @see ZrJit_Host_Collect
 */
ZR_API EZrJitHostStatus ZrJit_Host_Evict(
        TZrUInt64 codeIdentity,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 收集没有 lease 的 retired 记录并同步删除 facade proof。
 *
 * ## Parameters
 * @param[out] outCollected 非空计数输出；入口先置零，成功后表示本次 core 清零的记录数。
 * @param[out] diagnostic 可空；core 失败时保留其拒绝诊断。
 *
 * ## Returns
 * @return manager 注册且操作成功时返回 OK；空输出、未注册或 core 校验失败时返回错误。
 *
 * ## Constraints and lifecycle
 * 仅 retired 且 leaseCount 为零的记录可回收；对应 proof 在 core 成功后按剩余 identity
 * 清理。该数量不表示机器码资源释放，因为当前 facade 没有执行内存。
 *
 * ## References
 * @see ZrJit_Host_Evict
 * @see ZrJit_Host_Release
 * @see ZrJit_Host_TryShutdown
 */
ZR_API EZrJitHostStatus ZrJit_Host_Collect(
        TZrUInt32 *outCollected,
        SZrJitHostDiagnostic *diagnostic);
/**
 * ## Summary
 * @brief 按已租用记录查询执行入口地址。
 *
 * ## Parameters
 * @param[in] handle 由 facade manager 管理的有效记录句柄；调用期间只读借用。
 * @param[out] entryAddress 非空原生指针输出；进入时先置零。
 * @param[out] diagnostic 可空；失败时给出参数或 provider 状态。
 *
 * ## Returns
 * @return 当前没有 ORC provider；非空参数时返回 BACKEND_UNAVAILABLE 且地址为零。
 * 空 handle 或地址输出返回 INVALID_ARGUMENT；此 stub 不解析非空 handle 的内容。
 *
 * ## Constraints and lifecycle
 * 当前实现从不返回可执行地址；不得将零值转型或执行。未来 provider 必须另行核对
 * lease 生命周期与地址失效条件。
 *
 * ## References
 * @see ZrJit_Host_Acquire
 * @see ZrJit_Host_Release
 * @see SZrHostJitCodeHandle
 */
ZR_API EZrJitHostStatus ZrJit_Host_LookupEntry(
        const SZrHostJitCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrJitHostDiagnostic *diagnostic);

/**
 * ## Summary
 * @brief 验证 Compile 请求，并选择已允许的 fallback 或报告 provider 不可用。
 *
 * ## Parameters
 * @param[in] request 当前 schema 的进程内请求；调用期间只读借用。
 * @param[out] outHandle 调用时会清零的输出槽；不得传入仍须 Release 的旧 lease。
 * @param[out] diagnostic 可空；失败时保留请求校验、注册目标或 core 错误。
 *
 * ## Returns
 * @return 请求有效、目标与注册配置匹配且允许 fallback 时返回 FALLBACK_EXECBC；
 * 要求机器码而 provider 不可用时返回 BACKEND_UNAVAILABLE；其他情况返回拒绝状态。
 *
 * ## Constraints and lifecycle
 * 当前不会创建记录、机器码、入口地址或异步工作。outHandle 即使成功也保持空值；
 * 入口无条件清零，因此调用方须用空的独立输出槽，先归还其他 lease。全局单例比较
 * 在互斥量下执行；ValidateCompileRequest 可独立调用且不取该锁。
 *
 * ## References
 * @see ZrJit_Host_ValidateCompileRequest
 * @see ZrJit_Host_IsMachineCodeAvailable
 * @see ZrJit_Host_Release
 */
ZR_API EZrJitHostStatus ZrJit_Host_Compile(
        const SZrJitHostCompileRequest *request,
        SZrHostJitCodeHandle *outHandle,
        SZrJitHostDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_JIT_BACKEND_H */
