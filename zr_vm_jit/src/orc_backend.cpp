/**
 * @file
 * @brief JIT C facade、core record manager 适配及 backend-service callbacks。
 *
 * ## Summary
 * 该翻译单元在进程内维护一个 HostImpl：复制注册选项、固定 core 记录数组和 publication
 * proof；core manager 借用数组并负责记录状态/lease 计数。公开 API 负责串行生命周期，
 * descriptor callbacks 则满足 core service vtable，同时明确报告当前无机器码 provider。
 *
 * ## Constraints and lifecycle
 * global_mutex 覆盖 singleton、manager 和 proof 的所有共享访问；持锁取得的 HostImpl
 * 指针不能越锁保存。成功 Acquire 的 lease 必须 Release，退休记录须 Collect 后才能
 * Shutdown。proof 与 core record 共同存活，但不代表存在机器码或平台状态图。
 *
 * ## References
 * @see zr_vm_jit/include/zr_vm_jit/backend.h
 * @see zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_TryShutdown
 */
#include "zr_vm_jit/backend.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace {

/* core backend service 可在复制 descriptor 后回调本文件；需要单例的回调
 * 在锁内重新取得当前状态，其余空实现不保留 userData 指针。 */
EZrExecutionBackendStatus query_target(
        const SZrExecutionBackendTarget *target,
        TZrUInt32 requiredOperations,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus compile_async(
        const SZrExecutionBackendCompileInvocation *invocation,
        TZrPtr userData,
        SZrExecutionBackendCompiledCode *completedCode,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus cancel_compile(
        const SZrExecutionCompileTicket *ticket,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus lookup_entry(
        const SZrExecutionBackendCodeInfo *code,
        TZrNativePtr *entryAddress,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus query_map(
        const SZrExecutionBackendCodeInfo *code,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus unregister_maps(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
EZrExecutionBackendStatus retire_code(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic);
void destroy_backend(TZrPtr userData);

/**
 * ## Summary
 * Facade 在 Prepare 成功后保存的发布证明，供 Publish 前复核 core record 未覆盖的声明。
 *
 * ## Fields
 * codeIdentity 与 core manager 槽关联；publication hash/flags 和 operationMask
 * 缓存准备时输入；stateMaps 保存四类图声明，供后续发布路径核对。
 *
 * ## Constraints and lifecycle
 * HostImpl 独占 proof vector，所有读写受 global_mutex 保护。该对象不拥有实际图、
 * 可执行地址或额外 lease；core 收集相应 record 后 proof 一并删除。
 *
 * ## References
 * @see HostImpl
 * @see validate_publication_proof
 * @see prepare_locked
 * @see erase_collected_proofs
 */
struct SZrJitPublicationProof {
    TZrUInt64 codeIdentity; /**< 对应 core manager 记录的 identity。 */
    TZrUInt64 signatureHash; /**< Prepare 时核对的签名摘要。 */
    TZrUInt64 layoutHash; /**< publication 的布局摘要。 */
    TZrUInt64 abiHash; /**< publication 的 ABI 摘要。 */
    TZrUInt32 publicationFlags; /**< core publication 的 W^X/机器码声明位。 */
    TZrUInt32 registrationFlags; /**< 四类图注册声明位。 */
    TZrUInt32 operationMask; /**< publication 声明支持的操作集合。 */
    SZrJitStateMapFacts stateMaps; /**< PrepareWithMaps 提交的值副本，不是实际图存储。 */
};

/**
 * ## Summary
 * 进程唯一的 facade 实现对象，拥有配置快照、记录/证明 vector 和 core manager 外壳。
 *
 * ## Fields
 * manager 借用 records.data()；proofs 与记录按 codeIdentity 关联。
 * state 控制公开入口是否可操作；当前 machineCodeAvailable 固定 false，
 * fallbackAvailable 来自注册 options；backendIdentity 是进程内标量摘要。
 *
 * ## Constraints and lifecycle
 * 全部字段由 global_mutex 保护；manager 不拥有 records 存储。
 * Shutdown 前必须先排空/收集 records、确认无 lease，再 Deinit 解除借用，
 * 最后才能析构 vectors。调用方不得持有 HostImpl 裸指针越过锁。
 *
 * ## References
 * @see global_mutex
 * @see global_impl
 * @see require_impl
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_TryShutdown
 */
struct HostImpl {
    SZrHostJitOptions options{}; /**< Register 时复制的配置；内部不借用调用方内存。 */
    std::vector<SZrHostJitCodeRecord> records; /**< manager 借用其固定 data() 地址。 */
    std::vector<SZrJitPublicationProof> proofs; /**< 与未收集记录关联的 facade 证明。 */
    SZrHostJitCodeManager manager{}; /**< 只管理 records 中的 core 元数据与 lease 计数。 */
    EZrJitHostState state = ZR_JIT_HOST_STATE_UNINITIALIZED; /**< 锁保护的生命周期状态。 */
    TZrBool machineCodeAvailable = ZR_FALSE; /**< 当前无 provider，保持 false。 */
    TZrBool fallbackAvailable = ZR_FALSE; /**< 从注册配置复制的 ExecBC fallback 能力。 */
    TZrUInt64 backendIdentity = 0u; /**< 由目标/profile 派生的非零进程内身份值。 */
};

/**
 * ## Summary
 * 返回保护 JIT singleton、manager 和 proof 容器的唯一进程级互斥量。
 *
 * ## Returns
 * @return 返回函数静态存储的 mutex 借用引用；调用方不拥有且不得销毁它。
 *
 * ## Constraints and lifecycle
 * 共享状态访问者必须在持锁期间完成读取或变更；锁采用非递归 std::mutex，
 * 不得在已持锁路径再次调用会取同一锁的公开 API。
 *
 * ## References
 * @see global_impl
 * @see require_impl
 * @see fill_descriptor
 * @see ZrJit_Host_Register
 */
std::mutex &global_mutex() {
    static std::mutex mutex;
    return mutex;
}

/**
 * ## Summary
 * 返回保存可选 JIT facade singleton 的唯一 owning slot。
 *
 * ## Returns
 * @return 返回函数静态 std::unique_ptr 的可变引用；生命周期覆盖进程。
 *
 * ## Constraints and lifecycle
 * 所有读写必须先持 global_mutex。Register 在全部初始化成功后发布对象；
 * TryShutdown 只有在 manager 记录/lease 排空并 Deinit 后才 reset。
 *
 * ## References
 * @see global_mutex
 * @see require_impl
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_TryShutdown
 */
std::unique_ptr<HostImpl> &global_impl() {
    static std::unique_ptr<HostImpl> impl;
    return impl;
}

/**
 * ## Summary
 * 清理 Register 使用的 core ExecIR 诊断对象。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空时归零并将 code 设为 NONE。
 *
 * ## Returns
 * 无返回值；空指针不产生副作用。
 *
 * ## Constraints and lifecycle
 * 只初始化调用方值对象，不拥有或分配诊断存储；其协议与其余 JIT diagnostic 不同。
 *
 * ## References
 * @see set_exec_diagnostic_for_host
 * @see ZrJit_Host_Register
 */
void clear_execution_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_NONE;
    }
}

/**
 * ## Summary
 * 将可选 facade 诊断重置为成功态，供入口和成功转换路径复用。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空时归零所有字段并将 facade/core status 置为 OK。
 *
 * ## Returns
 * 无返回值；空诊断是 no-op。
 *
 * ## Constraints and lifecycle
 * 只写本次调用者的诊断对象；不触碰共享状态。
 *
 * ## References
 * @see fail_jit_with_core
 * @see core_operation
 * @see ZrJit_Host_DiagnosticInit
 */
void clear_jit_diagnostic(SZrJitHostDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_JIT_HOST_STATUS_OK;
        diagnostic->coreStatus = ZR_HOST_JIT_STATUS_OK;
    }
}

/**
 * ## Summary
 * 同时写入 facade 状态、原始 core 状态及可选差异上下文。
 *
 * ## Parameters
 * @param[out] diagnostic 可空的 facade 诊断。
 * @param[in] status 向 facade 调用方返回的分类。
 * @param[in] coreStatus 触发映射的原始 core 状态；facade 自身错误使用 core OK。
 * @param[in] expected 32 位期望字段。
 * @param[in] actual 32 位实际字段。
 * @param[in] expectedHash 64 位期望摘要。
 * @param[in] actualHash 64 位实际摘要。
 * @param[in] sourceIndex core 或 facade 报告的来源编号。
 *
 * ## Returns
 * @return 原样返回 @p status；诊断为空时仍返回该状态。
 *
 * ## Constraints and lifecycle
 * 不清理诊断中其他预先存在字段；公开调用方通常先经 clear_jit_diagnostic。
 *
 * ## References
 * @see fail_jit
 * @see map_core_status
 */
EZrJitHostStatus fail_jit_with_core(SZrJitHostDiagnostic *diagnostic,
                                    EZrJitHostStatus status,
                                    EZrHostJitStatus coreStatus,
                                    TZrUInt32 expected = 0u,
                          TZrUInt32 actual = 0u,
                          TZrUInt64 expectedHash = 0u,
                          TZrUInt64 actualHash = 0u,
                          TZrUInt32 sourceIndex = 0u) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->coreStatus = coreStatus;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
        diagnostic->expectedHash = expectedHash;
        diagnostic->actualHash = actualHash;
        diagnostic->sourceIndex = sourceIndex;
    }
    return status;
}

/**
 * ## Summary
 * 记录由 facade 自己发现的失败，并将 coreStatus 保持为 OK。
 *
 * ## Parameters
 * @param[out] diagnostic 可空的调用方诊断。
 * @param[in] status facade 的失败状态。
 * @param[in] expected 32 位期望字段。
 * @param[in] actual 32 位实际字段。
 * @param[in] expectedHash 64 位期望摘要。
 * @param[in] actualHash 64 位实际摘要。
 * @param[in] sourceIndex 可选来源索引。
 *
 * ## Returns
 * @return 返回 facade status；诊断非空时经 fail_jit_with_core 写入。
 *
 * ## Constraints and lifecycle
 * 只写诊断，不更改 HostImpl 或 core manager。
 *
 * ## References
 * @see fail_jit_with_core
 * @see prepare_locked
 */
EZrJitHostStatus fail_jit(SZrJitHostDiagnostic *diagnostic,
                          EZrJitHostStatus status,
                          TZrUInt32 expected = 0u,
                          TZrUInt32 actual = 0u,
                          TZrUInt64 expectedHash = 0u,
                          TZrUInt64 actualHash = 0u,
                          TZrUInt32 sourceIndex = 0u) {
    return fail_jit_with_core(diagnostic, status, ZR_HOST_JIT_STATUS_OK,
                              expected, actual, expectedHash, actualHash,
                              sourceIndex);
}

/**
 * ## Summary
 * 把 core manager/validator 状态映射到 facade 枚举，并保留原 core 状态和差异。
 *
 * ## Parameters
 * @param[in] status core 返回状态。
 * @param[out] diagnostic 可空 facade 输出。
 * @param[in] coreDiagnostic 可空的 core 差异快照；不转移所有权。
 *
 * ## Returns
 * @return 成功映射 OK 为 facade OK；已知错误使用对应 facade 值；未显式列出的 core
 * 状态当前回落到 INVALID_STATE，并仍保留 coreStatus。
 *
 * ## Constraints and lifecycle
 * 不调用 core、不修改 manager。TODO: ABI_MISMATCH/LAYOUT_MISMATCH 当前落到
 * INVALID_STATE；需核对 Register / ValidateCompileRequest 的可达输入及稳定诊断分类，
 * 再决定是否增加 facade 状态映射。
 *
 * ## References
 * @see fail_jit_with_core
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_ValidateCompileRequest
 */
EZrJitHostStatus map_core_status(EZrHostJitStatus status,
                                  SZrJitHostDiagnostic *diagnostic,
                                  const SZrHostJitDiagnostic *coreDiagnostic) {
    if (status == ZR_HOST_JIT_STATUS_OK) {
        clear_jit_diagnostic(diagnostic);
        return ZR_JIT_HOST_STATUS_OK;
    }
    TZrUInt32 expected = 0u;
    TZrUInt32 actual = 0u;
    TZrUInt64 expectedHash = 0u;
    TZrUInt64 actualHash = 0u;
    TZrUInt32 sourceIndex = 0u;
    if (coreDiagnostic != ZR_NULL) {
        expected = coreDiagnostic->expected;
        actual = coreDiagnostic->actual;
        expectedHash = coreDiagnostic->expectedHash;
        actualHash = coreDiagnostic->actualHash;
        sourceIndex = coreDiagnostic->sourceIndex;
    }
    EZrJitHostStatus mapped = ZR_JIT_HOST_STATUS_INVALID_STATE;
    switch (status) {
        case ZR_HOST_JIT_STATUS_INVALID_ARGUMENT:
            mapped = ZR_JIT_HOST_STATUS_INVALID_ARGUMENT;
            break;
        case ZR_HOST_JIT_STATUS_SCHEMA_MISMATCH:
            mapped = ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH;
            break;
        case ZR_HOST_JIT_STATUS_TARGET_UNSUPPORTED:
            mapped = ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED;
            break;
        case ZR_HOST_JIT_STATUS_TARGET_MISMATCH:
            mapped = ZR_JIT_HOST_STATUS_TARGET_MISMATCH;
            break;
        case ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN:
            mapped = ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN;
            break;
        case ZR_HOST_JIT_STATUS_IMPORT_INVALID:
            mapped = ZR_JIT_HOST_STATUS_IMPORT_INVALID;
            break;
        case ZR_HOST_JIT_STATUS_OPERATION_UNSUPPORTED:
            mapped = ZR_JIT_HOST_STATUS_OPERATION_UNSUPPORTED;
            break;
        case ZR_HOST_JIT_STATUS_REGISTRATION_INCOMPLETE:
            mapped = ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE;
            break;
        case ZR_HOST_JIT_STATUS_WX_REQUIRED:
            mapped = ZR_JIT_HOST_STATUS_WX_REQUIRED;
            break;
        case ZR_HOST_JIT_STATUS_CODE_INVALID:
            mapped = ZR_JIT_HOST_STATUS_CODE_INVALID;
            break;
        case ZR_HOST_JIT_STATUS_CAPACITY:
            mapped = ZR_JIT_HOST_STATUS_CAPACITY;
            break;
        case ZR_HOST_JIT_STATUS_STALE_HANDLE:
            mapped = ZR_JIT_HOST_STATUS_STALE_HANDLE;
            break;
        case ZR_HOST_JIT_STATUS_ACTIVE_LEASE:
            mapped = ZR_JIT_HOST_STATUS_ACTIVE_LEASE;
            break;
        case ZR_HOST_JIT_STATUS_OVERFLOW:
            mapped = ZR_JIT_HOST_STATUS_OVERFLOW;
            break;
        case ZR_HOST_JIT_STATUS_INVALID_FLAGS:
            mapped = ZR_JIT_HOST_STATUS_INVALID_FLAGS;
            break;
        case ZR_HOST_JIT_STATUS_NOT_PREPARED:
            mapped = ZR_JIT_HOST_STATUS_INVALID_STATE;
            break;
        case ZR_HOST_JIT_STATUS_INVALID_STATE:
            mapped = ZR_JIT_HOST_STATUS_INVALID_STATE;
            break;
        case ZR_HOST_JIT_STATUS_OK:
        default:
            mapped = ZR_JIT_HOST_STATUS_INVALID_STATE;
            break;
    }
    return fail_jit_with_core(diagnostic, mapped, status, expected, actual,
                              expectedHash, actualHash, sourceIndex);
}

/**
 * ## Summary
 * 在已持有 global_mutex 的前提下取得当前 REGISTERED 实例。
 *
 * ## Parameters
 * @param[out] out 可空；成功时接收 HostImpl 借用指针，失败时清为 null。
 * @param[out] diagnostic 可空；实例不存在或非 REGISTERED 时写 NOT_REGISTERED。
 *
 * ## Returns
 * @return 当前单例可用时返回 OK，否则返回 NOT_REGISTERED。
 *
 * ## Constraints and lifecycle
 * 调用者必须先持 global_mutex，并保持锁直到所有 HostImpl 访问结束；
 * 返回指针不拥有对象、不可越锁保存。该 helper 本身不加锁。
 *
 * ## References
 * @see global_mutex
 * @see global_impl
 * @see ZrJit_Host_QueryInfo
 * @see query_target
 */
EZrJitHostStatus require_impl(HostImpl **out,
                              SZrJitHostDiagnostic *diagnostic) {
    std::unique_ptr<HostImpl> &impl = global_impl();
    if (!impl || impl->state != ZR_JIT_HOST_STATE_REGISTERED) {
        if (out != ZR_NULL) *out = ZR_NULL;
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_NOT_REGISTERED);
    }
    if (out != ZR_NULL) *out = impl.get();
    return ZR_JIT_HOST_STATUS_OK;
}

