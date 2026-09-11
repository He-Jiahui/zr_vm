#include "zr_vm_core/execution_contract.h"

#include <string.h>

static void zr_execution_contract_clear_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static EZrExecutionContractStatus zr_execution_contract_fail(
        EZrExecutionContractStatus status,
        EZrExecutionDiagnosticCode code,
        const SZrExecutionContract *expected,
        const SZrExecutionContract *actual,
        TZrUInt32 expectedVersion,
        TZrUInt32 actualVersion,
        TZrUInt64 expectedHash,
        TZrUInt64 actualHash,
        SZrExecIrDiagnostic *diagnostic) {
    (void)actual;
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->functionToken = expected != ZR_NULL ? expected->targetToken : 0u;
        diagnostic->expectedVersion = expectedVersion;
        diagnostic->actualVersion = actualVersion;
        diagnostic->expectedHash = expectedHash;
        diagnostic->actualHash = actualHash;
    }
    return status;
}

EZrExecutionContractStatus ZrCore_ExecutionContract_Check(
        const SZrExecutionContract *expected,
        const SZrExecutionContract *actual,
        SZrExecIrDiagnostic *diagnostic) {
    zr_execution_contract_clear_diagnostic(diagnostic);
    if (expected == ZR_NULL || actual == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_EXECUTION_CONTRACT_INVALID_ARGUMENT;
    }
    if (expected->schemaVersion != actual->schemaVersion ||
        expected->abiVersion != actual->abiVersion ||
        expected->logicalVersion != actual->logicalVersion) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
                ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
                expected,
                actual,
                expected->schemaVersion,
                actual->schemaVersion,
                expected->abiVersion,
                actual->abiVersion,
                diagnostic);
    }
    if (expected->targetToken != actual->targetToken) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_TARGET_MISMATCH,
                ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
                expected,
                actual,
                expected->targetToken,
                actual->targetToken,
                0u,
                0u,
                diagnostic);
    }
    if (expected->generation != actual->generation) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_STALE_GENERATION,
                ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
                expected,
                actual,
                0u,
                0u,
                expected->generation,
                actual->generation,
                diagnostic);
    }
    if (expected->signatureHash != actual->signatureHash) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_SIGNATURE_MISMATCH,
                ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                expected,
                actual,
                0u,
                0u,
                expected->signatureHash,
                actual->signatureHash,
                diagnostic);
    }
    if (expected->layoutHash != actual->layoutHash) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_LAYOUT_MISMATCH,
                ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH,
                expected,
                actual,
                0u,
                0u,
                expected->layoutHash,
                actual->layoutHash,
                diagnostic);
    }
    if (expected->moduleHash != actual->moduleHash) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_MODULE_MISMATCH,
                ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
                expected,
                actual,
                0u,
                0u,
                expected->moduleHash,
                actual->moduleHash,
                diagnostic);
    }
    if ((expected->requiredCapabilities & ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u ||
        (actual->requiredCapabilities & ~ZR_EXECUTION_CAPABILITY_KNOWN_MASK) != 0u ||
        expected->requiredCapabilities != actual->requiredCapabilities) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_CAPABILITY_MISMATCH,
                ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
                expected,
                actual,
                expected->requiredCapabilities,
                actual->requiredCapabilities,
                0u,
                0u,
                diagnostic);
    }
    if ((expected->declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u ||
        (actual->declaredEffects & ~ZR_EXECUTION_EFFECT_KNOWN_MASK) != 0u ||
        expected->declaredEffects != actual->declaredEffects) {
        return zr_execution_contract_fail(
                ZR_EXECUTION_CONTRACT_EFFECT_MISMATCH,
                ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
                expected,
                actual,
                expected->declaredEffects,
                actual->declaredEffects,
                0u,
                0u,
                diagnostic);
    }
    return ZR_EXECUTION_CONTRACT_OK;
}

const TZrChar *ZrCore_ExecutionContract_StatusName(EZrExecutionContractStatus status) {
    switch (status) {
        case ZR_EXECUTION_CONTRACT_OK:
            return "OK";
        case ZR_EXECUTION_CONTRACT_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED:
            return "RECOMPILE_REQUIRED";
        case ZR_EXECUTION_CONTRACT_TARGET_MISMATCH:
            return "TARGET_MISMATCH";
        case ZR_EXECUTION_CONTRACT_SIGNATURE_MISMATCH:
            return "SIGNATURE_MISMATCH";
        case ZR_EXECUTION_CONTRACT_LAYOUT_MISMATCH:
            return "LAYOUT_MISMATCH";
        case ZR_EXECUTION_CONTRACT_MODULE_MISMATCH:
            return "MODULE_MISMATCH";
        case ZR_EXECUTION_CONTRACT_CAPABILITY_MISMATCH:
            return "CAPABILITY_MISMATCH";
        case ZR_EXECUTION_CONTRACT_EFFECT_MISMATCH:
            return "EFFECT_MISMATCH";
        case ZR_EXECUTION_CONTRACT_STALE_GENERATION:
            return "STALE_GENERATION";
        case ZR_EXECUTION_CONTRACT_UNSUPPORTED:
        case ZR_EXECUTION_CONTRACT_STATUS_COUNT:
        default:
            return "UNSUPPORTED";
    }
}
