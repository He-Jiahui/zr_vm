/* Focused optional-module contract test for SSA plan 10.02. */
#include "zr_vm_jit/backend.h"

#include <assert.h>
#include <string.h>

/* 按编译目标宏选择 optional facade 的主机架构见证。
 * 只支持 x86-64/AArch64 正向注册；没有跨架构执行。 */
static EZrHostJitArchitecture test_host_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return ZR_HOST_JIT_ARCH_X86_64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ZR_HOST_JIT_ARCH_AARCH64;
#else
    return ZR_HOST_JIT_ARCH_NONE;
#endif
}

/* 按传入平台构造 optional facade 目标，正向场景使用 HOST，末尾以 ANDROID 检查拒绝。
 * pointerSize 来自 sizeof(void*)；endianness 固定 little-endian；无系统目标探测。 */
static SZrHostJitTargetContract test_target(EZrHostJitPlatform platform) {
    SZrHostJitTargetContract target;
    memset(&target, 0, sizeof(target));
    target.schemaVersion = ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION;
    target.architecture = test_host_architecture();
    target.platform = platform;
    target.pointerSize = (TZrUInt32)sizeof(void *);
    target.endianness = ZR_HOST_JIT_ENDIAN_LITTLE;
    target.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    target.targetTripleHash = 1001u;
    target.layoutHash = 1002u;
    return target;
}

/* 为 optional facade 准备四图已注册的发布事实，与 maps 使用相同固定 hash。
 * 只有元数据；注册和记录发布不证明可执行入口存在。 */
static SZrHostJitPublicationFacts test_publication(void) {
    SZrHostJitPublicationFacts facts;
    memset(&facts, 0, sizeof(facts));
    facts.target = test_target(ZR_HOST_JIT_PLATFORM_HOST);
    facts.flags = ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE |
                  ZR_HOST_JIT_PUBLICATION_FLAG_WX;
    facts.registrationFlags = ZR_HOST_JIT_REGISTRATION_ROOTS |
                              ZR_HOST_JIT_REGISTRATION_UNWIND |
                              ZR_HOST_JIT_REGISTRATION_DEBUG |
                              ZR_HOST_JIT_REGISTRATION_DEOPT;
    facts.operationMask = ZR_HOST_JIT_OPERATION_TYPED_SCALAR |
                          ZR_HOST_JIT_OPERATION_CONTROL |
                          ZR_HOST_JIT_OPERATION_DIRECT_CALL |
                          ZR_HOST_JIT_OPERATION_SIMPLE_MEMBER |
                          ZR_HOST_JIT_OPERATION_ARRAY;
    facts.signatureHash = 2001u;
    facts.layoutHash = 1002u;
    facts.abiHash = 2002u;
    facts.gcMapHash = 3001u;
    facts.unwindMapHash = 3002u;
    facts.debugMapHash = 3003u;
    facts.deoptMapHash = 3004u;
    facts.codeSize = 64u;
    facts.codeIdentity = 4001u;
    return facts;
}

/* 为 facade 构造四类计数、hash 和 frame layout 的一致声明。
 * 每类计数仅为一；没有实际读取或验证状态图条目。 */
static SZrJitStateMapFacts test_maps(void) {
    SZrJitStateMapFacts maps;
    memset(&maps, 0, sizeof(maps));
    maps.schemaVersion = ZR_JIT_HOST_SCHEMA_VERSION;
    maps.registrationFlags = ZR_JIT_HOST_MAP_KNOWN_MASK;
    maps.rootEntryCount = 1u;
    maps.unwindEntryCount = 1u;
    maps.debugEntryCount = 1u;
    maps.deoptEntryCount = 1u;
    /* The publication proof carries the canonical hashes for these maps. */
    maps.rootMapHash = 3001u;
    maps.unwindMapHash = 3002u;
    maps.debugMapHash = 3003u;
    maps.deoptMapHash = 3004u;
    maps.frameLayoutHash = 1002u;
    return maps;
}

