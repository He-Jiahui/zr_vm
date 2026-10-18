#ifndef ZR_VM_PARSER_EXEC_IR_PASS_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_PASS_INTERNAL_H

#include "zr_vm_parser/exec_ir_pass_manager.h"

/*
 * These helpers are private to the scalar pass implementation.  Keeping the
 * public header about contracts rather than algorithms lets later passes use
 * the manager without coupling to SCCP's representation.
 */
TZrBool ZrParser_ExecIr_PassConsumeBudget(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        TZrUInt64 work);
void ZrParser_ExecIr_PassDiagnostic(
        SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code,
        const SZrExecIrFunction *function, TZrExecIrBlockId blockId,
        TZrExecIrInstructionId instructionId, TZrUInt32 expected,
        TZrUInt32 actual);
TZrBool ZrParser_ExecIr_PassRangeValid(SZrExecIrRange range, TZrUInt32 count);

TZrBool ZrParser_ExecIr_ComputeSccp(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        TZrBool rewrite, TZrBool *changed, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_RunSccpPass(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        TZrBool *changed, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_RunDcePass(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        TZrBool *changed, SZrExecIrDiagnostic *diagnostic);

/* The owner is an independently cloned, verified candidate.  A FALSE result
 * discards that entire candidate; these helpers do not roll back its pool.
 * Function membership is checked and context's constant view is refreshed. */
TZrBool ZrParser_ExecIr_RunModulePassPipeline(
        SZrExecIrModule *owner, SZrExecIrFunction *function,
        const SZrExecIrPassInfo *passes, TZrUInt32 passCount,
        SZrExecIrPassContext *context, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_RunModuleSccpPass(
        SZrExecIrModule *owner, SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrBool *changed,
        SZrExecIrDiagnostic *diagnostic);

/* Only canonical INT64 or BOOL (bits 0 or 1) tuples are materialized.
 * instructionId is one based; outIndex is a zero based constant pool index.
 * On TRUE, outReady==FALSE means budget exhaustion without an append.  When
 * ready, the caller must perform its non-failing rewrite immediately: pool
 * search and the reuse/append plus rewrite cost have already been charged. */
TZrBool ZrParser_ExecIr_MaterializeScalarConstant(
        SZrExecIrModule *owner, SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrExecIrInstructionId instructionId,
        TZrExecIrTypeToken typeToken, TZrUInt64 bits,
        TZrUInt32 *outIndex, TZrBool *outReady,
        SZrExecIrDiagnostic *diagnostic);

#endif
