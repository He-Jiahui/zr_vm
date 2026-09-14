#include "zr_vm_core/host_baseline_jit.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static EZrHostJitArchitecture host_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return ZR_HOST_JIT_ARCH_X86_64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ZR_HOST_JIT_ARCH_AARCH64;
#else
    return ZR_HOST_JIT_ARCH_NONE;
#endif
}

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
