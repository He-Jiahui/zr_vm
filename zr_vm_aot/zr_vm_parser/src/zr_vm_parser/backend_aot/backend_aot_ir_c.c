#include "backend_aot_ir_adapter.h"

#include <string.h>

TZrBool backend_aot_ir_c_emit(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    return backend_aot_ir_c_emit_ex(module, options, ZR_FALSE,
                                    outFacts, diagnostic);
}

TZrBool backend_aot_ir_c_emit_ex(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        TZrBool requireArtifact,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    TZrBool boolValid;

    boolValid = (TZrBool)(requireArtifact == ZR_FALSE ||
                          requireArtifact == ZR_TRUE);
    if (!boolValid || options == ZR_NULL ||
        options->target != ZR_AOT_IR_EMITTER_C) {
        if (outFacts != ZR_NULL) {
            (void)memset(outFacts, 0, sizeof(*outFacts));
            outFacts->descriptorOnly = ZR_TRUE;
        }
        if (diagnostic != ZR_NULL) {
            (void)memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (!backend_aot_ir_adapter_emit_target(module, ZR_AOT_IR_EMITTER_C,
                                             options, outFacts, diagnostic)) {
        return ZR_FALSE;
    }
    if (requireArtifact) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_ARTIFACT_UNAVAILABLE;
            diagnostic->aotIrStatus = ZR_AOT_IR_UNSUPPORTED;
        }
        if (outFacts != ZR_NULL) {
            outFacts->artifactAvailable = ZR_FALSE;
            outFacts->descriptorOnly = ZR_TRUE;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
