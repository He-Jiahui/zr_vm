#include "zr_vm_parser/exec_ir_projections.h"

#include <stdlib.h>
#include <string.h>

void ZrParser_AotIrProjection_Free(SZrAotIrProjection *projection) {
    if (projection == ZR_NULL || projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG) return;
    free(projection->opcodes);
    free(projection->instructions);
    free(projection->operands);
    free(projection->results);
    free(projection->valueSlots);
    free(projection->blocks);
    free(projection->predecessors);
    free(projection->successors);
    free(projection->phiCopySources);
    free(projection->phiCopyDestinations);
    free(projection->phiCopyEdges);
    free(projection->sourceMaps);
    memset(projection, 0, sizeof(*projection));
}

TZrBool ZrParser_ExecIr_LowerAot(const SZrExecIrFunction *function,
                                 SZrAotIrProjection *output,
                                 SZrExecIrDiagnostic *diagnostic) {
    SZrExecBcProjection prepared;
    SZrAotIrProjection candidate;
    if (output == ZR_NULL) {
        if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    memset(&prepared, 0, sizeof(prepared));
    if (!ZrParser_ExecIr_BuildProjection(function, &prepared, diagnostic)) return ZR_FALSE;
    memset(&candidate, 0, sizeof(candidate));
    ZrParser_ExecIr_MoveProjectionToAot(&prepared, &candidate);
    candidate.signatureHash = function->signatureHash;
    candidate.functionToken = function->functionToken;
    candidate.contract = function->contract;
    /* This record is an AOTIR seam, not executable native code yet. */
    candidate.runnable = ZR_FALSE;
    candidate.ownershipTag = ZR_EXEC_IR_PROJECTION_TAG;
    ZrParser_AotIrProjection_Free(output);
    *output = candidate;
    ZrParser_ExecBcProjection_Free(&prepared);
    return ZR_TRUE;
}
