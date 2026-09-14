#include "zr_vm_core/host_baseline_jit.h"

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static void host_jit_clear(SZrHostJitDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

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

static TZrBool host_jit_target_is_host(const SZrHostJitTargetContract *target) {
    return target != ZR_NULL && target->platform == ZR_HOST_JIT_PLATFORM_HOST &&
           (target->architecture == ZR_HOST_JIT_ARCH_X86_64 ||
            target->architecture == ZR_HOST_JIT_ARCH_AARCH64);
}

static EZrHostJitArchitecture host_jit_compiled_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return ZR_HOST_JIT_ARCH_X86_64;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return ZR_HOST_JIT_ARCH_AARCH64;
#else
    return ZR_HOST_JIT_ARCH_NONE;
#endif
}

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

TZrBool ZrCore_HostJit_SupportsOperation(
        TZrUInt32 operation,
        TZrUInt32 operationMask) {
    if (operation == 0u || (operation & (operation - 1u)) != 0u ||
        (operation & ~ZR_HOST_JIT_OPERATION_KNOWN_MASK) != 0u) {
        return ZR_FALSE;
    }
    return (TZrBool)((operationMask & operation) != 0u);
}

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

static void host_jit_lock(SZrHostJitCodeManager *manager) {
#if defined(_MSC_VER)
    while (_InterlockedExchange((volatile long *)&manager->lock, 1L) != 0L) {
    }
#else
    while (__sync_lock_test_and_set(&manager->lock, 1u) != 0u) {
    }
#endif
}

static void host_jit_unlock(SZrHostJitCodeManager *manager) {
#if defined(_MSC_VER)
    _InterlockedExchange((volatile long *)&manager->lock, 0L);
#else
    __sync_lock_release(&manager->lock);
#endif
}

static TZrBool host_jit_code_state_valid(TZrUInt32 state) {
    return (TZrBool)(state <= (TZrUInt32)ZR_HOST_JIT_CODE_RETIRED);
}

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
static TZrBool host_jit_manager_shape_valid(
        const SZrHostJitCodeManager *manager) {
    TZrUInt32 index;
    TZrUInt32 used = 0u;
    TZrUInt32 published = 0u;
    if (!host_jit_manager_shell_valid(manager)) {
        return ZR_FALSE;
    }
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

static TZrBool host_jit_manager_valid_locked(
        const SZrHostJitCodeManager *manager) {
    return host_jit_manager_shape_valid(manager);
}

static EZrHostJitStatus host_jit_require_manager(
        const SZrHostJitCodeManager *manager,
        SZrHostJitDiagnostic *diagnostic) {
    if (manager == ZR_NULL) {
        return host_jit_fail(ZR_HOST_JIT_STATUS_INVALID_ARGUMENT, diagnostic,
                             1u, 0u, 0u, 0u, 0u);
    }
    return ZR_HOST_JIT_STATUS_OK;
}

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

void ZrCore_HostJit_CodeManager_Deinit(SZrHostJitCodeManager *manager) {
    if (manager == ZR_NULL) {
        return;
    }
    host_jit_lock(manager);
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