/**
 * ## Summary
 * 把一次 core record manager 调用结果统一转换为 facade 结果。
 *
 * ## Parameters
 * @param[in] status core 操作的返回码。
 * @param[out] diagnostic 可空的 facade 诊断。
 * @param[in] coreDiagnostic 可空的 core 诊断快照。
 *
 * ## Returns
 * @return core 成功时清理 facade 诊断并返回 OK；失败时由 map_core_status 转换。
 *
 * ## Constraints and lifecycle
 * 本函数不改变 manager/handle 状态，也不保存诊断指针。
 *
 * ## References
 * @see map_core_status
 * @see ZrJit_Host_Publish
 * @see ZrJit_Host_Release
 */
EZrJitHostStatus core_operation(EZrHostJitStatus status,
                                 SZrJitHostDiagnostic *diagnostic,
                                 const SZrHostJitDiagnostic *coreDiagnostic) {
    if (status == ZR_HOST_JIT_STATUS_OK) {
        clear_jit_diagnostic(diagnostic);
        return ZR_JIT_HOST_STATUS_OK;
    }
    return map_core_status(status, diagnostic, coreDiagnostic);
}

/**
 * ## Summary
 * 从已复制的 target/profile 产生非零 backend identity 标量。
 *
 * ## Parameters
 * @param[in] options 当前 HostImpl 持有的注册选项。
 *
 * ## Returns
 * @return 对 architecture、targetTripleHash 和 layoutHash 混合后返回非零值。
 *
 * ## Constraints and lifecycle
 * 仅供本进程诊断/能力标识使用，不是密码学 hash、跨进程稳定缓存键或执行地址。
 *
 * ## References
 * @see ZrJit_Host_Register
 * @see ZrJit_Host_QueryInfo
 */
