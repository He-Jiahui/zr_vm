#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_FINALIZED_MODULE_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_FINALIZED_MODULE_INTERNAL_H

#include <stdint.h>
#include "exec_ir_source_callable_query_internal.h"
#include "zr_vm_core/exec_ir_owned_module.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"

/* Private finite Source snapshot seam. Inputs and every traversed typed allocation
 * are initialized, aligned, genuinely readable, live, stable and serialized.
 * Numeric span checks do not establish arbitrary-pointer readability. Canonical
 * IDs/hashes and declared fixture fields establish consistency, not genuine
 * Source history or durable registry ownership. The caller supplies the same live
 * successful scalar bridge, Build/Binder, compactor and frame producer witnesses.
 * Compiler and Function constant pools remain separate live borrowed inputs;
 * functionConstantCount has the actual Function ABI's UInt32 width.
 *
 * Destination is an exclusive live writable slot containing NULL or a genuine
 * unique generic owner. Slot and optional diagnostic are disjoint from each other,
 * all input storage and the old owner, including untraversed reachable storage.
 * Diagnostic is independent writable storage; NULL diagnostic is supported.
 * Inputs may borrow a distinct old owner, which stays live until successful last
 * publication. Overlapping input owners and same selected/finalized Function
 * admission remain subject to the exact finite v3 contract, not alias repair.
 *
 * Success deep-copies whole module storage with the finalized selected
 * Function, validates current witnesses and snapshots last. Stored numeric IDs
 * do not extend registry lifetime or authorize execution/default compilation.
 * Failure preserves inputs and destination; all nine diagnostic fields are reset
 * before reporting the exact wrapper or preserved lower refusal.
 *
 * Required NULL arguments report INVALID_ARGUMENT/token0. Other refusals follow
 * the ordered finite v3 contract. NULL functionConstants is a pool-shape check,
 * not a top-level argument guard. No wrapper allocation or input mutation occurs;
 * real lower verification/frame/copy/snapshot operations own their temporaries.
 */
TZrBool source_snapshot_finalized_primitive_module(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic);

/* Complete diagnostic reset shared by the private orchestration/validation TUs. */
TZrBool source_finalized_module_fail(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code,
                                     TZrMetadataToken token);

enum {
    SOURCE_FINALIZED_INPUT_SPANS = 94,
    SOURCE_FINALIZED_ALL_SPANS = 127,
    SOURCE_FINALIZED_ROLES = 3,
    SOURCE_FINALIZED_NO_ROLE = 3
};
typedef struct SSourceFinalizedSpan {
    uintptr_t start, end;
} SSourceFinalizedSpan;
/* Per-call borrowed witnesses; none survives the last snapshot operation. */
typedef struct SSourceFinalizedModuleWitness {
    const SZrSemanticIrFunction *semantic;
    const SZrSemanticContext *context;
    const SZrAstNode *script;
    const SZrFunctionSourceCallableIdentity *identity;
    const SZrArray *compilerConstants;
    const SZrTypeValue *functionConstants;
    TZrUInt32 functionConstantCount;
    const SZrExecIrModule *original;
    const SZrExecIrFunction *finalized, *selected;
    SZrExecIrOwnedModule **destination;
    SZrExecIrDiagnostic *diagnostic;
    TZrMetadataToken token;
    SSourceCallableFacts facts;
    SSourceFinalizedSpan spans[SOURCE_FINALIZED_ALL_SPANS];
    size_t spanCount, spanLimit;
    TZrUInt32 instructionIds[SOURCE_FINALIZED_ROLES], sourceIds[SOURCE_FINALIZED_ROLES];
    TZrBool mapped;
} SSourceFinalizedModuleWitness;
TZrBool source_finalized_module_error(SSourceFinalizedModuleWitness *w, EZrExecutionDiagnosticCode code,
                                      TZrUInt32 expectedVersion, TZrUInt32 actualVersion, TZrUInt64 expectedHash,
                                      TZrUInt64 actualHash, unsigned role);
TZrBool source_finalized_module_preflight(SSourceFinalizedModuleWitness *w);
TZrBool source_finalized_module_admit_source(SSourceFinalizedModuleWitness *w);
TZrBool source_finalized_module_mapping(SSourceFinalizedModuleWitness *w);
TZrBool source_finalized_module_relation(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *function);
TZrBool source_finalized_module_candidate_storage(SSourceFinalizedModuleWitness *w, const SZrExecIrModule *module);
TZrBool source_finalized_module_metadata(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *function);

#endif
