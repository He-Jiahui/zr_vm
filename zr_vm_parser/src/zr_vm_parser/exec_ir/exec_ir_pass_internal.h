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

#endif