TZrUInt64 backend_identity(const SZrHostJitOptions &options) {
    TZrUInt64 value = 0x5a524a4954000001ULL;
    value ^= ((TZrUInt64)options.target.architecture << 8u);
    value ^= options.target.targetTripleHash;
    value ^= (options.target.layoutHash << 1u);
    return value == 0u ? 1u : value;
}

/**
 * ## Summary
 * 将 codeCacheBytes 换算为固定数量的 core metadata record 槽。
 *
 * ## Parameters
 * @param[in] options Register 已验证的选项快照。
 *
 * ## Returns
 * @return 预算小于一个 record 时返回 0；否则向下取整并将容量截在 4096 个槽。
 *
 * ## Constraints and lifecycle
 * 该预算用于 vector metadata 槽，不是可执行代码 cache 大小；结果在 Register 分配前使用。
 *
 * ## References
 * @see ZrJit_Host_Register
 * @see SZrHostJitCodeRecord
 */
TZrUInt32 record_capacity(const SZrHostJitOptions &options) {
    const TZrUInt32 recordSize = (TZrUInt32)sizeof(SZrHostJitCodeRecord);
    if (recordSize == 0u || options.codeCacheBytes < recordSize) return 0u;
    TZrUInt32 capacity = options.codeCacheBytes / recordSize;
    /* 限制不可信预算导致的过量分配；这是记录数上限，不是机器码缓存。 */
    return std::min<TZrUInt32>(capacity, 4096u);
}

/**
 * ## Summary
 * 根据当前 HostImpl 填充 core backend service 可复制的 descriptor 值。
 *
 * ## Parameters
 * @param[in] impl 由调用方持 global_mutex 保护的 REGISTERED 实例。
 * @param[out] descriptor 调用方提供的非空输出；先清零后写入 ABI/目标/vtable。
 *
 * ## Returns
 * 无返回值；结果通过 descriptor 交付。
 *
 * ## Constraints and lifecycle
 * descriptor 的回调函数是静态代码，userData 清空，不借用 impl；复制值不会注册到
 * service，也不会延长 impl 生命周期。operation mask 是契约掩码，不是 provider 证明。
 *
 * ## References
 * @see ZrJit_Host_GetDescriptor
 * @see query_target
 * @see destroy_backend
 */
void fill_descriptor(const HostImpl &impl,
                     SZrExecutionBackendDescriptor *descriptor) {
    std::memset(descriptor, 0, sizeof(*descriptor));
    descriptor->magic = ZR_EXECUTION_BACKEND_MAGIC;
    descriptor->schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    descriptor->backendKind = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    descriptor->flags = ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_ASYNC |
                        ZR_EXECUTION_BACKEND_DESCRIPTOR_FLAG_RUNTIME_ONLY;
    descriptor->target.schemaVersion = ZR_EXECUTION_BACKEND_SCHEMA_VERSION;
    descriptor->target.kind = ZR_EXECUTION_BACKEND_TARGET_HOST_JIT;
    descriptor->target.abiVersion = impl.options.target.abiVersion;
    descriptor->target.targetTripleHash = impl.options.target.targetTripleHash;
    descriptor->target.layoutHash = impl.options.target.layoutHash;
    descriptor->target.capabilityHash = impl.backendIdentity;
    descriptor->supportedOperations = ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK;
    descriptor->vtable.queryTarget = &query_target;
    descriptor->vtable.compileAsync = &compile_async;
    descriptor->vtable.cancelCompile = &cancel_compile;
    descriptor->vtable.lookupEntry = &lookup_entry;
    descriptor->vtable.queryMap = &query_map;
    descriptor->vtable.unregisterMaps = &unregister_maps;
    descriptor->vtable.retire = &retire_code;
    descriptor->vtable.destroy = &destroy_backend;
    /* 复制后的 descriptor 可能晚于本次注册；空 userData 避免悬挂 HostImpl。 */
    descriptor->userData = ZR_NULL;
}

/**
 * ## Summary
 * 清零并填充 core execution backend callback 的诊断结构。
 *
 * ## Parameters
 * @param[out] diagnostic 可空的 core service 输出。
 * @param[in] status callback 的 core backend 结果。
 * @param[in] expected 32 位期望/类别字段。
 * @param[in] actual 32 位实际/类别字段。
 *
 * ## Returns
 * 无返回值；空诊断时不写入。
 *
 * ## Constraints and lifecycle
 * 先清零整份结构；此 helper 参数限制为 32 位，因此不得用它承载完整 64 位 hash。
 *
 * ## References
 * @see query_target
 * @see compile_async
 * @see lookup_entry
 */
void fill_backend_diagnostic(SZrExecutionBackendDiagnostic *diagnostic,
                             EZrExecutionBackendStatus status,
                             TZrUInt32 expected = 0u,
                             TZrUInt32 actual = 0u) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
}

/**
 * ## Summary
 * 响应 core service 的 target-selection callback，检查 target 类别、操作位和注册摘要。
 *
 * ## Parameters
 * @param[in] target service 提供的目标快照；必须非空且 kind 为 HOST_JIT。
 * @param[in] requiredOperations 请求操作集合；未知位被拒绝。
 * @param[in] userData descriptor 预留上下文；本实现忽略，因为 descriptor 不借用 HostImpl。
 * @param[out] diagnostic 可空的 core backend 诊断。
 *
 * ## Returns
 * @return 已注销或目标无效时报告不可用/不支持；ABI、triple 或 layout 不匹配时报契约错误；
 * 当前机器码能力为 false，因此所有匹配目标仍返回 BACKEND_UNAVAILABLE。
 *
 * ## Constraints and lifecycle
 * 通过复制 descriptor 中的静态回调进入；持 global_mutex 后重新查 singleton，
 * 不保存 target/userData。service 可据不可用结果决定 fallback。
 * BUG: layoutHash 比较使用完整 64 位值，但错误诊断经 32 位 fill_backend_diagnostic
 * 参数截断。可达路径是 GetDescriptor 后直接调用 vtable.queryTarget，并传入仅高 32
 * 位不同的 layoutHash：返回 CONTRACT_MISMATCH，却无法在 expected/actual 诊断中区分。
 * 修正入口应为 fill_backend_diagnostic 的哈希宽度契约及本 callback。
 *
 * ## References
 * @see fill_descriptor
 * @see require_impl
 * @see fill_backend_diagnostic
 * @see tests/core/test_ssa_host_jit_optional.c
 */
