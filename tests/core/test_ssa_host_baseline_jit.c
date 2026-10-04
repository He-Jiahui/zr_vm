#include "zr_vm_core/host_baseline_jit.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* 按编译目标宏生成主机架构见证，供目标验证和异架构拒绝场景使用。
 * 仅 x86-64/AArch64 可走正向场景；其他架构返回 NONE 后首个 OK 断言不成立。 */
static EZrHostJitArchitecture host_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return ZR_HOST_JIT_ARCH_X86_64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ZR_HOST_JIT_ARCH_AARCH64;
#else
    return ZR_HOST_JIT_ARCH_NONE;
#endif
}

/* 构造 core host 契约见证，使目标和发布事实共享 layout hash。
 * 固定 8 字节指针与 little-endian；hash 是测试常量，不是实际机器布局计算。 */
static SZrHostJitTargetContract host_target(EZrHostJitArchitecture architecture) {
    SZrHostJitTargetContract target;
    memset(&target, 0, sizeof(target));
    target.schemaVersion = ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION;
    target.architecture = architecture;
    target.platform = ZR_HOST_JIT_PLATFORM_HOST;
    target.pointerSize = 8u;
    target.endianness = ZR_HOST_JIT_ENDIAN_LITTLE;
    target.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    target.targetTripleHash = architecture == ZR_HOST_JIT_ARCH_X86_64 ? 11u : 22u;
    target.layoutHash = 33u;
    return target;
}

/* 构造 manager 生命周期测试所需发布事实，再由 main 补全四图注册位。
 * 只声明 MACHINE_CODE/WX 和图 hash；不分配可执行页，也不执行代码。 */
static SZrHostJitPublicationFacts valid_facts(void) {
    SZrHostJitPublicationFacts facts;
    memset(&facts, 0, sizeof(facts));
    facts.target = host_target(host_architecture());
    facts.flags = ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE |
                  ZR_HOST_JIT_PUBLICATION_FLAG_WX;
    facts.signatureHash = 101u;
    facts.layoutHash = 33u;
    facts.abiHash = 202u;
    facts.operationMask = ZR_HOST_JIT_OPERATION_TYPED_SCALAR |
                          ZR_HOST_JIT_OPERATION_CONTROL |
                          ZR_HOST_JIT_OPERATION_DIRECT_CALL |
                          ZR_HOST_JIT_OPERATION_SIMPLE_MEMBER |
                          ZR_HOST_JIT_OPERATION_ARRAY;
    facts.requiredCapabilities = ZR_EXECUTION_CAPABILITY_ARITHMETIC |
                                 ZR_EXECUTION_CAPABILITY_MEMORY;
    facts.declaredEffects = ZR_EXECUTION_EFFECT_READ_MEMORY;
    facts.gcMapHash = 301u;
    facts.unwindMapHash = 302u;
    facts.debugMapHash = 303u;
    facts.deoptMapHash = 304u;
    facts.codeSize = 4096u;
    facts.codeIdentity = 401u;
    return facts;
}

/* 由 CTest 的 ssa_host_baseline_jit 入口核对契约拒绝与 lease 延迟回收。
 * 先直接改 leaseCount/count 注入边界，再恢复值；Deinit 在有退休记录时须保留 manager。 */
