#include "zr_vm_parser/exec_ir_projections.h"

#include <stdlib.h>
#include <string.h>

void ZrParser_AotIrProjection_Free(SZrAotIrProjection *projection) {
    if (projection == ZR_NULL || projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG) return;
    free(projection->opcodes);
    free(projection->instructions);
    free(projection->operands);
    free(projection->results);
    free(projection->memoryTokens);
    free(projection->frameSlots);
    free(projection->valueSlots);
    free(projection->slotValues);
    free(projection->blocks);
    free(projection->predecessors);
    free(projection->successors);
    free(projection->phis);
    free(projection->phiIncomings);
    free(projection->phiCopySources);
    free(projection->phiCopyDestinations);
    free(projection->phiCopyEdges);
    free(projection->phiMoves);
    free(projection->sourceMaps);
    ZrCore_ExecIr_GcMapFree(&projection->gcMap);
    free(projection->gcRoots);
    free(projection->deoptStates);
    free(projection->deoptValues);
    free(projection->deoptAggregates);
    free(projection->deoptAggregateFields);
    ZrCore_ExecIr_StateMapFree(&projection->stateMap);
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
    if (function->stateMap != ZR_NULL) {
        EZrExecutionDiagnosticCode failure =
                ZrCore_ExecIr_StateMapStorageValid(function->stateMap)
                ? ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY
                : ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION;
        if (failure == ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION ||
            !ZrCore_ExecIr_StateMapClone(function->stateMap, &candidate.stateMap)) {
            if (diagnostic != ZR_NULL) {
                diagnostic->code = failure;
                diagnostic->functionToken = function->functionToken;
            }
            ZrParser_AotIrProjection_Free(&candidate);
            ZrParser_ExecBcProjection_Free(&prepared);
            return ZR_FALSE;
        }
    }
    ZrParser_AotIrProjection_Free(output);
    *output = candidate;
    ZrParser_ExecBcProjection_Free(&prepared);
    return ZR_TRUE;
}
