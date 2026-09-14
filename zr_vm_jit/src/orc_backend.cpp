#include "zr_vm_jit/backend.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace {

/* Callback declarations are kept private to this translation unit; the
 * public descriptor only exposes their C ABI function pointers. */
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

struct SZrJitPublicationProof {
    TZrUInt64 codeIdentity;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 abiHash;
    TZrUInt32 publicationFlags;
    TZrUInt32 registrationFlags;
    TZrUInt32 operationMask;
    SZrJitStateMapFacts stateMaps;
};

struct HostImpl {
    SZrHostJitOptions options{};
    std::vector<SZrHostJitCodeRecord> records;
    std::vector<SZrJitPublicationProof> proofs;
    SZrHostJitCodeManager manager{};
    EZrJitHostState state = ZR_JIT_HOST_STATE_UNINITIALIZED;
    TZrBool machineCodeAvailable = ZR_FALSE;
    TZrBool fallbackAvailable = ZR_FALSE;
    TZrUInt64 backendIdentity = 0u;
};

std::mutex &global_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::unique_ptr<HostImpl> &global_impl() {
    static std::unique_ptr<HostImpl> impl;
    return impl;
}

void clear_execution_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_NONE;
    }
}

void clear_jit_diagnostic(SZrJitHostDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_JIT_HOST_STATUS_OK;
        diagnostic->coreStatus = ZR_HOST_JIT_STATUS_OK;
    }
}

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

EZrJitHostStatus core_operation(EZrHostJitStatus status,
                                 SZrJitHostDiagnostic *diagnostic,
                                 const SZrHostJitDiagnostic *coreDiagnostic) {
    if (status == ZR_HOST_JIT_STATUS_OK) {
        clear_jit_diagnostic(diagnostic);
        return ZR_JIT_HOST_STATUS_OK;
    }
    return map_core_status(status, diagnostic, coreDiagnostic);
}

TZrUInt64 backend_identity(const SZrHostJitOptions &options) {
    /* This is a stable process/profile identity, not an address. */
    TZrUInt64 value = 0x5a524a4954000001ULL;
    value ^= ((TZrUInt64)options.target.architecture << 8u);
    value ^= options.target.targetTripleHash;
    value ^= (options.target.layoutHash << 1u);
    return value == 0u ? 1u : value;
}

TZrUInt32 record_capacity(const SZrHostJitOptions &options) {
    const TZrUInt32 recordSize = (TZrUInt32)sizeof(SZrHostJitCodeRecord);
    if (recordSize == 0u || options.codeCacheBytes < recordSize) return 0u;
    TZrUInt32 capacity = options.codeCacheBytes / recordSize;
    /* A hostile configuration must not turn registration into an unbounded
     * C++ allocation.  The caller can increase this cap deliberately later. */
    return std::min<TZrUInt32>(capacity, 4096u);
}

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
    /* Callbacks resolve the current global adapter under the mutex.  Keeping
     * userData null prevents a descriptor copied into the core service from
     * becoming a dangling pointer when the optional module is shut down. */
    descriptor->userData = ZR_NULL;
}

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

void destroy_backend(TZrPtr userData) {
    (void)userData;
}

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

void erase_collected_proofs(HostImpl &impl) {
    impl.proofs.erase(
            std::remove_if(impl.proofs.begin(), impl.proofs.end(),
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

extern "C" void ZrJit_Host_DiagnosticInit(SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
}

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
    if (request->stateMaps.frameLayoutHash != request->publication.layoutHash) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_TARGET_MISMATCH,
                        1u, 0u, request->publication.layoutHash,
                        request->stateMaps.frameLayoutHash);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

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

extern "C" EZrJitHostStatus ZrJit_Host_Prepare(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *handle,
        SZrJitHostDiagnostic *diagnostic) {
    SZrJitStateMapFacts stateMaps;
    clear_jit_diagnostic(diagnostic);
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

extern "C" EZrJitHostStatus ZrJit_Host_Compile(
        const SZrJitHostCompileRequest *request,
        SZrHostJitCodeHandle *outHandle,
        SZrJitHostDiagnostic *diagnostic) {
    clear_jit_diagnostic(diagnostic);
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
    if ((request->flags & ZR_JIT_HOST_OPTION_REQUIRE_MACHINE_CODE) != 0u ||
        (request->flags & ZR_JIT_HOST_OPTION_ALLOW_FALLBACK) == 0u ||
        !impl->fallbackAvailable) {
        return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_BACKEND_UNAVAILABLE,
                        1u, 0u, 0u, 0u, request->sourceIndex);
    }
    return fail_jit(diagnostic, ZR_JIT_HOST_STATUS_FALLBACK_EXECBC,
                    1u, 0u, 0u, 0u, request->sourceIndex);
}
