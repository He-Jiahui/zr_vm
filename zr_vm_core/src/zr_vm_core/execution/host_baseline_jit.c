/* 本翻译单元只维护 caller-owned 元数据与 lease；普通 core 构建无需 LLVM 或可执行页 provider。 */
#include "zr_vm_core/host_baseline_jit.h"

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/*
 * 使可选诊断成为本次调用的空白输出。
 * 只清零调用方可写存储；不表示已准备或已发布代码。
 */
static void host_jit_clear(SZrHostJitDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

/*
 * 把拒绝原因及标量见证写入可选诊断，供上层映射状态。
 * 不保存输入指针；expected/actual 与 hash 的含义由失败分支决定。
 */
static EZrHostJitStatus host_jit_fail(
        EZrHostJitStatus status,
        SZrHostJitDiagnostic *diagnostic,
        TZrUInt32 expected,
        TZrUInt32 actual,
        TZrUInt64 expectedHash,
        TZrUInt64 actualHash,
        TZrUInt32 sourceIndex) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
        diagnostic->expectedHash = expectedHash;
        diagnostic->actualHash = actualHash;
        diagnostic->sourceIndex = sourceIndex;
    }
    return status;
}

/*
 * 限定此契约允许尝试的主机平台与两种架构。
 * 属于声明白名单；仍需另核编译架构、指针宽度与 ABI。
 */
static TZrBool host_jit_target_is_host(const SZrHostJitTargetContract *target) {
    return target != ZR_NULL && target->platform == ZR_HOST_JIT_PLATFORM_HOST &&
           (target->architecture == ZR_HOST_JIT_ARCH_X86_64 ||
            target->architecture == ZR_HOST_JIT_ARCH_AARCH64);
}

/*
 * 从编译器目标宏取得本翻译单元的架构见证。
 * 未知目标返回 NONE；不探测运行时 CPU 或操作系统能力。
 */
static EZrHostJitArchitecture host_jit_compiled_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return ZR_HOST_JIT_ARCH_X86_64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ZR_HOST_JIT_ARCH_AARCH64;
#else
    return ZR_HOST_JIT_ARCH_NONE;
#endif
}

/*
 * 确认目标声明符合本 core 的主机、ABI 与布局摘要约束。
 * 只读借用输入；hash 仅要求非零，不重算宿主布局或证明 provider 可用。
 */
EZrHostJitStatus ZrCore_HostJit_ValidateTarget(
        const SZrHostJitTargetContract *target,
        SZrHostJitDiagnostic *diagnostic) {
    host_jit_clear(diagnostic);
    if (target == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             0u, 0u, 0u, 0u, 0u);
    }
    if (target->schemaVersion != ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_SCHEMA_MISMATCH, diagnostic,
                             ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION,
                             target->schemaVersion, 0u, 0u, 0u);
    }
    if (!host_jit_target_is_host(target)) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_TARGET_UNSUPPORTED, diagnostic,
                             ZR_HOST_JIT_PLATFORM_HOST, target->platform,
                             0u, 0u, 0u);
    }
    {
        EZrHostJitArchitecture compiledArchitecture =
                host_jit_compiled_architecture();
        if (compiledArchitecture == ZR_HOST_JIT_ARCH_NONE ||
            target->architecture != compiledArchitecture) {
            return host_jit_fail(ZR_HOST_JIT_STATUS_TARGET_MISMATCH,
                                 diagnostic, compiledArchitecture,
                                 target->architecture, 0u, 0u, 0u);
        }
    }
    if (target->pointerSize != sizeof(void *) ||
        target->endianness != ZR_HOST_JIT_ENDIAN_LITTLE) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_TARGET_MISMATCH, diagnostic,
                             (TZrUInt32)sizeof(void *), target->pointerSize,
                             0u, 0u, 0u);
    }
    if (target->abiVersion != ZR_EXECUTION_CONTRACT_ABI_VERSION) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_ABI_MISMATCH, diagnostic,
                             ZR_EXECUTION_CONTRACT_ABI_VERSION,
                             target->abiVersion, 0u, 0u, 0u);
    }
    if (target->targetTripleHash == 0u || target->layoutHash == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_LAYOUT_MISMATCH, diagnostic,
                             1u, 0u, 0u, target->layoutHash, 0u);
    }
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 为启用主机记录管理的配置建立前置契约。
 * 未设 ENABLE 时只核 schema 和已知选项位；不校验 target/预算，也不分配缓存。
 */
