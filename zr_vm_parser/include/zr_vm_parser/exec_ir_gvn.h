#ifndef ZR_VM_PARSER_EXEC_IR_GVN_H
#define ZR_VM_PARSER_EXEC_IR_GVN_H

#include "zr_vm_parser/exec_ir_ranges.h"
#include "zr_vm_parser/exec_ir_pass_manager.h"

/* Runs a conservative, same-block value-numbering pass.  The pass rewrites a
 * duplicate pure definition as COPY of its dominating definition, preserving
 * the result value and all SSA identities. `facts` is optional: pure CSE has
 * no reason to reject a caller merely because no range facts were computed. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_RunGvnCse(
        SZrExecIrFunction *function,
        const SZrExecIrAnalysisFacts *facts,
        SZrExecIrRemarkSink *remarks,
        SZrExecIrDiagnostic *diagnostic);

/* A caller may use this proof before removing a bounds-check instruction. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_CanElideBoundsCheck(
        const SZrExecIrRangeFact *index,
        const SZrExecIrRangeFact *length,
        SZrExecIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_EXEC_IR_GVN_H */