/* 从已注册 facade 取得 C ABI descriptor，再实际调用 queryTarget 核对无 provider 的不可用状态。
 * info 来自 QueryInfo；不调用 descriptor 编译回调，不能据此宣称生成机器码。 */
static void test_descriptor_unavailable(const SZrJitHostBackendInfo *info) {
    SZrExecutionBackendDescriptor descriptor;
    SZrJitHostDiagnostic diagnostic;
    SZrExecutionBackendDiagnostic backendDiagnostic;
    EZrExecutionBackendStatus status;

    memset(&descriptor, 0, sizeof(descriptor));
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_GetDescriptor(&descriptor, &diagnostic) == ZR_TRUE);
    assert(descriptor.magic == ZR_EXECUTION_BACKEND_MAGIC);
    assert(descriptor.schemaVersion == ZR_EXECUTION_BACKEND_SCHEMA_VERSION);
    assert(descriptor.backendKind == ZR_EXECUTION_BACKEND_TARGET_HOST_JIT);
    assert(descriptor.supportedOperations == info->supportedOperations);
    assert(descriptor.vtable.queryTarget != ZR_NULL);
    memset(&backendDiagnostic, 0, sizeof(backendDiagnostic));
    status = descriptor.vtable.queryTarget(
            &descriptor.target,
            ZR_EXECUTION_BACKEND_OPERATION_TYPED_SCALAR,
            descriptor.userData,
            &backendDiagnostic);
    assert(status == ZR_EXECUTION_BACKEND_STATUS_BACKEND_UNAVAILABLE);
    assert(backendDiagnostic.status == status);
}

/* 由启用 ZR_VM_ENABLE_HOST_JIT 的 CTest 入口核对无 provider 注册、fallback 和记录 lease 生命周期。
 * 只有一个 debugEntryCount=0 负例；编译返回 fallback 不执行 ExecBC，入口查询返回零。 */
