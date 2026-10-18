#ifndef ZR_VM_PARSER_EXEC_IR_PHI_IDENTITY_H
#define ZR_VM_PARSER_EXEC_IR_PHI_IDENTITY_H

#include "exec_ir/exec_ir_pass_internal.h"

typedef struct SZrExecIrPhiIdentityPlan SZrExecIrPhiIdentityPlan;
typedef TZrBool (*TZrExecIrLegacyPhiPredecessor)(
        SZrExecIrFunction *, SZrExecIrPassContext *, const SZrExecIrBlock *,
        TZrUInt32, TZrExecIrBlockId, TZrUInt32 *);

/* Prepare reads the original graph and proves every planned slot before any
 * store. FALSE is scratch OOM/overflow, not a direct SCCP cache rollback.
 * Budget exhaustion is TRUE with no plan. Apply allocates nothing and cannot
 * fail; the function must not be changed between Prepare and Apply. */
TZrBool ZrParser_ExecIr_PhiIdentityPrepare(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrExecIrLegacyPhiPredecessor legacy,
        SZrExecIrPhiIdentityPlan **plan, SZrExecIrDiagnostic *diagnostic);
void ZrParser_ExecIr_PhiIdentityApply(SZrExecIrPhiIdentityPlan *plan,
        TZrBool *changed);
void ZrParser_ExecIr_PhiIdentityDispose(SZrExecIrPhiIdentityPlan *plan);

#endif
