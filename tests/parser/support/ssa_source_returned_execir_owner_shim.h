#ifndef ZR_TEST_RETURNED_EXEC_SHIM_H
#define ZR_TEST_RETURNED_EXEC_SHIM_H
#include "ssa_source_returned_execir_fixture.h"
#include "exec_ir/source/exec_ir_source_finalized_module_internal.h"
#include "exec_ir/source/exec_ir_source_frame.h"
/* Identical typed inputs to the private helper. The observer never reconstructs
 * a runtime Function from its output slot or substitutes a producer graph. */
TZrBool ZrTests_ReturnedExec_Finalized(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrTests_ReturnedExec_AttachFrame(SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount, TZrUInt32 frameByteLimit,
        SZrExecIrDiagnostic *diagnostic);
#ifndef ZR_TEST_RETURNED_EXEC_NO_REDIRECT
#define ZrParser_ExecIr_AttachPrimitiveSourceFrame ZrTests_ReturnedExec_AttachFrame
#define ZrCore_ExecIr_ModuleAppendLayout ZrTests_ReturnedExec_AppendLayout
#define source_snapshot_finalized_primitive_module ZrTests_ReturnedExec_Finalized
#endif
#endif