int main(void) {
    SZrHostJitOptions options;
    SZrExecIrDiagnostic executionDiagnostic;
    SZrJitHostDiagnostic diagnostic;
    SZrJitHostBackendInfo info;
    SZrJitStateMapFacts maps = test_maps();
    SZrHostJitPublicationFacts publication = test_publication();
    SZrHostJitImport import;
    SZrHostJitImportManifest manifest;
    SZrHostJitCodeHandle prepared;
    SZrHostJitCodeHandle active;
    SZrJitHostCompileRequest request;
    TZrUInt32 collected = 0u;
    TZrNativePtr entry = 0;

    memset(&options, 0, sizeof(options));
    options.schemaVersion = ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION;
    options.flags = ZR_HOST_JIT_OPTION_FLAG_ENABLE |
                    ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK;
    options.target = test_target(ZR_HOST_JIT_PLATFORM_HOST);
    options.codeCacheBytes = 65536u;
    options.maxImports = 8u;

    /* 注册成功只表示 facade 状态可用；机器码不可用由能力查询和 descriptor 回调分别核对。 */
    memset(&executionDiagnostic, 0, sizeof(executionDiagnostic));
    assert(ZrJit_Host_Register(&options, &executionDiagnostic) == ZR_TRUE);
    assert(executionDiagnostic.code == ZR_EXECUTION_DIAGNOSTIC_NONE);
    assert(ZrJit_Host_IsRegistered() == ZR_TRUE);
    assert(ZrJit_Host_IsMachineCodeAvailable() == ZR_FALSE);
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_QueryInfo(&info, &diagnostic) == ZR_JIT_HOST_STATUS_OK);
    assert(info.state == ZR_JIT_HOST_STATE_REGISTERED);
    assert(info.fallbackAvailable == ZR_TRUE);
    assert(info.supportedOperations == ZR_HOST_JIT_OPERATION_KNOWN_MASK);
    test_descriptor_unavailable(&info);

    /* 只将 debugEntryCount 置零验证缺失条目拒绝，恢复全部声明后继续；没有 hash 错配负例。 */
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_ValidateStateMaps(&maps, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    maps.debugEntryCount = 0u;
    assert(ZrJit_Host_ValidateStateMaps(&maps, &diagnostic) ==
           ZR_JIT_HOST_STATUS_STATE_MAP_INVALID);
    maps = test_maps();

    /* 符号和签名共同约束导入；分别改签名与 symbolId 检查禁止状态。 */
    memset(&import, 0, sizeof(import));
    import.symbolId = 6001u;
    import.signatureHash = 6002u;
    import.kind = ZR_HOST_JIT_IMPORT_RUNTIME;
    memset(&manifest, 0, sizeof(manifest));
    manifest.schemaVersion = ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION;
    manifest.count = 1u;
    manifest.imports = &import;
    manifest.allowedSymbolHash = 6003u;
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_ValidateImport(&manifest, 6001u, 6002u, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_ValidateImport(&manifest, 6001u, 6004u, &diagnostic) ==
           ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN);
    assert(ZrJit_Host_ValidateImport(&manifest, 6005u, 6002u, &diagnostic) ==
           ZR_JIT_HOST_STATUS_IMPORT_FORBIDDEN);

    /* TODO: publication.platform 被改为 WASM，但下方 Register 仍使用 HOST options 且已有注册；不能证明 WASM 拒绝。下一步在独立行为修复中核对改 options 后的平台注册负例。 */
    publication.target.platform = ZR_HOST_JIT_PLATFORM_WASM;
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_ValidateImport(ZR_NULL, 1u, 2u, &diagnostic) ==
           ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    assert(ZrJit_Host_Register(&options, &executionDiagnostic) == ZR_FALSE);
    assert(executionDiagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT ||
           executionDiagnostic.code == ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    publication = test_publication();

    /* 合法请求允许 fallback，输出 prepared.record 仍为空；这里只核对回退决策，不运行解释器。 */
    memset(&request, 0, sizeof(request));
    request.magic = ZR_JIT_HOST_MAGIC;
    request.schemaVersion = ZR_JIT_HOST_SCHEMA_VERSION;
    request.flags = ZR_JIT_HOST_OPTION_ALLOW_FALLBACK;
    request.operationMask = ZR_HOST_JIT_OPERATION_TYPED_SCALAR;
    request.publication = publication;
    request.stateMaps = maps;
    request.sourceIndex = 77u;
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_ValidateCompileRequest(&request, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_Compile(&request, &prepared, &diagnostic) ==
           ZR_JIT_HOST_STATUS_FALLBACK_EXECBC);
    assert(prepared.record == ZR_NULL);

    /* 记录发布仍无可执行入口；Evict 后活跃 lease 阻止 Collect 与关闭，Release 后再回收关闭。 */
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_PrepareWithMaps(&publication, &maps, &prepared, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_Publish(&prepared, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_Acquire(&active, &diagnostic) == ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_LookupEntry(&active, &entry, &diagnostic) ==
           ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE);
    assert(entry == 0);
    assert(ZrJit_Host_Evict(publication.codeIdentity, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_Collect(&collected, &diagnostic) == ZR_JIT_HOST_STATUS_OK);
    assert(collected == 0u);
    assert(ZrJit_Host_TryShutdown(&diagnostic) == ZR_JIT_HOST_STATUS_ACTIVE_LEASE);
    assert(ZrJit_Host_Release(&active, &diagnostic) == ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_Collect(&collected, &diagnostic) == ZR_JIT_HOST_STATUS_OK);
    assert(collected == 1u);
    assert(ZrJit_Host_TryShutdown(&diagnostic) == ZR_JIT_HOST_STATUS_OK);
    assert(ZrJit_Host_IsRegistered() == ZR_FALSE);
    options.target.platform = ZR_HOST_JIT_PLATFORM_ANDROID;
    assert(ZrJit_Host_Register(&options, &executionDiagnostic) == ZR_FALSE);
    assert(executionDiagnostic.code == ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    ZrJit_Host_Shutdown();
    return 0;
}
