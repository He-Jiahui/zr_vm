#include "zr_vm_core/exec_ir_module_copy.h"

#include <string.h>

static void module_copy_diagnostic(SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code, TZrMetadataToken token) {
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = token;
}

static TZrBool module_copy_fail32(SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code, TZrMetadataToken token,
        TZrUInt32 expected, TZrUInt32 actual) {
    module_copy_diagnostic(diagnostic, code, token);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedVersion = expected;
        diagnostic->actualVersion = actual;
    }
    return ZR_FALSE;
}

static TZrBool module_copy_fail64(SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code, TZrMetadataToken token,
        TZrUInt64 expected, TZrUInt64 actual) {
    module_copy_diagnostic(diagnostic, code, token);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return ZR_FALSE;
}

static TZrBool module_copy_identity(const SZrExecIrFunction *selected,
        TZrExecIrFunctionId selectedId, const SZrExecIrFunction *replacement,
        SZrExecIrDiagnostic *diagnostic) {
    if (selected->id != selectedId)
        return module_copy_fail32(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                selected->functionToken, selectedId, selected->id);
    if (replacement->id != selectedId)
        return module_copy_fail32(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
                selected->functionToken, selectedId, replacement->id);
#define MATCH32(field, code) \
    do { if (selected->field != replacement->field) \
        return module_copy_fail32(diagnostic, code, selected->functionToken, \
                selected->field, replacement->field); } while (0)
#define MATCH64(field, code) \
    do { if (selected->field != replacement->field) \
        return module_copy_fail64(diagnostic, code, selected->functionToken, \
                selected->field, replacement->field); } while (0)
    MATCH32(functionToken, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    MATCH64(signatureHash, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    MATCH32(contract.schemaVersion, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    MATCH32(contract.abiVersion, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    MATCH32(contract.logicalVersion, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    MATCH32(contract.reserved0, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    MATCH32(contract.reserved1, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    MATCH64(contract.generation, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION);
    MATCH32(contract.targetToken, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH);
    MATCH64(contract.signatureHash, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    MATCH64(contract.moduleHash, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH);
    MATCH32(contract.requiredCapabilities, ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH);
    MATCH32(contract.declaredEffects, ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH);
#undef MATCH32
#undef MATCH64
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_CloneModuleReplacingFunction(
        const SZrExecIrModule *source, TZrExecIrFunctionId selectedId,
        const SZrExecIrFunction *replacement, SZrExecIrModule *destination,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrModule candidate;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (source == ZR_NULL || replacement == ZR_NULL || destination == ZR_NULL ||
        source == destination) {
        return module_copy_fail32(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                0u, 0u, 0u);
    }
    if (selectedId == 0u || selectedId > source->functionCount) {
        return module_copy_fail32(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                0u, source->functionCount, selectedId);
    }
    ZrCore_ExecIr_ModuleInit(&candidate);
    if (!ZrCore_ExecIr_CloneModule(source, &candidate, diagnostic)) goto failure;
    if (!module_copy_identity(&candidate.functions[selectedId - 1u], selectedId,
            replacement, diagnostic)) goto failure;
    if (!ZrCore_ExecIr_CloneFunction(replacement,
            &candidate.functions[selectedId - 1u], diagnostic)) goto failure;
    /* All reads of a replacement borrowed from the old destination are done. */
    ZrCore_ExecIr_FreeModule(destination);
    *destination = candidate;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    return ZR_TRUE;
failure:
    /* Free does not alter the diagnostic from either lower clone or identity. */
    ZrCore_ExecIr_FreeModule(&candidate);
    return ZR_FALSE;
}
