/* Focused optional-module contract test for SSA plan 10.02. */
#include "zr_vm_jit/backend.h"

#include <assert.h>
#include <string.h>

static EZrHostJitArchitecture test_host_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return ZR_HOST_JIT_ARCH_X86_64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ZR_HOST_JIT_ARCH_AARCH64;
#else
    return ZR_HOST_JIT_ARCH_NONE;
#endif
}

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

    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_ValidateStateMaps(&maps, &diagnostic) ==
           ZR_JIT_HOST_STATUS_OK);
    maps.debugEntryCount = 0u;
    assert(ZrJit_Host_ValidateStateMaps(&maps, &diagnostic) ==
           ZR_JIT_HOST_STATUS_STATE_MAP_INVALID);
    maps = test_maps();

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

    publication.target.platform = ZR_HOST_JIT_PLATFORM_WASM;
    ZrJit_Host_DiagnosticInit(&diagnostic);
    assert(ZrJit_Host_ValidateImport(ZR_NULL, 1u, 2u, &diagnostic) ==
           ZR_JIT_HOST_STATUS_INVALID_ARGUMENT);
    assert(ZrJit_Host_Register(&options, &executionDiagnostic) == ZR_FALSE);
    assert(executionDiagnostic.code == ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT ||
           executionDiagnostic.code == ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    publication = test_publication();

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