EZrHostJitStatus ZrCore_HostJit_ValidateOptions(
        const SZrHostJitOptions *options,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (options == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             0u, 0u, 0u, 0u, 0u);
    }
    if (options->schemaVersion != ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_SCHEMA_MISMATCH, diagnostic,
                             ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION,
                             options->schemaVersion, 0u, 0u, 0u);
    }
    if ((options->flags & ~ZR_HOST_JIT_OPTION_FLAG_KNOWN_MASK) != 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_FLAGS, diagnostic,
                             ZR_HOST_JIT_OPTION_FLAG_KNOWN_MASK,
                             options->flags, 0u, 0u, 0u);
    }
    /*
     * 禁用配置保留普通 core 路由的前置校验边界。
     * 只核 schema/已知选项位；不把该成功解释为 target 或预算已有效。
     */
    if ((options->flags & ZR_HOST_JIT_OPTION_FLAG_ENABLE) == 0u) {
        return ZR_HOST_JIT_STATUS_OK;
    }
    status = ZrCore_HostJit_ValidateTarget(&options->target, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return status;
    }
    if (options->codeCacheBytes == 0u || options->maxImports == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_CODE_INVALID, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 核对借用导入清单的结构与符号唯一性，供发布和查询共用。
 * 发布允许空 manifest；非空摘要仅核非零，不验证实际符号绑定或摘要来源。
 */
static EZrHostJitStatus host_jit_validate_manifest(
        const SZrHostJitImportManifest *manifest,
        SZrHostJitDiagnostic *diagnostic) {
    if (manifest == ZR_NULL) {
        return ZR_HOST_JIT_STATUS_OK;
    }
    if (manifest->schemaVersion != ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION ||
        (manifest->count != 0u && manifest->imports == ZR_NULL) ||
        manifest->allowedSymbolHash == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_IMPORT_INVALID, diagnostic,
                             ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION,
                             manifest->schemaVersion, 0u,
                             manifest->allowedSymbolHash, 0u);
    }
    for (TZrUInt32 i = 0u; i < manifest->count; ++i) {
        const SZrHostJitImport *item = &manifest->imports[i];
        if (item->symbolId == 0u || item->signatureHash == 0u ||
            (item->kind != ZR_HOST_JIT_IMPORT_RUNTIME &&
             item->kind != ZR_HOST_JIT_IMPORT_NATIVE) || item->reserved != 0u) {
            return host_jit_fail(ZR_HOST_JIT_STATUS_IMPORT_INVALID, diagnostic,
                                 1u, 0u, 0u, 0u, i);
        }
        for (TZrUInt32 j = 0u; j < i; ++j) {
            if (manifest->imports[j].symbolId == item->symbolId) {
                return host_jit_fail(ZR_HOST_JIT_STATUS_IMPORT_INVALID,
                                     diagnostic, j, i, item->symbolId,
                                     manifest->imports[j].signatureHash, i);
            }
        }
    }
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 按符号身份与签名共同限制一次导入查询。
 * 调用期间借用完整清单和数组；拒绝缺项或签名不符，不返回可调用地址。
 */
EZrHostJitStatus ZrCore_HostJit_ValidateImports(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (manifest == ZR_NULL || symbolId == 0u || signatureHash == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             0u, 0u, 0u, 0u, 0u);
    }
    status = host_jit_validate_manifest(manifest, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return status;
    }
    for (TZrUInt32 i = 0u; i < manifest->count; ++i) {
        const SZrHostJitImport *item = &manifest->imports[i];
        if (item->symbolId == symbolId) {
            if (item->signatureHash != signatureHash) {
                return host_jit_fail(ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN,
                                     diagnostic, 0u, 0u, item->signatureHash,
                                     signatureHash, i);
            }
            return ZR_HOST_JIT_STATUS_OK;
        }
    }
    return host_jit_fail(ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN, diagnostic,
                         0u, 0u, symbolId, 0u, manifest->count);
}

/*
 * 检查一个已知操作位是否包含在给定声明掩码中。
 * operation 必须恰有一个已知位；不校验 mask 的其他位，也不查询机器码能力。
 */
