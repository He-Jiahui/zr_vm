#ifndef ZR_EXEC_IR_CALL_TARGET_H
#define ZR_EXEC_IR_CALL_TARGET_H

#include "zr_vm_parser/exec_ir_interprocedural.h"

/* Private scalar graph consumer.  The caller owns instruction/row identity;
 * this helper never invokes a provider or changes ExecIR/graph storage. */
TZrBool ZrParser_ExecIr_BuildCallEdge(
    const SZrExecIrModule *module, const SZrExecIrFunction *caller,
    TZrExecIrInstructionId instructionId,
    const SZrExecIrFunctionSummary *summaries, TZrUInt32 summaryCount,
    SZrExecIrCallEdge *edge);

typedef struct SZrExecIrTypedCallMismatch {
    EZrExecutionDiagnosticCode code;
    TZrExecIrFunctionId callerId;
    TZrExecIrInstructionId instructionId;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrExecIrTypedCallMismatch;

TZrBool ZrParser_ExecIr_TypedCallEdgeMatches(
    const SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
    const SZrExecIrCallEdge *edge, SZrExecIrTypedCallMismatch *mismatch);

/* Called after every present edge's caller, instruction and opcode bounds
 * have been checked.  Only typed CALL/INVOKE sites require correspondence. */
TZrBool ZrParser_ExecIr_TypedCallSitesMatch(
    const SZrExecIrModule *module, const SZrExecIrCallGraph *graph,
    SZrExecIrTypedCallMismatch *mismatch);

#endif