EZrExecutionBackendStatus query_target(
        const SZrExecutionBackendTarget *target,
        TZrUInt32 requiredOperations,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, ZR_NULL) != ZR_JIT_HOST_STATUS_OK ||
        target == ZR_NULL) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE);
        return ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
    }
    if (target->kind != ZR_EXECUTION_BACKEND_TARGET_HOST_JIT) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET,
                                ZR_EXECUTION_BACKEND_TARGET_HOST_JIT,
                                target->kind);
        return ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_TARGET;
    }
    if ((requiredOperations & ~ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK) != 0u) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION,
                                ZR_EXECUTION_BACKEND_OPERATION_KNOWN_MASK,
                                requiredOperations);
        return ZR_EXECUTION_BACKEND_STATUS_UNSUPPORTED_OPERATION;
    }
    if (target->abiVersion != impl->options.target.abiVersion ||
        target->targetTripleHash != impl->options.target.targetTripleHash ||
        target->layoutHash != impl->options.target.layoutHash) {
        /* BUG 证据见 callback 文档：该 helper 的两个值参数只有 32 位。 */
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH,
                                (TZrUInt32)impl->options.target.layoutHash,
                                (TZrUInt32)target->layoutHash);
        return ZR_EXECUTION_BACKEND_STATUS_CONTRACT_MISMATCH;
    }
    if (!impl->machineCodeAvailable) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE,
                                1u, 0u);
        return ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
    }
    fill_backend_diagnostic(diagnostic, ZR_EXECUTION_BACKEND_STATUS_OK);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * ## Summary
 * 实现 descriptor 的 compileAsync 回调；当前 adapter 没有编译 provider。
 *
 * ## Parameters
 * @param[in] invocation 非空的 service 编译请求；本实现只检查其是否存在。
 * @param[in] userData 未使用的 descriptor context。
 * @param[out] completedCode 可空；非空时先清零，绝不伪造已编译结果。
 * @param[out] diagnostic 可空的 service 诊断。
 *
 * ## Returns
 * @return invocation 为空时 INVALID_ARGUMENT；否则 BACKEND_UNAVAILABLE。
 *
 * ## Constraints and lifecycle
 * 不创建异步任务、代码缓存或 machine code；无资源需取消或释放。
 *
 * ## References
 * @see fill_descriptor
 * @see cancel_compile
 * @see ZrJit_Host_Compile
 */
EZrExecutionBackendStatus compile_async(
        const SZrExecutionBackendCompileInvocation *invocation,
        TZrPtr userData,
        SZrExecutionBackendCompiledCode *completedCode,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    if (completedCode != ZR_NULL) std::memset(completedCode, 0, sizeof(*completedCode));
    if (invocation == ZR_NULL) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
        return ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT;
    }
    fill_backend_diagnostic(diagnostic,
                            ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE,
                            1u, 0u);
    return ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
}

/**
 * ## Summary
 * 满足 descriptor cancelCompile vtable 的无工作取消回调。
 *
 * ## Parameters
 * @param[in] ticket 非空的 service 编译票据；当前实现不解释其内容。
 * @param[in] userData 未使用的 descriptor context。
 * @param[out] diagnostic 可空的 service 诊断。
 *
 * ## Returns
 * @return 非空票据返回 OK；空指针返回 INVALID_ARGUMENT。
 *
 * ## Constraints and lifecycle
 * 当前 compileAsync 不启动异步任务，因此成功表示无待取消工作，不释放票据或其他资源。
 *
 * ## References
 * @see compile_async
 * @see fill_descriptor
 */
EZrExecutionBackendStatus cancel_compile(
        const SZrExecutionCompileTicket *ticket,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    if (ticket == ZR_NULL) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
        return ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT;
    }
    fill_backend_diagnostic(diagnostic, ZR_EXECUTION_BACKEND_STATUS_OK);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * ## Summary
 * 实现 descriptor lookupEntry callback，并保证当前 provider 不泄露入口地址。
 *
 * ## Parameters
 * @param[in] code 非空 service 代码元数据；只检查是否存在。
 * @param[out] entryAddress 非空原生地址输出；非空时先置零。
 * @param[in] userData 未使用的 descriptor context。
 * @param[out] diagnostic 可空的 service 诊断。
 *
 * ## Returns
 * @return 参数无效时 INVALID_ARGUMENT；合法输入在当前无 provider 时返回 BACKEND_UNAVAILABLE。
 *
 * ## Constraints and lifecycle
 * 从不生成或返回可执行地址；空输出保持零，调用方不得执行零值。
 *
 * ## References
 * @see fill_descriptor
 * @see ZrJit_Host_LookupEntry
 */
EZrExecutionBackendStatus lookup_entry(
        const SZrExecutionBackendCodeInfo *code,
        TZrNativePtr *entryAddress,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    if (entryAddress != ZR_NULL) *entryAddress = (TZrNativePtr)0;
    if (code == ZR_NULL || entryAddress == ZR_NULL) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
        return ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT;
    }
    fill_backend_diagnostic(diagnostic,
                            ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE,
                            1u, 0u);
    return ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE;
}

/**
 * ## Summary
 * 从 service 提供的 code-info 值快照选择并返回一种 map/import 摘要。
 *
 * ## Parameters
 * @param[in] code 非空发布元数据快照；调用期间只读借用。
 * @param[in] mapKind 指定 roots、EH、debug、deopt 或 imports 字段。
 * @param[out] mapHash 非空输出；入口先清零，成功后写入选定摘要。
 * @param[in] userData 未使用的 descriptor context。
 * @param[out] diagnostic 可空的 service 诊断。
 *
 * ## Returns
 * @return 参数或 kind 非法时 INVALID_ARGUMENT；选定摘要为零时 MAP_REGISTRATION_INCOMPLETE；
 * 非零快照值返回 OK。
 *
 * ## Constraints and lifecycle
 * 只读取 code-info 中的标量，不读取真实图注册或检查摘要内容；值由调用方提供。
 *
 * ## References
 * @see fill_descriptor
 * @see ZrJit_Host_ValidateStateMaps
 */
EZrExecutionBackendStatus query_map(
        const SZrExecutionBackendCodeInfo *code,
        EZrExecutionBackendMapKind mapKind,
        TZrUInt64 *mapHash,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    if (mapHash != ZR_NULL) *mapHash = 0u;
    if (code == ZR_NULL || mapHash == ZR_NULL ||
        mapKind < ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS ||
        mapKind >= ZR_EXECUTION_BACKEND_MAP_KIND_COUNT) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
        return ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT;
    }
    switch (mapKind) {
        case ZR_EXECUTION_BACKEND_MAP_KIND_ROOTS: *mapHash = code->rootMapHash; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_EH: *mapHash = code->ehMapHash; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_DEBUG: *mapHash = code->debugMapHash; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_DEOPT: *mapHash = code->deoptMapHash; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_IMPORTS: *mapHash = code->runtimeImportsHash; break;
        case ZR_EXECUTION_BACKEND_MAP_KIND_COUNT: break;
    }
    if (*mapHash == 0u) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE);
        return ZR_EXECUTION_BACKEND_STATUS_MAP_REGISTRATION_INCOMPLETE;
    }
    fill_backend_diagnostic(diagnostic, ZR_EXECUTION_BACKEND_STATUS_OK);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * ## Summary
 * 实现 descriptor unregisterMaps callback；当前无平台图注册资源需要注销。
 *
 * ## Parameters
 * @param[in] code 非空 service 代码元数据，仅用于验证调用形状。
 * @param[in] userData 未使用的 descriptor context。
 * @param[out] diagnostic 可空的 service 诊断。
 *
 * ## Returns
 * @return code 非空返回 OK；空指针返回 INVALID_ARGUMENT。
 *
 * ## Constraints and lifecycle
 * 本回调不修改 maps、code record 或平台状态；core service 的记录退休仍由其自己的流程完成。
 *
 * ## References
 * @see fill_descriptor
 * @see retire_code
 */
EZrExecutionBackendStatus unregister_maps(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    if (code == ZR_NULL) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
        return ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT;
    }
    fill_backend_diagnostic(diagnostic, ZR_EXECUTION_BACKEND_STATUS_OK);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * ## Summary
 * 实现 descriptor retire callback；本适配层没有需要释放的可执行内存。
 *
 * ## Parameters
 * @param[in] code 非空 service 代码元数据，仅用于检查调用。
 * @param[in] userData 未使用的 descriptor context。
 * @param[out] diagnostic 可空的 service 诊断。
 *
 * ## Returns
 * @return code 非空返回 OK；空指针返回 INVALID_ARGUMENT。
 *
 * ## Constraints and lifecycle
 * 此 callback 不退休 facade handle、不更改 core manager lease；公开 Evict API 负责记录状态。
 *
 * ## References
 * @see fill_descriptor
 * @see ZrJit_Host_Evict
 */
