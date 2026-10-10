#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_FINALIZED_MODULE_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_FINALIZED_MODULE_INTERNAL_H

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
 * Intended success deep-copies whole module storage with the finalized selected
 * Function, validates current witnesses and snapshots last. Stored numeric IDs
 * do not extend registry lifetime or authorize execution/default compilation.
 * Failure preserves inputs and destination; all nine diagnostic fields are reset
 * before reporting the exact wrapper or preserved lower refusal.
 *
 * Current RED staging: required NULL arguments report INVALID_ARGUMENT/token0;
 * other calls report UNSUPPORTED with only a safely bounded original selected
 * token, otherwise token0. No allocation/free, nested traversal, query, Verify,
 * clone, lifecycle or publication occurs. NULL functionConstants is not a
 * top-level guard; its count/storage admission belongs to later GREEN.
 */
TZrBool source_snapshot_finalized_primitive_module(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context, const SZrAstNode *script,
        const SZrFunctionSourceCallableIdentity *sourceIdentity, const SZrArray *compilerConstants,
        const SZrTypeValue *functionConstants, TZrUInt32 functionConstantCount, const SZrExecIrModule *original,
        const SZrExecIrFunction *finalized, SZrExecIrOwnedModule **destination, SZrExecIrDiagnostic *diagnostic);

/* Complete diagnostic reset shared by the private orchestration/validation TUs. */
TZrBool source_finalized_module_fail(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code,
                                     TZrMetadataToken token);

/* RED token observation only: table arithmetic is not source admission. */
TZrMetadataToken source_finalized_module_read_selected_token(const SZrExecIrModule *original);

#endif
