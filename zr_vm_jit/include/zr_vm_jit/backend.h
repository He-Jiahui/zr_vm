#ifndef ZR_VM_JIT_BACKEND_H
#define ZR_VM_JIT_BACKEND_H

/*
 * Optional host baseline-JIT facade.
 *
 * The implementation is C++ so that an LLVM ORC/JITLink provider can be
 * plugged in without making the core C11 target depend on LLVM.  Everything
 * crossing this header is a value contract or a process-local opaque handle:
 * executable addresses are never persisted in an artifact, a binding row, or
 * a cache key.  A build without an ORC provider remains usable and reports an
 * explicit BACKEND_UNAVAILABLE/fallback result.
 */

#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_core/execution_backend.h"
#include "zr_vm_core/host_baseline_jit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 请求、状态图及信息结构共享版本；magic 只识别进程内请求，不是磁盘格式。 */
#define ZR_JIT_HOST_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_JIT_HOST_MAGIC ((TZrUInt32)0x314a485au) /* ZHJ1 */

/* 请求级 fallback 选择必须与注册时的全局选项同时允许。 */
#define ZR_JIT_HOST_OPTION_ALLOW_FALLBACK ((TZrUInt32)1u << 0u)
#define ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE ((TZrUInt32)1u << 1u)
#define ZR_JIT_HOST_OPTION_KNOWN_MASK \
    (ZR_JIT_HOST_OPTION_ALLOW_FALLBACK | ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE)

/* 四类注册声明必须同时出现；单独的 hash 不能作为完整登记声明。 */
#define ZR_JIT_HOST_MAP_ROOTS ((TZrUInt32)1u << 0u)
#define ZR_JIT_HOST_MAP_UNWIND ((TZrUInt32)1u << 1u)
#define ZR_JIT_HOST_MAP_DEBUG ((TZrUInt32)1u << 2u)
#define ZR_JIT_HOST_MAP_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_JIT_HOST_MAP_KNOWN_MASK \
    (ZR_JIT_HOST_MAP_ROOTS | ZR_JIT_HOST_MAP_UNWIND | \
     ZR_JIT_HOST_MAP_DEBUG | ZR_JIT_HOST_MAP_DEOPT)

/** @brief 区分契约拒绝、机器码不可用及向上层建议 fallback 的结果。 */
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

/* Short aliases make the optional facade easy to consume from adapters that
 * already use the plan's EZrJitStatus spelling. */
typedef EZrJitHostStatus EZrJitStatus;
#define ZR_JIT_STATUS_OK ZR_JIT_HOST_STATUS_OK
#define ZR_JIT_STATUS_BACKEND_UNAVAILABLE ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE
#define ZR_JIT_STATUS_INVALID_ARGUMENT ZR_JIT_HOST_STATUS_INVALID_ARGUMENT
#define ZR_JIT_STATUS_TARGET_UNSUPPORTED ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED
#define ZR_JIT_STATUS_TARGET_MISMATCH ZR_JIT_HOST_STATUS_TARGET_MISMATCH
#define ZR_JIT_STATUS_IMPORT_FORBIDDEN ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN
#define ZR_JIT_STATUS_STATE_MAP_INVALID ZR_JIT_HOST_STATUS_STATE_MAP_INVALID

/** @brief 单例适配器的生命周期；只有 REGISTERED 接受代码记录操作。 */
typedef enum EZrJitHostState {
    ZR_JIT_HOST_STATE_UNINITIALIZED = 0,
    ZR_JIT_HOST_STATE_REGISTERED,
    ZR_JIT_HOST_STATE_SHUTTING_DOWN,
    ZR_JIT_HOST_STATE_SHUTDOWN
} EZrJitHostState;