EZrExecutionBackendStatus retire_code(
        const SZrExecutionBackendCodeInfo *code,
        TZrPtr userData,
        SZrExecutionBackendDiagnostic *diagnostic) {
    (void)userData;
    if (code == ZR_NULL) {
        fill_backend_diagnostic(diagnostic,
                                ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT);
        return ZR_EXECUTION_BACKEND_STATUS_INVALID_ARGUMENT;
    }
    fill_backend_diagnostic(diagnostic, ZR_EXECUTION_BACKEND_STATUS_OK);
    return ZR_EXECUTION_BACKEND_STATUS_OK;
}

/**
 * ## Summary
 * 满足 descriptor destroy vtable 的空操作。
 *
 * ## Parameters
 * @param[in] userData 当前 descriptor 将其设为 null；该参数不会被解引用。
 *
 * ## Returns
 * 无返回值；不修改任何共享状态。
 *
 * ## Constraints and lifecycle
 * descriptor 不拥有 facade singleton，因此 core service 注销 descriptor 不能隐式关闭 JIT；
 * singleton 仅由显式 TryShutdown/Shutdown 管理。
 *
 * ## References
 * @see fill_descriptor
 * @see ZrJit_Host_TryShutdown
 */
void destroy_backend(TZrPtr userData) {
    (void)userData;
}

/**
 * ## Summary
 * 在调用 core Publish 前核对 facade 保存的 publication/proof 不变量。
 *
 * ## Parameters
 * @param[in] proof Prepare 时由 facade 保存的值副本。
 * @param[out] diagnostic 可空；失败时写入声明缺失或代码身份错误。
 *
 * ## Returns
 * @return 已知 publication/registration 位齐全且 identity、hash 和 operationMask 非零时返回 OK；
 * W^X、图登记或代码摘要缺失时返回对应 facade 状态。
 *
 * ## Constraints and lifecycle
 * 只验证 proof 中缓存的声明，不检验真实页权限、图内容或 machine code。
 * proof 生命周期与对应未回收 core record 一致。
 *
 * ## References
 * @see SZrJitPublicationProof
 * @see ZrJit_Host_Publish
 */
EZrJitHostStatus validate_publication_proof(
        const SZrJitPublicationProof &proof,
        SZrJitHostDiagnostic *diagnostic) {
    if ((proof.publicationFlags & ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK) !=
            ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_WX_REQUIRED,
                        ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK,
                        proof.publicationFlags);
    }
    if ((proof.registrationFlags & ZR_HOST_JIT_REGISTRATION_KNOWN_MASK) !=
            ZR_HOST_JIT_REGISTRATION_KNOWN_MASK) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
                        ZR_HOST_JIT_REGISTRATION_KNOWN_MASK,
                        proof.registrationFlags);
    }
    if (proof.codeIdentity == 0u || proof.signatureHash == 0u ||
        proof.layoutHash == 0u || proof.abiHash == 0u ||
        proof.operationMask == 0u) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_CODE_INVALID, 1u, 0u);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

/**
 * ## Summary
 * 持全局锁时验证 map/publication 声明、准备 core record 并登记对应 facade proof。
 *
 * ## Parameters
 * @param[in,out] impl 当前 REGISTERED HostImpl；调用方持 global_mutex。
 * @param[in] facts 待准备的 publication 值快照。
 * @param[in] stateMaps 对应四类图与 frame layout 声明。
 * @param[out] handle 已由公开 wrapper 清空的 core handle 输出。
 * @param[out] diagnostic 可空的 facade 诊断。
 *
 * ## Returns
 * @return 两组声明与 core prepare 均成功且 proof 追加成功时返回 OK。map 或 publication 不自洽、
 * core 拒绝或 proof 分配失败时返回对应状态；proof 追加失败分支会尝试 Evict/Collect、
 * 清空 handle 并返回 CAPACITY，但忽略清理调用结果，不能保证失败回滚已完成。
 *
 * ## Constraints and lifecycle
 * 必须由持 global_mutex 的 wrapper 调用；facts/stateMaps 仅同步借用。
 * 当前逐一比较四个 map hash 与 publication。TODO: facts.target 由 core 自身校验，
 * 但此处未和 impl.options.target 比较；核 Prepare/Publish 的目标身份公开约定及未来
 * provider 绑定要求，避免仅因 Compile 有比较就推断准备阶段契约。
 *
 * ## References
 * @see ZrJit_Host_PrepareWithMaps
 * @see ZrJit_Host_ValidateStateMaps
 * @see ZrCore_HostJit_Code_Prepare
 * @see erase_collected_proofs
 */
EZrJitHostStatus prepare_locked(
        HostImpl &impl,
        const SZrHostJitPublicationFacts &facts,
        const SZrJitStateMapFacts &stateMaps,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    EZrJitHostStatus mapStatus = ZrJit_Host_ValidateStateMaps(
            &stateMaps, diagnostic);
    if (mapStatus != ZR_JIT_HOST_STATUS_OK) return mapStatus;
    if (stateMaps.frameLayoutHash != facts.layoutHash ||
        stateMaps.rootMapHash != facts.gcMapHash ||
        stateMaps.unwindMapHash != facts.unwindMapHash ||
        stateMaps.debugMapHash != facts.debugMapHash ||
        stateMaps.deoptMapHash != facts.deoptMapHash) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_TARGET_MISMATCH,
                        1u, 0u, facts.layoutHash, stateMaps.frameLayoutHash);
    }
    EZrHostJitStatus status = ZrCore_HostJit_Code_Prepare(
            &impl.manager, &facts, handle, &coreDiagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return map_core_status(status, diagnostic, &coreDiagnostic);
    }
    SZrJitPublicationProof proof{};
    proof.codeIdentity = facts.codeIdentity;
    proof.signatureHash = facts.signatureHash;
    proof.layoutHash = facts.layoutHash;
    proof.abiHash = facts.abiHash;
    proof.publicationFlags = facts.flags;
    proof.registrationFlags = facts.registrationFlags;
    proof.operationMask = facts.operationMask;
    proof.stateMaps = stateMaps;
    try {
        impl.proofs.push_back(proof);
    } catch (...) {
        /* core 记录先于 proof vector 写入；清理可能失败，故返回 CAPACITY 不等于回滚成功。 */
        TZrUInt32 collected = 0u;
        (void)ZrCore_HostJit_Code_Evict(&impl.manager, facts.codeIdentity,
                                        ZR_NULL);
        (void)ZrCore_HostJit_Code_CollectRetired(&impl.manager, &collected,
                                                 ZR_NULL);
        std::memset(handle, 0, sizeof(*handle));
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_CAPACITY);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

/**
 * ## Summary
 * 移除已经没有非 FREE core record 对应的 publication proofs。
 *
 * ## Parameters
 * @param[in,out] impl 持锁访问的 HostImpl；proof vector 被原地压缩。
 *
 * ## Returns
 * 无返回值；仅保留仍能在记录数组中找到的 codeIdentity。
 *
 * ## Constraints and lifecycle
 * 调用前需完成 core CollectRetired；带 active/retired lease 的记录仍非 FREE，
 * 因而其 proof 必须保留。操作受 global_mutex 保护。
 *
 * ## References
 * @see ZrJit_Host_Collect
 * @see ZrJit_Host_TryShutdown
 * @see SZrJitPublicationProof
 */
void erase_collected_proofs(HostImpl &impl) {
    impl.proofs.erase(
            std::remove_if(impl.proofs.begin(), impl.proofs.end(),
                           /* 只删 records 扫描已不存在或 FREE 的 identity；带 lease 的
                            * retired record 仍非 FREE，因此本谓词会保留其 proof。 */
                           [&impl](const SZrJitPublicationProof &proof) {
                               for (const SZrHostJitCodeRecord &record : impl.records) {
                                   if (record.state != ZR_HOST_JIT_CODE_FREE &&
                                       record.codeIdentity == proof.codeIdentity) {
                                       return false;
                                   }
                               }
                               return true;
                           }),
            impl.proofs.end());
}