int main(void) {
    SZrHostJitDiagnostic diagnostic;
    SZrHostJitTargetContract target = host_target(host_architecture());
    SZrHostJitPublicationFacts facts = valid_facts();
    SZrHostJitOptions options;
    SZrHostJitImport imports[2];
    SZrHostJitImportManifest manifest;
    SZrHostJitCodeRecord records[2];
    SZrHostJitCodeManager manager;
    SZrHostJitCodeHandle first;
    SZrHostJitCodeHandle active;
    SZrHostJitCodeHandle duplicate;
    SZrHostJitCodeHandle overflow;
    SZrHostJitCodeView view;
    TZrUInt32 savedCount;
    TZrUInt32 collected;

    assert(ZrCore_HostJit_ValidateTarget(&target, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    /* 未知选项位、零缓存、WASM 与异主机架构是独立拒绝场景；没有修改 ABI 版本做负例。 */
    memset(&options, 0, sizeof(options));
    options.schemaVersion = ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION;
    options.flags = ZR_HOST_JIT_OPTION_FLAG_ENABLE |
                    ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK;
    options.target = target;
    options.codeCacheBytes = 65536u;
    options.maxImports = 8u;
    assert(ZrCore_HostJit_ValidateOptions(&options, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    options.flags |= ((TZrUInt32)1u << 7u);
    assert(ZrCore_HostJit_ValidateOptions(&options, &diagnostic) ==
           ZR_HOST_JIT_STATUS_INVALID_FLAGS);
    options.flags = ZR_HOST_JIT_OPTION_FLAG_ENABLE;
    options.codeCacheBytes = 0u;
    assert(ZrCore_HostJit_ValidateOptions(&options, &diagnostic) ==
           ZR_HOST_JIT_STATUS_CODE_INVALID);
    options.codeCacheBytes = 65536u;
    target.platform = ZR_HOST_JIT_PLATFORM_WASM;
    assert(ZrCore_HostJit_ValidateTarget(&target, &diagnostic) ==
           ZR_HOST_JIT_STATUS_TARGET_UNSUPPORTED);
    target = host_target(host_architecture());
    target.architecture = host_architecture() == ZR_HOST_JIT_ARCH_X86_64
                              ? ZR_HOST_JIT_ARCH_AARCH64
                              : ZR_HOST_JIT_ARCH_X86_64;
    assert(ZrCore_HostJit_ValidateTarget(&target, &diagnostic) ==
           ZR_HOST_JIT_STATUS_TARGET_MISMATCH);
    target = host_target(host_architecture());

    /* 用有效导入、未知 symbol 和重复 symbolId 检查白名单与 manifest 唯一性，随后移除借用指针。 */
    memset(imports, 0, sizeof(imports));
    imports[0].symbolId = 501u;
    imports[0].signatureHash = 502u;
    imports[0].kind = ZR_HOST_JIT_IMPORT_RUNTIME;
    memset(&manifest, 0, sizeof(manifest));
    manifest.schemaVersion = ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION;
    manifest.imports = imports;
    manifest.count = 1u;
    manifest.allowedSymbolHash = 503u;
    facts.imports = &manifest;
    assert(ZrCore_HostJit_ValidateImports(&manifest, 501u, 502u, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(ZrCore_HostJit_ValidateImports(&manifest, 999u, 502u, &diagnostic) ==
           ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN);
    imports[1] = imports[0];
    imports[1].signatureHash = 504u;
    manifest.count = 2u;
    assert(ZrCore_HostJit_ValidateImports(&manifest, 501u, 502u, &diagnostic) ==
           ZR_HOST_JIT_STATUS_IMPORT_INVALID);
    manifest.count = 1u;
    facts.imports = ZR_NULL;

    /* 单操作查询不接受多位组合；四图注册缺失的负例只保留 ROOTS。 */
    facts.registrationFlags = ZR_HOST_JIT_REGISTRATION_ROOTS |
                              ZR_HOST_JIT_REGISTRATION_UNWIND |
                              ZR_HOST_JIT_REGISTRATION_DEBUG |
                              ZR_HOST_JIT_REGISTRATION_DEOPT;
    assert(ZrCore_HostJit_ValidatePublication(&facts, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(ZrCore_HostJit_SupportsOperation(
               ZR_HOST_JIT_OPERATION_TYPED_SCALAR, facts.operationMask));
    assert(!ZrCore_HostJit_SupportsOperation(
               ZR_HOST_JIT_OPERATION_TYPED_SCALAR |
               ZR_HOST_JIT_OPERATION_CONTROL, facts.operationMask));
    facts.registrationFlags = ZR_HOST_JIT_REGISTRATION_ROOTS;
    assert(ZrCore_HostJit_ValidatePublication(&facts, &diagnostic) ==
           ZR_HOST_JIT_STATUS_REGISTRATION_INCOMPLETE);
    facts = valid_facts();
    facts.registrationFlags = ZR_HOST_JIT_REGISTRATION_ROOTS |
                              ZR_HOST_JIT_REGISTRATION_UNWIND |
                              ZR_HOST_JIT_REGISTRATION_DEBUG |
                              ZR_HOST_JIT_REGISTRATION_DEOPT;

    /* 直接注入 UINT32_MAX lease 和零 count 检查边界拒绝，再恢复 manager 状态以继续生命周期。 */
    assert(ZrCore_HostJit_CodeManager_Init(&manager, records, 2u, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(ZrCore_HostJit_Code_Prepare(&manager, &facts, &first, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(ZrCore_HostJit_Code_Publish(&manager, &first, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(ZrCore_HostJit_Code_Prepare(&manager, &facts, &duplicate, &diagnostic) ==
           ZR_HOST_JIT_STATUS_CODE_INVALID);
    assert(ZrCore_HostJit_Code_AcquireActive(&manager, &active, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    active.record->leaseCount = UINT32_MAX;
    assert(ZrCore_HostJit_Code_AcquireActive(&manager, &overflow, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OVERFLOW);
    active.record->leaseCount = 1u;
    savedCount = manager.count;
    manager.count = 0u;
    assert(ZrCore_HostJit_Code_Resolve(&manager, &active, &view, &diagnostic) ==
           ZR_HOST_JIT_STATUS_INVALID_ARGUMENT);
    manager.count = savedCount;
    /* 退休后 Deinit 必须保留仍被 lease 引用的记录；归还并收集后旧 handle 才失效。 */
    assert(ZrCore_HostJit_Code_Evict(&manager, active.codeIdentity, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    ZrCore_HostJit_CodeManager_Deinit(&manager);
    assert(manager.records != ZR_NULL);
    assert(manager.count == 1u);
    assert(ZrCore_HostJit_Code_Resolve(&manager, &active, &view, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(view.state == ZR_HOST_JIT_CODE_RETIRED);
    collected = 0u;
    assert(ZrCore_HostJit_Code_CollectRetired(&manager, &collected, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(collected == 0u);
    assert(ZrCore_HostJit_Code_Release(&manager, &active, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(ZrCore_HostJit_Code_CollectRetired(&manager, &collected, &diagnostic) ==
           ZR_HOST_JIT_STATUS_OK);
    assert(collected == 1u);
    assert(ZrCore_HostJit_Code_Resolve(&manager, &active, &view, &diagnostic) ==
           ZR_HOST_JIT_STATUS_STALE_HANDLE);
    ZrCore_HostJit_CodeManager_Deinit(&manager);
    return 0;
}