TZrBool ZrCore_HostJit_SupportsOperation(
        TZrUInt32 operation,
        TZrUInt32 operationMask) {
    if (operation == 0u || (operation & (operation - 1u)) != 0u ||
        (operation & ~ZR_HOST_JIT_OPERATION_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }
    return (TZrBool)((operationMask & operation) != 0u);
}

/*
 * 核对发布者提交的目标、注册与代码摘要声明是否自洽。
 * MACHINE_CODE/WX/四图位与非零 hash 都是提交者的声明；本层不检查页权限、图内容或机器码。
 */
EZrHostJitStatus ZrCore_HostJit_ValidatePublication(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (facts == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             0u, 0u, 0u, 0u, 0u);
    }
    status = ZrCore_HostJit_ValidateTarget(&facts->target, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return status;
    }
    if ((facts->flags & ~ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK) != 0u ||
        (facts->registrationFlags & ~ZR_HOST_JIT_REGISTRATION_KNOWN_MASK) != 0u ||
        facts->reserved != 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_FLAGS, diagnostic,
                             ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK,
                             facts->flags, 0u, 0u, 0u);
    }
    /*
     * 发布标记与四图声明的责任边界。
     * 真正页权限与登记资源由 provider/调用方负责，core 只核提交声明。
     */
    if ((facts->flags & ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE) == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_CODE_INVALID, diagnostic,
                             ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE,
                             facts->flags, 0u, 0u, 0u);
    }
    if ((facts->flags & ZR_HOST_JIT_PUBLICATION_FLAG_WX) == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_WX_REQUIRED, diagnostic,
                             ZR_HOST_JIT_PUBLICATION_FLAG_WX, facts->flags,
                             0u, 0u, 0u);
    }
    if ((facts->registrationFlags & ZR_HOST_JIT_REGISTRATION_KNOWN_MASK) !=
        ZR_HOST_JIT_REGISTRATION_KNOWN_MASK) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_REGISTRATION_INCOMPLETE,
                             diagnostic, ZR_HOST_JIT_REGISTRATION_KNOWN_MASK,
                             facts->registrationFlags, 0u, 0u, 0u);
    }
    if (facts->operationMask == 0u ||
        (facts->operationMask & ~ZR_HOST_JIT_OPERATION_KNOWN_MASK) != 0u ||
        facts->signatureHash == 0u || facts->layoutHash != facts->target.layoutHash ||
        facts->abiHash == 0u || facts->gcMapHash == 0u ||
        facts->unwindMapHash == 0u || facts->debugMapHash == 0u ||
        facts->deoptMapHash == 0u || facts->codeSize == 0u ||
        facts->codeIdentity == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_CODE_INVALID, diagnostic,
                             1u, 0u, facts->target.layoutHash,
                             facts->layoutHash, 0u);
    }
    if ((facts->requiredCapabilities & ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u ||
        (facts->declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_FLAGS, diagnostic,
                             ZR_EXECUTION_CAPABILITY_KNOWN_MASK,
                             facts->requiredCapabilities, 0u,
                             facts->declaredEffects, 0u);
    }
    return host_jit_validate_manifest(facts->imports, diagnostic);
}

/*
 * 串行化同一 manager 的记录与计数观察。
 * MSVC 与其他编译器使用各自原子交换；自旋锁不可重入，存储须在整个操作期间有效。
 */
static void host_jit_lock(SZrHostJitCodeManager *manager) {
#if defined(_MSC_VER)
    while (_InterlockedExchange((volatile long *)&manager->lock, 1L) != 0L) {
    }
#else
    while (__sync_lock_test_and_set(&manager->lock, 1u) != 0u) {
    }
#endif
}

/*
 * 结束 manager 的互斥观察并允许后续记录操作。
 * 须由持锁路径配对调用；volatile 字段本身不能替代这对锁操作。
 */
static void host_jit_unlock(SZrHostJitCodeManager *manager) {
#if defined(_MSC_VER)
    _InterlockedExchange((volatile long *)&manager->lock, 0L);
#else
    __sync_lock_release(&manager->lock);
#endif
}

/*
 * 拒绝无法解释的记录状态数值。
 * 枚举当前连续从 FREE 到 RETIRED；具体状态与字段关系另由 shape 验证。
 */
static TZrBool host_jit_code_state_valid(TZrUInt32 state) {
    return (TZrBool)(state <= (TZrUInt32)ZR_HOST_JIT_CODE_RETIRED);
}

/*
 * 保证记录数组容量在地址宽度下可形成字节跨度。
 * 32 位地址分支限制乘积；不证明调用方实际提供了足够、对齐且存活的数组。
 */