/**
 * ## Summary
 * 把 facade 注册错误转成既有 ExecIR diagnostic code，并复制 core 差异字段。
 *
 * ## Parameters
 * @param[out] diagnostic 可空的 ExecIR 诊断输出。
 * @param[in] status facade 注册阶段状态。
 * @param[in] coreDiagnostic 可空的 core 差异快照。
 *
 * ## Returns
 * 无返回值；由 diagnostic->code 与 source/version/hash 字段交付映射结果。
 *
 * ## Constraints and lifecycle
 * 仅 Register 使用的诊断协议；未知或未专门处理的 facade 状态按 default 分类。
 * 不修改 HostImpl，也不保留诊断指针。
 *
 * ## References
 * @see clear_execution_diagnostic
 * @see ZrJit_Host_Register
 */
void set_exec_diagnostic_for_host(SZrExecIrDiagnostic *diagnostic,
                                  EZrJitHostStatus status,
                                  const SZrHostJitDiagnostic *coreDiagnostic) {
    clear_execution_diagnostic(diagnostic);
    if (diagnostic == ZR_NULL) return;
    switch (status) {
        case ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH:
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
            break;
        case ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED:
        case ZR_JIT_HOST_STATUS_TARGET_MISMATCH:
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH;
            break;
        case ZR_JIT_HOST_STATUS_STATE_MAP_INVALID:
        case ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE:
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
            break;
        case ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN:
        case ZR_JIT_HOST_STATUS_IMPORT_INVALID:
        case ZR_JIT_HOST_STATUS_OPERATION_UNSUPPORTED:
        case ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE:
        case ZR_JIT_HOST_STATUS_DISABLED:
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
            break;
        case ZR_JIT_HOST_STATUS_INVALID_ARGUMENT:
        case ZR_JIT_HOST_STATUS_INVALID_FLAGS:
        case ZR_JIT_HOST_STATUS_CODE_INVALID:
        case ZR_JIT_HOST_STATUS_CAPACITY:
        case ZR_JIT_HOST_STATUS_ALREADY_REGISTERED:
        case ZR_JIT_HOST_STATUS_NOT_REGISTERED:
        case ZR_JIT_HOST_STATUS_STALE_HANDLE:
        case ZR_JIT_HOST_STATUS_ACTIVE_LEASE:
        case ZR_JIT_HOST_STATUS_INVALID_STATE:
        case ZR_JIT_HOST_STATUS_OVERFLOW:
        case ZR_JIT_HOST_STATUS_WX_REQUIRED:
        case ZR_JIT_HOST_STATUS_PENDING:
        case ZR_JIT_HOST_STATUS_FALLBACK_EXECBC:
        case ZR_JIT_HOST_STATUS_FALLBACK_AOT:
        case ZR_JIT_HOST_STATUS_OK:
        case ZR_JIT_HOST_STATUS_COUNT:
        default:
            diagnostic->code = status == ZR_JIT_HOST_STATUS_OK
                    ? ZR_EXECUTION_DIAGNOSTIC_NONE
                    : ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
            break;
    }
    if (coreDiagnostic != ZR_NULL) {
        diagnostic->sourceId = coreDiagnostic->sourceIndex;
        diagnostic->expectedVersion = coreDiagnostic->expected;
        diagnostic->actualVersion = coreDiagnostic->actual;
        diagnostic->expectedHash = coreDiagnostic->expectedHash;
        diagnostic->actualHash = coreDiagnostic->actualHash;
    }
}

}  // namespace

/* 测试和调用方在跨 API 复用诊断对象前可显式清除旧状态。 */
extern "C" void ZrJit_Host_DiagnosticInit(SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
}

/* 未知枚举值返回通用名称，供诊断展示而不假定输入可信。 */
extern "C" const TZrChar *ZrJit_Host_StatusName(EZrJitHostStatus status) {
    switch (status) {
        case ZR_JIT_HOST_STATUS_OK: return "ok";
        case ZR_JIT_HOST_STATUS_PENDING: return "pending";
        case ZR_JIT_HOST_STATUS_DISABLED: return "disabled";
        case ZR_JIT_HOST_STATUS_ALREADY_REGISTERED: return "already-registered";
        case ZR_JIT_HOST_STATUS_NOT_REGISTERED: return "not-registered";
        case ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE: return "backend-unavailable";
        case ZR_JIT_HOST_STATUS_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH: return "schema-mismatch";
        case ZR_JIT_HOST_STATUS_INVALID_FLAGS: return "invalid-flags";
        case ZR_JIT_HOST_STATUS_TARGET_UNSUPPORTED: return "target-unsupported";
        case ZR_JIT_HOST_STATUS_TARGET_MISMATCH: return "target-mismatch";
        case ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN: return "import-forbidden";
        case ZR_JIT_HOST_STATUS_IMPORT_INVALID: return "import-invalid";
        case ZR_JIT_HOST_STATUS_OPERATION_UNSUPPORTED: return "operation-unsupported";
        case ZR_JIT_HOST_STATUS_STATE_MAP_INVALID: return "state-map-invalid";
        case ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE: return "registration-incomplete";
        case ZR_JIT_HOST_STATUS_WX_REQUIRED: return "wx-required";
        case ZR_JIT_HOST_STATUS_CODE_INVALID: return "code-invalid";
        case ZR_JIT_HOST_STATUS_CAPACITY: return "capacity";
        case ZR_JIT_HOST_STATUS_STALE_HANDLE: return "stale-handle";
        case ZR_JIT_HOST_STATUS_ACTIVE_LEASE: return "active-lease";
        case ZR_JIT_HOST_STATUS_INVALID_STATE: return "invalid-state";
        case ZR_JIT_HOST_STATUS_OVERFLOW: return "overflow";
        case ZR_JIT_HOST_STATUS_FALLBACK_EXECBC: return "fallback-execbc";
        case ZR_JIT_HOST_STATUS_FALLBACK_AOT: return "fallback-aot";
        case ZR_JIT_HOST_STATUS_COUNT:
        default: return "jit-host-error";
    }
}

/* Compile 在接触注册状态前先验证请求，避免 fallback 掩盖格式错误。 */
extern "C" EZrJitHostStatus ZrJit_Host_ValidateCompileRequest(
        const SZrJitHostCompileRequest *request,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    if (request == ZR_NULL) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
                        ZR_JIT_HOST_MAGIC, 0u);
    }
    if (request->magic != ZR_JIT_HOST_MAGIC) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
                        ZR_JIT_HOST_MAGIC, request->magic);
    }
    if (request->schemaVersion != ZR_JIT_HOST_SCHEMA_VERSION) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH,
                        ZR_JIT_HOST_SCHEMA_VERSION, request->schemaVersion);
    }
    if ((request->flags & ~ZR_JIT_HOST_OPTION_KNOWN_MASK) != 0u ||
        request->reserved != 0u) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_FLAGS,
                        ZR_JIT_HOST_OPTION_KNOWN_MASK, request->flags);
    }
    if (request->operationMask == 0u ||
        (request->operationMask & ~ZR_HOST_JIT_OPERATION_KNOWN_MASK) != 0u ||
        (request->operationMask & ~request->publication.operationMask) != 0u) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_OPERATION_UNSUPPORTED,
                        request->publication.operationMask,
                        request->operationMask);
    }
    EZrHostJitStatus status = ZrCore_HostJit_ValidatePublication(
            &request->publication, &coreDiagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return map_core_status(status, diagnostic, &coreDiagnostic);
    }
    status = ZrJit_Host_ValidateStateMaps(&request->stateMaps, diagnostic) ==
                     ZR_JIT_HOST_STATUS_OK
             ? ZR_HOST_JIT_STATUS_OK
             : ZR_HOST_JIT_STATUS_CODE_INVALID;
    if (status != ZR_HOST_JIT_STATUS_OK) {
        /* The state-map helper has already populated the public diagnostic. */
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_STATE_MAP_INVALID;
    }
    /* TODO: 该 validator 只比较 frameLayoutHash；prepare_locked 还逐一比较四图 hash。
     * 非零但不同的 map hash 可通过本函数并继续得到 fallback。需核 fallback 请求契约是否
     * 要求与 publication 完全一致，再决定两入口是否统一；当前不能在此承诺四 hash 相等。 */
    if (request->stateMaps.frameLayoutHash != request->publication.layoutHash) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_TARGET_MISMATCH,
                        1u, 0u, request->publication.layoutHash,
                        request->stateMaps.frameLayoutHash);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

