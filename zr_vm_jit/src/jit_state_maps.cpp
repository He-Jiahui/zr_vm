#include "zr_vm_jit/backend.h"

#include <cstring>

namespace {

void clear_diagnostic(SZrJitHostDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        std::memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_JIT_HOST_STATUS_OK;
        diagnostic->coreStatus = ZR_HOST_JIT_STATUS_OK;
    }
}

EZrJitHostStatus fail(SZrJitHostDiagnostic *diagnostic,
                      EZrJitHostStatus status,
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

bool has_flag(TZrUInt32 flags, TZrUInt32 flag) {
    return (flags & flag) != 0u;
}

EZrJitHostStatus validate_entry(TZrUInt32 flag,
                                TZrUInt32 count,
                                TZrUInt64 hash,
                                TZrUInt32 sourceIndex,
                                const SZrJitStateMapFacts *facts,
                                SZrJitHostDiagnostic *diagnostic) {
    if (has_flag(facts->registrationFlags, flag)) {
        if (count == 0u || hash == 0u) {
            return fail(diagnostic, ZR_JIT_HOST_STATUS_STATE_MAP_INVALID,
                        1u, count, 1u, hash, sourceIndex);
        }
    } else if (count != 0u || hash != 0u) {
        /* A hash/count without its registration bit is ambiguous and must not
         * be mistaken for a completed registration. */
        return fail(diagnostic, ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
                    flag, facts->registrationFlags, 0u, hash, sourceIndex);
    }
    return ZR_JIT_HOST_STATUS_OK;
}

}  // namespace

extern "C" EZrJitHostStatus ZrJit_Host_ValidateStateMaps(
        const SZrJitStateMapFacts *facts,
        SZrJitHostDiagnostic *diagnostic) {
    EZrJitHostStatus status;
    clear_diagnostic(diagnostic);
    if (facts == ZR_NULL) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_INVALID_ARGUMENT,
                    1u, 0u, 0u, 0u, 0u);
    }
    if (facts->schemaVersion != ZR_JIT_HOST_SCHEMA_VERSION) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_SCHEMA_MISMATCH,
                    ZR_JIT_HOST_SCHEMA_VERSION, facts->schemaVersion,
                    0u, 0u, 0u);
    }
    if ((facts->registrationFlags & ~ZR_JIT_HOST_MAP_KNOWN_MASK) != 0u) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_INVALID_FLAGS,
                    ZR_JIT_HOST_MAP_KNOWN_MASK, facts->registrationFlags,
                    0u, 0u, 0u);
    }
    if (facts->frameLayoutHash == 0u) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_STATE_MAP_INVALID,
                    1u, 0u, 1u, 0u, 0u);
    }
    if ((facts->registrationFlags & ZR_JIT_HOST_MAP_KNOWN_MASK) !=
        ZR_JIT_HOST_MAP_KNOWN_MASK) {
        return fail(diagnostic, ZR_JIT_HOST_STATUS_REGISTRATION_INCOMPLETE,
                    ZR_JIT_HOST_MAP_KNOWN_MASK, facts->registrationFlags,
                    0u, 0u, 0u);
    }

    status = validate_entry(ZR_JIT_HOST_MAP_ROOTS, facts->rootEntryCount,
                            facts->rootMapHash, 0u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    status = validate_entry(ZR_JIT_HOST_MAP_UNWIND, facts->unwindEntryCount,
                            facts->unwindMapHash, 1u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    status = validate_entry(ZR_JIT_HOST_MAP_DEBUG, facts->debugEntryCount,
                            facts->debugMapHash, 2u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    status = validate_entry(ZR_JIT_HOST_MAP_DEOPT, facts->deoptEntryCount,
                            facts->deoptMapHash, 3u, facts, diagnostic);
    if (status != ZR_JIT_HOST_STATUS_OK) return status;
    return ZR_JIT_HOST_STATUS_OK;
}