/** @brief 将 facade 与 core 的错误及期望值保存在无指针诊断中。 */
typedef struct SZrJitHostDiagnostic {
    EZrJitHostStatus status;
    EZrHostJitStatus coreStatus;
    TZrUInt32 sourceIndex;
    TZrUInt32 operation;
    TZrUInt32 expected;
    TZrUInt32 actual;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrJitHostDiagnostic;

/** @brief 根、展开、调试和反优化图的进程内注册声明。
 * @note 每类 bit、条目数及 hash 必须一致；调用方负责提供真实登记数据，
 * frameLayoutHash 还须与发布事实匹配。
 */
typedef struct SZrJitStateMapFacts {
    TZrUInt32 schemaVersion;
    TZrUInt32 registrationFlags;
    TZrUInt32 rootEntryCount;
    TZrUInt32 unwindEntryCount;
    TZrUInt32 debugEntryCount;
    TZrUInt32 deoptEntryCount;
    TZrUInt64 rootMapHash;
    TZrUInt64 unwindMapHash;
    TZrUInt64 debugMapHash;
    TZrUInt64 deoptMapHash;
    TZrUInt64 frameLayoutHash;
} SZrJitStateMapFacts;

/** @brief 编译前的不可持久化请求快照；publication/stateMaps 是待核验的声明。 */
typedef struct SZrJitHostCompileRequest {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 operationMask;
    SZrHostJitPublicationFacts publication;
    SZrJitStateMapFacts stateMaps;
    TZrUInt32 sourceIndex;
    TZrUInt32 reserved;
} SZrJitHostCompileRequest;

/** @brief 注册后查询到的能力快照；supportedOperations 不表示已有机器码 provider。 */
typedef struct SZrJitHostBackendInfo {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrJitHostState state;
    TZrBool machineCodeAvailable;
    TZrBool fallbackAvailable;
    TZrUInt16 reserved;
    SZrHostJitTargetContract target;
    TZrUInt32 supportedOperations;
    TZrUInt32 codeRecordCapacity;
    TZrUInt64 backendIdentity;
} SZrJitHostBackendInfo;

/** @brief 为下一次公开调用初始化可选诊断输出；允许传入空指针。 */
ZR_API void ZrJit_Host_DiagnosticInit(SZrJitHostDiagnostic *diagnostic);
/** @brief 将 facade 状态映射为稳定的诊断名称，不依赖注册状态。 */
ZR_API const TZrChar *ZrJit_Host_StatusName(EZrJitHostStatus status);
/** @brief 在准备或编译前核对四类状态图声明的完整性。 */
ZR_API EZrJitHostStatus ZrJit_Host_ValidateStateMaps(
        const SZrJitStateMapFacts *facts,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 校验请求版本、操作集合及发布事实，供 Compile 的失败前置检查使用。 */
ZR_API EZrJitHostStatus ZrJit_Host_ValidateCompileRequest(
        const SZrJitHostCompileRequest *request,
        SZrJitHostDiagnostic *diagnostic);

/** @brief 注册单例 facade，验证 host target 并预留 core 代码记录。
 * @pre options 的 ENABLE 位须开启；重复注册返回 false。
 * @note 注册成功不表示 LLVM/ORC 可用，失败原因写入可选的执行诊断。
 */
ZR_API TZrBool ZrJit_Host_Register(const SZrHostJitOptions *options,
        SZrExecIrDiagnostic *diagnostic);
/** @brief 无存活记录或 lease 时注销；ACTIVE_LEASE/INVALID_STATE 可重试。 */
ZR_API EZrJitHostStatus ZrJit_Host_TryShutdown(
        SZrJitHostDiagnostic *diagnostic);
/** @brief 无返回值的清理入口；需观察失败原因时调用 TryShutdown。 */
ZR_API void ZrJit_Host_Shutdown(void);
/** @brief 查询单例是否可接受代码记录操作。 */
ZR_API TZrBool ZrJit_Host_IsRegistered(void);
/** @brief 查询真实机器码 provider 能力；当前实现恒为 false。 */
ZR_API TZrBool ZrJit_Host_IsMachineCodeAvailable(void);
/** @brief 获取目标、fallback 和容量快照；须先成功 Register。 */
ZR_API EZrJitHostStatus ZrJit_Host_QueryInfo(
        SZrJitHostBackendInfo *info,
        SZrJitHostDiagnostic *diagnostic);

/** @brief 提供可复制到 core backend service 的进程内回调描述符。
 * @note userData 为空；目标查询在锁内重查单例，注销后不悬挂 HostImpl 指针。
 */
ZR_API TZrBool ZrJit_Host_GetDescriptor(
        SZrExecutionBackendDescriptor *descriptor,
        SZrJitHostDiagnostic *diagnostic);

/** @brief 将运行时或 native 符号限制在 manifest 的 ID 与签名白名单内。 */
ZR_API EZrJitHostStatus ZrJit_Host_ValidateImport(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrJitHostDiagnostic *diagnostic);

/** @brief 从发布事实推导状态图摘要并准备代码记录；只供契约/生命周期验证。
 * @note 此入口不注册真实图条目；需调用方独立声明时使用 PrepareWithMaps。
 */
ZR_API EZrJitHostStatus ZrJit_Host_Prepare(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 对照发布 hash 与调用方的状态图声明后在 core manager 中准备记录。 */
ZR_API EZrJitHostStatus ZrJit_Host_PrepareWithMaps(
        const SZrHostJitPublicationFacts *facts,
        const SZrJitStateMapFacts *stateMaps,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 发布已准备的记录；发布成功仍不提供可执行入口地址。 */
ZR_API EZrJitHostStatus ZrJit_Host_Publish(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 对当前发布记录取得 lease；调用方须配对 Release。 */
ZR_API EZrJitHostStatus ZrJit_Host_Acquire(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 归还 lease，使退休记录可由 Collect 回收。 */
ZR_API EZrJitHostStatus ZrJit_Host_Release(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 将指定 identity 的记录退休；活跃 lease 不会立即失效。 */
ZR_API EZrJitHostStatus ZrJit_Host_Evict(
        TZrUInt64 codeIdentity,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 回收无 lease 的退休记录并同步清除 facade 的发布证明。 */
ZR_API EZrJitHostStatus ZrJit_Host_Collect(
        TZrUInt32 *outCollected,
        SZrJitHostDiagnostic *diagnostic);
/** @brief 入口地址查询；当前无 ORC provider，输出置零并返回不可用。 */
ZR_API EZrJitHostStatus ZrJit_Host_LookupEntry(
        const SZrHostJitCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrJitHostDiagnostic *diagnostic);

/** @brief 校验请求与注册目标后选择 fallback 或报告机器码不可用。
 * @note 当前不会创建代码 handle，也不会制造函数指针。
 */
ZR_API EZrJitHostStatus ZrJit_Host_Compile(
        const SZrJitHostCompileRequest *request,
        SZrHostJitCodeHandle *outHandle,
        SZrJitHostDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_JIT_BACKEND_H */