/* 先完整验证并构造候选状态，再一次性发布全局单例；不会安装机器码 provider。 */
extern "C" TZrBool ZrJit_Host_Register(
        const SZrHostJitOptions *options,
        SZrExecIrDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_execution_diagnostic(diagnostic);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    if (options == ZR_NULL) {
        set_exec_diagnostic_for_host(diagnostic,
                                     ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
                                     ZR_NULL);
        return ZR_FALSE;
    }
    EZrHostJitStatus coreStatus = ZrCore_HostJit_ValidateOptions(
            options, &coreDiagnostic);
    if (coreStatus != ZR_HOST_JIT_STATUS_OK) {
        EZrJitHostStatus mapped = map_core_status(
                coreStatus, ZR_NULL, &coreDiagnostic);
        set_exec_diagnostic_for_host(diagnostic, mapped, &coreDiagnostic);
        return ZR_FALSE;
    }
    if ((options->flags & ZR_HOST_JIT_OPTION_FLAG_ENABLE) == 0u) {
        set_exec_diagnostic_for_host(diagnostic,
                                     ZR_JIT_HOST_STATUS_DISABLED,
                                     &coreDiagnostic);
        return ZR_FALSE;
    }
    std::lock_guard<std::mutex> lock(global_mutex());
    if (global_impl()) {
        set_exec_diagnostic_for_host(diagnostic,
                                     ZR_JIT_HOST_STATUS_ALREADY_REGISTERED,
                                     ZR_NULL);
        return ZR_FALSE;
    }
    const TZrUInt32 capacity = record_capacity(*options);
    if (capacity == 0u) {
        set_exec_diagnostic_for_host(diagnostic,
                                     ZR_JIT_HOST_STATUS_CAPACITY,
                                     ZR_NULL);
        return ZR_FALSE;
    }
    /* 先在局部 candidate 中完成 vector 与 manager 初始化；只有全部成功后才把 owning
     * unique_ptr 发布到全局 slot，避免其他线程看到半初始化状态。 */
    try {
        std::unique_ptr<HostImpl> candidate(new HostImpl());
        candidate->options = *options;
        candidate->records.resize(capacity);
        candidate->proofs.reserve(capacity);
        candidate->backendIdentity = backend_identity(*options);
        candidate->machineCodeAvailable = ZR_FALSE;
        candidate->fallbackAvailable =
                (options->flags & ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK) != 0u
                ? ZR_TRUE : ZR_FALSE;
        EZrHostJitStatus status = ZrCore_HostJit_CodeManager_Init(
                &candidate->manager, candidate->records.data(), capacity,
                &coreDiagnostic);
        if (status != ZR_HOST_JIT_STATUS_OK) {
            EZrJitHostStatus mapped = map_core_status(
                    status, ZR_NULL, &coreDiagnostic);
            set_exec_diagnostic_for_host(diagnostic, mapped, &coreDiagnostic);
            return ZR_FALSE;
        }
        candidate->state = ZR_JIT_HOST_STATE_REGISTERED;
        global_impl() = std::move(candidate);
        clear_execution_diagnostic(diagnostic);
        return ZR_TRUE;
    } catch (const std::bad_alloc &) {
        set_exec_diagnostic_for_host(diagnostic,
                                     ZR_JIT_HOST_STATUS_CAPACITY,
                                     ZR_NULL);
        return ZR_FALSE;
    } catch (...) {
        set_exec_diagnostic_for_host(diagnostic,
                                     ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE,
                                     ZR_NULL);
        return ZR_FALSE;
    }
}

/* 已准备或发布的记录须先退休回收，活跃 lease 归还后才能撤销数组借用。 */
extern "C" EZrJitHostStatus ZrJit_Host_TryShutdown(
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        if (!global_impl()) {
            clear_jit_diagnostic(diagnostic);
            return ZR_JIT_HOST_STATUS_OK;
        }
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    TZrUInt32 collected = 0u;
    SZrHostJitDiagnostic coreDiagnostic;
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    EZrHostJitStatus status = ZrCore_HostJit_Code_CollectRetired(
            &impl->manager, &collected, &coreDiagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return map_core_status(status, diagnostic, &coreDiagnostic);
    }
    erase_collected_proofs(*impl);
    if (impl->manager.count != 0u || impl->manager.active != ZR_NULL) {
        /* Deinit 只允许空 manager；区分 lease 阻塞与仍存在的非租用记录，供调用方选
         * Release 或 Evict/Collect 后重试，而不是释放 records.data() 的借用存储。 */
        TZrBool hasLease = ZR_FALSE;
        for (const SZrHostJitCodeRecord &record : impl->records) {
            if (record.state != ZR_HOST_JIT_CODE_FREE && record.leaseCount != 0u) {
                hasLease = ZR_TRUE;
                break;
            }
        }
        return fail_jit(diagnostic,
                        hasLease ? ZR_JIT_HOST_STATUS_ACTIVE_LEASE
                                 : ZR_JIT_HOST_STATUS_INVALID_STATE,
                        0u, impl->manager.count);
    }
    impl->state = ZR_JIT_HOST_STATE_SHUTTING_DOWN;
    ZrCore_HostJit_CodeManager_Deinit(&impl->manager);
    if (impl->manager.records != ZR_NULL) {
        impl->state = ZR_JIT_HOST_STATE_REGISTERED;
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_STATE);
    }
    global_impl().reset();
    clear_jit_diagnostic(diagnostic);
    return ZR_JIT_HOST_STATUS_OK;
}

/* 无返回值入口不能传达 ACTIVE_LEASE；需要确认注销完成时应使用 TryShutdown。 */
extern "C" void ZrJit_Host_Shutdown(void) {
    SZrJitHostDiagnostic diagnostic;
    (void)ZrJit_Host_TryShutdown(&diagnostic);
}

extern "C" TZrBool ZrJit_Host_IsRegistered(void) {
    std::lock_guard<std::mutex> lock(global_mutex());
    return (TZrBool)(global_impl() != nullptr &&
                     global_impl()->state == ZR_JIT_HOST_STATE_REGISTERED);
}

extern "C" TZrBool ZrJit_Host_IsMachineCodeAvailable(void) {
    std::lock_guard<std::mutex> lock(global_mutex());
    return global_impl() != nullptr ? global_impl()->machineCodeAvailable : ZR_FALSE;
}

extern "C" EZrJitHostStatus ZrJit_Host_QueryInfo(
        SZrJitHostBackendInfo *info,
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
    if (info != ZR_NULL) std::memset(info, 0, sizeof(*info));
    if (info == ZR_NULL) return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    info->magic = ZR_JIT_HOST_MAGIC;
    info->schemaVersion = ZR_JIT_HOST_SCHEMA_VERSION;
    info->state = impl->state;
    info->machineCodeAvailable = impl->machineCodeAvailable;
    info->fallbackAvailable = impl->fallbackAvailable;
    info->target = impl->options.target;
    info->supportedOperations = ZR_HOST_JIT_OPERATION_KNOWN_MASK;
    info->codeRecordCapacity = (TZrUInt32)impl->records.size();
    info->backendIdentity = impl->backendIdentity;
    return ZR_JIT_HOST_STATUS_OK;
}

extern "C" TZrBool ZrJit_Host_GetDescriptor(
        SZrExecutionBackendDescriptor *descriptor,
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
    if (descriptor != ZR_NULL) std::memset(descriptor, 0, sizeof(*descriptor));
    if (descriptor == ZR_NULL) {
        fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) return ZR_FALSE;
    fill_descriptor(*impl, descriptor);
    return ZR_TRUE;
}

/* core 白名单检验通过后，再执行本次注册的导入条数上限。 */
extern "C" EZrJitHostStatus ZrJit_Host_ValidateImport(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    EZrHostJitStatus status = ZrCore_HostJit_ValidateImports(
            manifest, symbolId, signatureHash, &coreDiagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return map_core_status(status, diagnostic, &coreDiagnostic);
    }
    std::lock_guard<std::mutex> lock(global_mutex());
    if (global_impl() != nullptr && manifest->count > global_impl()->options.maxImports) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_CAPACITY,
                        global_impl()->options.maxImports, manifest->count);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

/* 便捷入口只为代码记录生命周期构造摘要，条目数来自默认值而非真实登记。
 * TODO: 接入实际 ORC 发布路径前，核实是否应只允许带独立图证据的
 * PrepareWithMaps；目前全仓没有该便捷入口的调用方。 */
