#ifndef ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_GRAPH_SHIM_H
#define ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_GRAPH_SHIM_H

#include "zr_vm_core/exec_ir_module_copy.h"
#include "zr_vm_core/exec_ir_owned_module.h"
TZrBool ZrTests_IntegerExpr_Candidate(const SZrExecIrModule *source,
        TZrExecIrFunctionId selectedId, const SZrExecIrFunction *replacement,
        SZrExecIrModule *destination, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrTests_IntegerExpr_Owner(const SZrExecIrModule *source,
        SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic);

/* Applied after the common allocator FI, to private Parser objects only.
 * Core's real definitions are separate objects with their original symbols. */
#ifndef ZR_TEST_INTEGER_EXPR_NO_REDIRECT
#define ZrCore_ExecIr_CloneModuleReplacingFunction ZrTests_IntegerExpr_Candidate
#define ZrCore_ExecIr_CloneOwnedModule ZrTests_IntegerExpr_Owner
#endif
#endif
