#ifndef ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_PRODUCER_SHIM_H
#define ZR_TEST_SSA_SOURCE_INTEGER_EXPRESSION_FAULTS_PRODUCER_SHIM_H

#include <stdlib.h>
#include "tests/core/support/ssa_exec_ir_owned_module_allocator.h"
#include "exec_ir/source/exec_ir_source_integer_constants.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "exec_ir/source/exec_ir_source_finalized_module_internal.h"
#include "exec_ir/exec_ir_mutation_internal.h"

TZrBool ZrTests_IntegerExpr_Prepare(const struct SZrCompilerState *compiler,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrTests_IntegerExpr_Compact(const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context, const SZrExecIrFunction *input,
        SZrExecIrFunction *output, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrTests_IntegerExpr_AppendLayout(SZrExecIrModule *module,
        const SZrExecIrLayout *layouts, TZrSize count, SZrExecIrRange *range);
TZrBool ZrTests_IntegerExpr_Attach(SZrExecIrFunction *function,
        const SZrSemanticContext *context, const SZrExecIrLayout *layouts,
        TZrUInt32 layoutCount, TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrTests_IntegerExpr_Finalized(const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity,
        const SZrArray *compilerConstants, const SZrTypeValue *functionConstants,
        TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination,
        SZrExecIrDiagnostic *diagnostic);

/* Forced into the producer TU only. The real prepare/finalizer/compactor/frame
 * definitions are separate objects. Neither allocator nor observer substitutes
 * a status, diagnostic, allocation size, graph or output slot. */
#ifndef ZR_TEST_INTEGER_EXPR_NO_REDIRECT
#define malloc ZrTests_ExecOwner_Malloc
#define calloc ZrTests_ExecOwner_Calloc
#define realloc ZrTests_ExecOwner_Realloc
#define free ZrTests_ExecOwner_Free
#define source_integer_constants_prepare ZrTests_IntegerExpr_Prepare
#define ZrParser_ExecIr_EliminateDeadSourcePlaces ZrTests_IntegerExpr_Compact
#define ZrCore_ExecIr_ModuleAppendLayout ZrTests_IntegerExpr_AppendLayout
#define ZrParser_ExecIr_AttachPrimitiveSourceFrame ZrTests_IntegerExpr_Attach
#define source_snapshot_finalized_primitive_module ZrTests_IntegerExpr_Finalized
#endif
#endif