extern "C" EZrJitHostStatus ZrJit_Host_Prepare(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    SZrJitStateMapFacts stateMaps;
    clear_jit_diagnostic(diagnostic);
    /* TODO: 与下面两个 facade 输出一样会覆盖旧句柄。调用方须提供空独立槽；
     * 核对仓内/外使用是否遵守 core Prepare 的 no-active-lease 前置，再确认此 API 契约。 */
    if (handle != ZR_NULL) std::memset(handle, 0, sizeof(*handle));
    if (facts == ZR_NULL || handle == ZR_NULL) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    }
    std::memset(&stateMaps, 0, sizeof(stateMaps));
    stateMaps.schemaVersion = ZR_JIT_HOST_SCHEMA_VERSION;
    stateMaps.registrationFlags = ZR_JIT_HOST_MAP_KNOWN_MASK;
    stateMaps.rootEntryCount = 1u;
    stateMaps.unwindEntryCount = 1u;
    stateMaps.debugEntryCount = 1u;
    stateMaps.deoptEntryCount = 1u;
    stateMaps.rootMapHash = facts->gcMapHash;
    stateMaps.unwindMapHash = facts->unwindMapHash;
    stateMaps.debugMapHash = facts->debugMapHash;
    stateMaps.deoptMapHash = facts->deoptMapHash;
    stateMaps.frameLayoutHash = facts->layoutHash;
    return ZrJit_Host_PrepareWithMaps(facts, &stateMaps, handle, diagnostic);
}

extern "C" EZrJitHostStatus ZrJit_Host_PrepareWithMaps(
        const SZrHostJitPublicationFacts *facts,
        const SZrJitStateMapFacts *stateMaps,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
    /* TODO: wrapper 不检查 handle 原状态，先清空再校验；空输出/非租用句柄是调用前提，
     * 复用 lease 时须先保留原句柄并 Release。核对 facade 输出语义和所有调用点。 */
    if (handle != ZR_NULL) std::memset(handle, 0, sizeof(*handle));
    if (facts == ZR_NULL || stateMaps == ZR_NULL || handle == ZR_NULL) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    return prepare_locked(*impl, *facts, *stateMaps, handle, diagnostic);
}

/* facade 先找准备时保存的证明，再由 core manager 完成状态切换。 */
extern "C" EZrJitHostStatus ZrJit_Host_Publish(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    if (handle == ZR_NULL) return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    /* 谓词只按 handle identity 找准备时保存的副本；core Publish 仍复核句柄身份/状态。 */
    const auto proof = std::find_if(
            impl->proofs.begin(), impl->proofs.end(),
            [handle](const SZrJitPublicationProof &candidate) {
                return candidate.codeIdentity == handle->codeIdentity;
            });
    if (proof == impl->proofs.end()) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_STALE_HANDLE);
    }
    EZrJitHostStatus proofStatus = validate_publication_proof(*proof, diagnostic);
    if (proofStatus != ZR_JIT_HOST_STATUS_OK) return proofStatus;
    EZrHostJitStatus status = ZrCore_HostJit_Code_Publish(
            &impl->manager, handle, &coreDiagnostic);
    return core_operation(status, diagnostic, &coreDiagnostic);
}

/* Acquire 返回的 handle 带 lease；即使随后 Evict，仍须 Release。 */
extern "C" EZrJitHostStatus ZrJit_Host_Acquire(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    if (handle == ZR_NULL) return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    SZrHostJitCodeHandle acquired{};
    EZrHostJitStatus status = ZrCore_HostJit_Code_AcquireActive(
            &impl->manager, &acquired, &coreDiagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return map_core_status(status, diagnostic, &coreDiagnostic);
    /* TODO: core 已对新临时槽取得 lease；写入 facade 输出前不检查旧值。
     * 调用方须提供空独立槽，复用 lease 前保存并 Release；核查 facade 公开输出约定。 */
    *handle = acquired;
    return ZR_JIT_HOST_STATUS_OK;
}

extern "C" EZrJitHostStatus ZrJit_Host_Release(
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    if (handle == ZR_NULL) return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    EZrHostJitStatus status = ZrCore_HostJit_Code_Release(
            &impl->manager, handle, &coreDiagnostic);
    return core_operation(status, diagnostic, &coreDiagnostic);
}

extern "C" EZrJitHostStatus ZrJit_Host_Evict(
        TZrUInt64 codeIdentity,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    EZrHostJitStatus status = ZrCore_HostJit_Code_Evict(
            &impl->manager, codeIdentity, &coreDiagnostic);
    return core_operation(status, diagnostic, &coreDiagnostic);
}

/* core 先回收无 lease 的退休记录，facade 再按剩余 identity 清证明。 */
extern "C" EZrJitHostStatus ZrJit_Host_Collect(
        TZrUInt32 *outCollected,
        SZrJitHostDiagnostic *diagnostic) {
    SZrHostJitDiagnostic coreDiagnostic;
    clear_jit_diagnostic(diagnostic);
    if (outCollected != ZR_NULL) *outCollected = 0u;
    if (outCollected == ZR_NULL) return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    std::memset(&coreDiagnostic, 0, sizeof(coreDiagnostic));
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    EZrHostJitStatus status = ZrCore_HostJit_Code_CollectRetired(
            &impl->manager, outCollected, &coreDiagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return map_core_status(status, diagnostic, &coreDiagnostic);
    erase_collected_proofs(*impl);
    return ZR_JIT_HOST_STATUS_OK;
}

extern "C" EZrJitHostStatus ZrJit_Host_LookupEntry(
        const SZrHostJitCodeHandle *handle,
        TZrNativePtr *entryAddress,
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
    if (entryAddress != ZR_NULL) *entryAddress = (TZrNativePtr)0;
    if (handle == ZR_NULL || entryAddress == ZR_NULL) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    (void)impl;
    return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE,
                    1u, 0u);
}

/* 当前验证请求与注册目标后选择 fallback 或不可用，不产生代码记录或地址。 */
extern "C" EZrJitHostStatus ZrJit_Host_Compile(
        const SZrJitHostCompileRequest *request,
        SZrHostJitCodeHandle *outHandle,
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
    /* TODO: outHandle 即使校验失败也会先清零；调用方必须给独立空槽，
     * 不得用仍须 Release 的 lease 作输出。核查 facade 与公开 C 输出参数约定是否一致。 */
    if (outHandle != ZR_NULL) std::memset(outHandle, 0, sizeof(*outHandle));
    if (request == ZR_NULL || outHandle == ZR_NULL) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    }
    EZrJitHostStatus status = ZrJit_Host_ValidateCompileRequest(request, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    std::lock_guard<std::mutex> lock(global_mutex());
    HostImpl *impl = ZR_NULL;
    if (require_impl(&impl, diagnostic) != ZR_JIT_HOST_STATUS_OK) {
        return diagnostic != ZR_NULL ? diagnostic->status
                                     : ZR_JIT_HOST_STATUS_NOT_REGISTERED;
    }
    if (request->publication.target.architecture !=
            impl->options.target.architecture ||
        request->publication.target.platform != impl->options.target.platform ||
        request->publication.target.pointerSize != impl->options.target.pointerSize ||
        request->publication.target.endianness != impl->options.target.endianness ||
        request->publication.target.abiVersion != impl->options.target.abiVersion ||
        request->publication.target.targetTripleHash !=
            impl->options.target.targetTripleHash ||
        request->publication.target.layoutHash != impl->options.target.layoutHash) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_TARGET_MISMATCH,
                        impl->options.target.architecture,
                        request->publication.target.architecture,
                        impl->options.target.layoutHash,
                        request->publication.target.layoutHash);
    }
    if (impl->machineCodeAvailable) {
        /* The optional source deliberately has no LLVM ABI dependency yet;
         * an ORC provider can replace this branch without changing callers. */
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE);
    }
    /* 只有请求和注册两层都允许 fallback 时才返回可继续执行 ExecBC 的提示状态，
     * 否则保持显式 BACKEND_UNAVAILABLE。 */
    if ((request->flags & ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE) != 0u ||
        (request->flags & ZR_JIT_HOST_OPTION_ALLOW_FALLBACK) == 0u ||
        !impl->fallbackAvailable) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE,
                        1u, 0u, 0u, 0u, request->sourceIndex);
    }
    return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_FALLBACK_EXECBC,
                    1u, 0u, 0u, 0u, request->sourceIndex);
}