static TZrBool host_jit_capacity_bytes_valid(TZrUInt32 capacity) {
#if UINTPTR_MAX <= UINT32_MAX
    return (TZrBool)(capacity != 0u &&
                     capacity <=
                         (TZrUInt32)(UINTPTR_MAX /
                                     sizeof(SZrHostJitCodeRecord)));
#else
    return (TZrBool)(capacity != 0u);
#endif
}

/*
 * 在解引用外来记录指针前核对数组范围和记录边界。
 * 整数地址检查只证明声明范围内的位置；manager 和底层数组的有效生命周期仍由调用方保证。
 */
static TZrBool host_jit_belongs(const SZrHostJitCodeManager *manager,
                                const SZrHostJitCodeRecord *record) {
    uintptr_t begin;
    uintptr_t span;
    uintptr_t candidate;
    if (manager == ZR_NULL || record == ZR_NULL || manager->records == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!host_jit_capacity_bytes_valid(manager->capacity)) {
        return ZR_FALSE;
    }
    begin = (uintptr_t)manager->records;
    span = (uintptr_t)manager->capacity * sizeof(*manager->records);
    if (begin > ((uintptr_t)-1) - span) return ZR_FALSE;
    candidate = (uintptr_t)record;
    return (TZrBool)(candidate >= begin && candidate < begin + span &&
                     ((candidate - begin) % sizeof(*manager->records)) == 0u);
}

/*
 * 先核对借用数组的外壳，避免完整扫描使用明显失效的容量。
 * 调用者须持 manager 锁；合法容量不等于实际分配长度证明。
 */
static TZrBool host_jit_manager_shell_valid(
        const SZrHostJitCodeManager *manager) {
    if (manager == ZR_NULL || manager->records == ZR_NULL ||
        manager->capacity == 0u || manager->count > manager->capacity) {
        return ZR_FALSE;
    }
    return host_jit_capacity_bytes_valid(manager->capacity);
}

/* Validate the caller-owned manager shell while its lock is held.  All
 * mutable manager fields are therefore observed as one shape, even when a
 * concurrent operation is publishing, collecting, or deinitializing code. */
/*
 * 在同一次持锁观察中确认记录、计数、唯一身份与 active 一致。
 * 扫描整个固定容量；FREE 必须全零，PREPARED 不得有 lease，PUBLISHED 至多一条。
 */
static TZrBool host_jit_manager_shape_valid(
        const SZrHostJitCodeManager *manager) {
    TZrUInt32 index;
    TZrUInt32 used = 0u;
    TZrUInt32 published = 0u;
    if (!host_jit_manager_shell_valid(manager)) {
        return ZR_FALSE;
    }
    /*
     * 将数组、唯一身份、计数与 active 作为同一次观察验证。
     * 调用者持锁；普通局部 guard 均归此函数，不对外部数组分配作证明。
     */
    for (index = 0u; index < manager->capacity; ++index) {
        const SZrHostJitCodeRecord *record = &manager->records[index];
        if (!host_jit_code_state_valid(record->state)) return ZR_FALSE;
        if (record->state == ZR_HOST_JIT_CODE_FREE) {
            if (record->codeIdentity != 0u || record->signatureHash != 0u ||
                record->layoutHash != 0u || record->abiHash != 0u ||
                record->leaseCount != 0u) {
                return ZR_FALSE;
            }
        } else {
            if (used == UINT32_MAX) return ZR_FALSE;
            ++used;
            if (record->codeIdentity == 0u || record->signatureHash == 0u ||
                record->layoutHash == 0u || record->abiHash == 0u) {
                return ZR_FALSE;
            }
            if (record->state == ZR_HOST_JIT_CODE_PREPARED &&
                record->leaseCount != 0u) {
                return ZR_FALSE;
            }
            if (record->state == ZR_HOST_JIT_CODE_PUBLISHED) {
                if (published == UINT32_MAX) return ZR_FALSE;
                ++published;
            }
            for (TZrUInt32 prior = 0u; prior < index; ++prior) {
                const SZrHostJitCodeRecord *other = &manager->records[prior];
                if (other->state != ZR_HOST_JIT_CODE_FREE &&
                    other->codeIdentity == record->codeIdentity) {
                    return ZR_FALSE;
                }
            }
        }
    }
    if (used != manager->count || published > 1u) {
        return ZR_FALSE;
    }
    if (manager->active == ZR_NULL) {
        return (TZrBool)(published == 0u);
    }
    return (TZrBool)(published == 1u &&
                     host_jit_belongs(manager, manager->active) &&
                     manager->active->state == ZR_HOST_JIT_CODE_PUBLISHED);
}

