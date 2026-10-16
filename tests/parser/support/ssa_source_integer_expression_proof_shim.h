#ifndef ZR_TEST_SOURCE_INTEGER_PROOF_SHIM_H
#define ZR_TEST_SOURCE_INTEGER_PROOF_SHIM_H
#include <stdlib.h>
#include "tests/core/support/ssa_exec_ir_owned_module_allocator.h"
#include "exec_ir/source/exec_ir_source_finalized_module_internal.h"
#include "exec_ir/source/exec_ir_source_integer_constants.h"
/* Forced into the real retention producer only. No producer result is replaced. */
TZrBool ZrTests_IntegerProof_Prepare(const struct SZrCompilerState *compiler,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrTests_IntegerProof_Finalized(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *identity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic);
#ifndef ZR_TEST_INTEGER_PROOF_NO_REDIRECT
#define malloc ZrTests_ExecOwner_Malloc
#define calloc ZrTests_ExecOwner_Calloc
#define realloc ZrTests_ExecOwner_Realloc
#define free ZrTests_ExecOwner_Free
#define source_integer_constants_prepare ZrTests_IntegerProof_Prepare
#define source_snapshot_finalized_primitive_module ZrTests_IntegerProof_Finalized
#endif
#endif