/*
 * 给公共记录操作统一提供持锁完整验证入口。
 * 保留调用层次；不获取锁，也不允许无锁直接调用。
 */
static TZrBool host_jit_manager_valid_locked(
        const SZrHostJitCodeManager *manager) {
    return host_jit_manager_shape_valid(manager);
}

/*
 * 在尝试获取锁前拒绝空 manager 参数。
 * 只核指针非空；可变外壳和全部记录须在锁内另核。
 */
static EZrHostJitStatus host_jit_require_manager(
        const SZrHostJitCodeManager *manager,
        SZrHostJitDiagnostic *diagnostic) {
    if (manager == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 把调用方的固定记录数组接入新 manager 并建立全空状态。
 * 借用并清零整个数组，不分配或释放存储；初始化须独占且不可覆盖尚有记录或 lease 的实例。
 */
EZrHostJitStatus ZrCore_HostJit_CodeManager_Init(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeRecord *records,
        TZrUInt32 capacity,
        SZrHostJitDiagnostic *diagnostic) {
    host_jit_clear(diagnostic);
    if (manager == ZR_NULL || records == ZR_NULL ||
        !host_jit_capacity_bytes_valid(capacity)) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    memset(manager, 0, sizeof(*manager));
    manager->lock = 0u;
    manager->active = ZR_NULL;
    manager->records = records;
    manager->capacity = capacity;
    manager->count = 0u;
    memset(records, 0, (TZrSize)capacity * sizeof(*records));
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 仅在完整且已排空的 manager 上解除对数组的借用。
 * void 入口在有记录或外壳失效时保留原状态；调用方须确认 records 已为空后才释放数组。
 */
void ZrCore_HostJit_CodeManager_Deinit(SZrHostJitCodeManager *manager) {
    if (manager == ZR_NULL) {
        return;
    }
    host_jit_lock(manager);
    /*
     * 有活跃或未收集记录时保留 manager，允许所有者继续清理。
     * void 入口无法返回失败；records 清空前不得释放借用存储。
     */
    if (!host_jit_manager_valid_locked(manager) ||
        manager->active != ZR_NULL || manager->count != 0u) {
        /* A void deinit API cannot report failure.  Leave the manager intact
         * when a lease, prepared record, or retired record is still live so a
         * caller can release/collect it and retry. */
        host_jit_unlock(manager);
        return;
    }
    manager->active = ZR_NULL;
    manager->records = ZR_NULL;
    manager->capacity = 0u;
    manager->count = 0u;
    host_jit_unlock(manager);
}

/*
 * 把已验证发布声明的身份与三个 hash 放入空记录。
 * 输出是未租用的准备句柄；不保存 imports/状态图，不生成机器码；调用方须提供无活跃 lease 的独立输出。
 * facts/manager 前置校验失败时输出可能仍为原值；进入记录操作前才清零，失败时不能把输出当作新准备句柄。
 */
EZrHostJitStatus ZrCore_HostJit_Code_Prepare(
        SZrHostJitCodeManager *manager,
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *outHandle,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (facts == ZR_NULL || outHandle == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    status = ZrCore_HostJit_ValidatePublication(facts, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) {
        return status;
    }
    memset(outHandle, 0, sizeof(*outHandle));
    host_jit_lock(manager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    for (TZrUInt32 j = 0u; j < manager->capacity; ++j) {
        if (manager->records[j].state != ZR_HOST_JIT_CODE_FREE &&
            manager->records[j].codeIdentity == facts->codeIdentity) {
            /* TODO: 重复身份与容量失败分支在解锁后读取 manager/record 作为诊断；需确认公开直接并发调用的约束，或改为锁内标量快照。 */
            host_jit_unlock(manager);
            return host_jit_fail(ZR_HOST_JIT_STATUS_CODE_INVALID,
                                 diagnostic, 0u, 0u, facts->codeIdentity,
                                 manager->records[j].codeIdentity, j);
        }
    }
    for (TZrUInt32 i = 0u; i < manager->capacity; ++i) {
        SZrHostJitCodeRecord *record = &manager->records[i];
        if (record->state == ZR_HOST_JIT_CODE_FREE) {
            if (manager->count == UINT32_MAX || manager->count >= manager->capacity) {
                host_jit_unlock(manager);
                return host_jit_fail(ZR_HOST_JIT_STATUS_CAPACITY, diagnostic,
                                     manager->capacity, manager->count,
                                     0u, 0u, i);
            }
            record->codeIdentity = facts->codeIdentity;
            record->signatureHash = facts->signatureHash;
            record->layoutHash = facts->layoutHash;
            record->abiHash = facts->abiHash;
            record->state = ZR_HOST_JIT_CODE_PREPARED;
            record->leaseCount = 0u;
            if (manager->count < manager->capacity) {
                ++manager->count;
            }
            outHandle->record = record;
            outHandle->codeIdentity = record->codeIdentity;
            host_jit_unlock(manager);
            return ZR_HOST_JIT_STATUS_OK;
        }
    }
    host_jit_unlock(manager);
    return host_jit_fail(ZR_HOST_JIT_STATUS_CAPACITY, diagnostic,
                         manager->capacity, manager->capacity, 0u, 0u, 0u);
}

/*
 * 使准备记录成为唯一 active，并让旧发布记录进入退休状态。
 * 仅接受本 manager 的未租用 PREPARED 句柄；不会取得 lease 或清空 prepared，也不发布可执行地址。
 */
EZrHostJitStatus ZrCore_HostJit_Code_Publish(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *prepared,
    SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (prepared == ZR_NULL || prepared->record == ZR_NULL ||
        prepared->leased) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    host_jit_lock(manager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    if (!host_jit_belongs(manager, prepared->record) ||
        prepared->record->codeIdentity != prepared->codeIdentity ||
        prepared->record->state != ZR_HOST_JIT_CODE_PREPARED) {
        TZrUInt32 actualState = host_jit_belongs(manager, prepared->record)
                ? prepared->record->state : ZR_HOST_JIT_CODE_FREE;
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_NOT_PREPARED, diagnostic,
                             ZR_HOST_JIT_CODE_PREPARED,
                             actualState, 0u,
                             prepared->codeIdentity, 0u);
    }
    {
        /*
         * 替换 active 时先退休旧记录，保留既有 lease 的可解析性。
         * 不减旧 lease，不回收资源；最后归还后另 CollectRetired。
         */
        SZrHostJitCodeRecord *old = manager->active;
        if (old != ZR_NULL && old != prepared->record &&
            old->state == ZR_HOST_JIT_CODE_PUBLISHED) {
            old->state = ZR_HOST_JIT_CODE_RETIRED;
        }
    }
    prepared->record->state = ZR_HOST_JIT_CODE_PUBLISHED;
    manager->active = prepared->record;
    host_jit_unlock(manager);
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 为当前已发布记录取得一份须归还的 lease。
 * 输出不得覆盖已有 lease；退休可与既有 lease 并存，记录数组必须保持有效直到归还。
 * 空 manager 等前置失败不保证清空输出；只有成功返回才表示取得新 lease。
 */
EZrHostJitStatus ZrCore_HostJit_Code_AcquireActive(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *outHandle,
        SZrHostJitDiagnostic *diagnostic) {
    SZrHostJitCodeRecord *record;
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (outHandle == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    memset(outHandle, 0, sizeof(*outHandle));
    host_jit_lock(manager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    record = manager->active;
    if (record == ZR_NULL || record->state != ZR_HOST_JIT_CODE_PUBLISHED) {
        /* TODO: 失败诊断在解锁后读取 record 状态或 leaseCount；facade 已外层串行，公开 core 并发消费仍需核清，不能据此保证一致快照。 */
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_STALE_HANDLE, diagnostic,
                             ZR_HOST_JIT_CODE_PUBLISHED,
                             record == ZR_NULL ? ZR_HOST_JIT_CODE_FREE : record->state,
                             0u, 0u, 0u);
    }
    if (record->leaseCount == UINT32_MAX) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_OVERFLOW, diagnostic,
                             UINT32_MAX, record->leaseCount, 0u, 0u, 0u);
    }
    ++record->leaseCount;
    outHandle->record = record;
    outHandle->codeIdentity = record->codeIdentity;
    outHandle->leased = ZR_TRUE;
    host_jit_unlock(manager);
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 按身份退休记录，阻止其继续作为 active 被获取。
 * PREPARED 也可退休；已有 lease 不减少，实际回收由 CollectRetired 在归还后完成。
 */
EZrHostJitStatus ZrCore_HostJit_Code_Evict(
        SZrHostJitCodeManager *manager,
        TZrUInt64 codeIdentity,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (codeIdentity == 0u) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    host_jit_lock(manager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    for (TZrUInt32 i = 0u; i < manager->capacity; ++i) {
        SZrHostJitCodeRecord *record = &manager->records[i];
        if (record->codeIdentity == codeIdentity &&
            record->state != ZR_HOST_JIT_CODE_FREE) {
            if (record->state == ZR_HOST_JIT_CODE_RETIRED) {
                host_jit_unlock(manager);
                return ZR_HOST_JIT_STATUS_OK;
            }
            record->state = ZR_HOST_JIT_CODE_RETIRED;
            if (manager->active == record) {
                manager->active = ZR_NULL;
            }
            host_jit_unlock(manager);
            return ZR_HOST_JIT_STATUS_OK;
        }
    }
    host_jit_unlock(manager);
    return host_jit_fail(ZR_HOST_JIT_STATUS_STALE_HANDLE, diagnostic,
                         1u, 0u, codeIdentity, 0u, 0u);
}

/*
 * 在 lease 有效期间取得记录元数据的同次持锁快照。
 * RETIRED 仍可解析；const manager 仍会修改锁，须来自可写实例；view 不是入口地址或新 lease。
 * 无效输入的早期失败不保证清空 view；仅成功快照可供调用方观察。
 */
EZrHostJitStatus ZrCore_HostJit_Code_Resolve(
        const SZrHostJitCodeManager *manager,
        const SZrHostJitCodeHandle *handle,
        SZrHostJitCodeView *outView,
        SZrHostJitDiagnostic *diagnostic) {
    SZrHostJitCodeManager *mutableManager;
    SZrHostJitCodeRecord *record;
    EZrHostJitStatus status;
    host_jit_clear(diagnostic);
    if (handle == ZR_NULL || outView == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    if (handle->record == ZR_NULL || !handle->leased) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_STALE_HANDLE, diagnostic,
                             1u, 0u, handle->codeIdentity,
                             0u, 0u);
    }
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    memset(outView, 0, sizeof(*outView));
    /*
     * 以可写锁保护对 const 视图的一次元数据读取。
     * 只返回标量快照；底层实例必须可写且仍存活，不代表机器码地址。
     */
    mutableManager = (SZrHostJitCodeManager *)(void *)manager;
    host_jit_lock(mutableManager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(mutableManager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    record = handle->record;
    if (!host_jit_belongs(manager, record) ||
        record->codeIdentity != handle->codeIdentity ||
        record->state == ZR_HOST_JIT_CODE_FREE) {
        host_jit_unlock(mutableManager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_STALE_HANDLE, diagnostic,
                             1u, 0u, handle->codeIdentity, 0u, 0u);
    }
    if (record->leaseCount == 0u) {
        host_jit_unlock(mutableManager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_STATE, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    outView->codeIdentity = record->codeIdentity;
    outView->signatureHash = record->signatureHash;
    outView->layoutHash = record->layoutHash;
    outView->abiHash = record->abiHash;
    outView->state = (EZrHostJitCodeState)record->state;
    outView->leaseCount = record->leaseCount;
    host_jit_unlock(mutableManager);
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 归还传入句柄代表的一份 lease，并清空该句柄。
 * 成功后该句柄不可再用；不自动回收退休记录，复制 leased 句柄不能产生另一份拥有权。
 * 失败路径保留传入句柄；不能把失败当作 lease 已归还。
 */
EZrHostJitStatus ZrCore_HostJit_Code_Release(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *handle,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    SZrHostJitCodeRecord *record;
    TZrUInt32 leaseCount;
    host_jit_clear(diagnostic);
    if (handle == ZR_NULL || handle->record == ZR_NULL || !handle->leased) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    host_jit_lock(manager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    record = handle->record;
    if (!host_jit_belongs(manager, record) ||
        record->codeIdentity != handle->codeIdentity ||
        record->state == ZR_HOST_JIT_CODE_FREE) {
        /* TODO: stale 分支解锁后再次判断归属并读取 identity；需核清外部直接并发调用和存储生命周期，再确定诊断是否需锁内快照。 */
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_STALE_HANDLE, diagnostic,
                             1u, 0u, handle->codeIdentity,
                             host_jit_belongs(manager, record)
                                 ? record->codeIdentity : 0u,
                             0u);
    }
    leaseCount = record->leaseCount;
    if (leaseCount == 0u) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_STATE, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    --record->leaseCount;
    handle->record = ZR_NULL;
    handle->codeIdentity = 0u;
    handle->leased = ZR_FALSE;
    host_jit_unlock(manager);
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 在最后一份 lease 归还后把退休记录恢复为空槽。
 * 只清除 core 元数据并报告数量；外部 proof 与可执行资源由其所有者另行管理。
 * 非空 outCollected 在 manager 校验前置零；成功计数仅表示本层清零的记录数。
 */
EZrHostJitStatus ZrCore_HostJit_Code_CollectRetired(
        SZrHostJitCodeManager *manager,
        TZrUInt32 *outCollected,
        SZrHostJitDiagnostic *diagnostic) {
    EZrHostJitStatus status;
    TZrUInt32 collected = 0u;
    host_jit_clear(diagnostic);
    if (outCollected == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    *outCollected = 0u;
    status = host_jit_require_manager(manager, diagnostic);
    if (status != ZR_HOST_JIT_STATUS_OK) return status;
    host_jit_lock(manager);
    if (!host_jit_manager_valid_locked(manager)) {
        host_jit_unlock(manager);
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    for (TZrUInt32 i = 0u; i < manager->capacity; ++i) {
        SZrHostJitCodeRecord *record = &manager->records[i];
        /*
         * 退休且无 lease 才清零槽位，允许随后重新准备。
         * 只回收记录元数据；不能据此证明外部代码页或 proof 已释放。
         */
        if (record->state == ZR_HOST_JIT_CODE_RETIRED &&
            record->leaseCount == 0u) {
            memset(record, 0, sizeof(*record));
            if (manager->count != 0u) {
                --manager->count;
            }
            ++collected;
        }
    }
    host_jit_unlock(manager);
    *outCollected = collected;
    return ZR_HOST_JIT_STATUS_OK;
}

/*
 * 为 core 状态提供借用的静态诊断名称。
 * 未知值返回通用名称，调用方不得释放返回字符串；TODO: 当前全后缀检索无正向调用，需确认仓库外公开 ABI 消费者。
 */
const TZrChar *ZrCore_HostJit_StatusName(EZrHostJitStatus status) {
    switch (status) {
        case ZR_HOST_JIT_STATUS_OK: return "OK";
        case ZR_HOST_JIT_STATUS_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ZR_HOST_JIT_STATUS_SCHEMA_MISMATCH: return "SCHEMA_MISMATCH";
        case ZR_HOST_JIT_STATUS_TARGET_UNSUPPORTED: return "TARGET_UNSUPPORTED";
        case ZR_HOST_JIT_STATUS_TARGET_MISMATCH: return "TARGET_MISMATCH";
        case ZR_HOST_JIT_STATUS_ABI_MISMATCH: return "ABI_MISMATCH";
        case ZR_HOST_JIT_STATUS_LAYOUT_MISMATCH: return "LAYOUT_MISMATCH";
        case ZR_HOST_JIT_STATUS_INVALID_FLAGS: return "INVALID_FLAGS";
        case ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN: return "IMPORT_FORBIDDEN";
        case ZR_HOST_JIT_STATUS_IMPORT_INVALID: return "IMPORT_INVALID";
        case ZR_HOST_JIT_STATUS_OPERATION_UNSUPPORTED: return "OPERATION_UNSUPPORTED";
        case ZR_HOST_JIT_STATUS_REGISTRATION_INCOMPLETE: return "REGISTRATION_INCOMPLETE";
        case ZR_HOST_JIT_STATUS_WX_REQUIRED: return "WX_REQUIRED";
        case ZR_HOST_JIT_STATUS_CODE_INVALID: return "CODE_INVALID";
        case ZR_HOST_JIT_STATUS_CAPACITY: return "CAPACITY";
        case ZR_HOST_JIT_STATUS_NOT_PREPARED: return "NOT_PREPARED";
        case ZR_HOST_JIT_STATUS_STALE_HANDLE: return "STALE_HANDLE";
        case ZR_HOST_JIT_STATUS_ACTIVE_LEASE: return "ACTIVE_LEASE";
        case ZR_HOST_JIT_STATUS_INVALID_STATE: return "INVALID_STATE";
        case ZR_HOST_JIT_STATUS_OVERFLOW: return "OVERFLOW";
        default: return "HOST_JIT_ERROR";
    }
}
