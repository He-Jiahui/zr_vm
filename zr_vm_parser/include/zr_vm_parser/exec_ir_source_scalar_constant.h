#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_SCALAR_CONSTANT_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_SCALAR_CONSTANT_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/exec_ir.h"

struct SZrCompilerState;

/* Construct one independent Core scalar constant from the actual validated
 * literal SCRIPT compiler snapshot. This finite profile admits only one I64
 * constant at actual pool index zero, followed by the existing inert PLACE_BASE
 * and a RETURN of that constant, in one terminated entry CFG block. The complete
 * dead-address proof for PLACE_BASE belongs to EliminateDeadSourcePlaces.
 *
 * The caller supplies a live current CompilerState after assembly and function
 * finalization, with its same live State/Global/AST/current Function/SemIR and
 * canonical context, and independently readable typed allocations. Output and
 * optional diagnostic must be live writable storage, nonoverlapping with each
 * other and every input owner. The caller serializes mutation and lifecycle.
 * Geometry checks establish bounded storage spans, not arbitrary pointer
 * readability or historical allocation sizes. NULL diagnostic is permitted.
 *
 * Admission requires currentAst == scriptAst of SCRIPT kind, no current function
 * node, top-level function, submission context/entry or compiler error, a valid
 * current module key, and initialized/validated/active/terminated preSemanticIr.
 * Compiler, SemIR and semantic context must refer to the same State. The current
 * Function must own its storage, be versionless, and have no receiver, parameters,
 * variadic arguments, captures, children, imports, exports, module-entry effects
 * or test manifest. Relevant existing counts are checked, never cleared here.
 *
 * Finalized SourceCallableIdentity must match SemIR and the current canonical
 * FUNCTION node in schema/type/symbol, nonzero actual structural/signature hash,
 * empty valid parameterContracts, receiver NONE, effect NONE and explicit
 * noargs/I64 witnesses. Its canonical return must be PRIMITIVE INT64 and its
 * actual runtime callableReturnType nonnullable. Full module/symbol/signature
 * blob binding remains BindSourceModuleContract; this API fabricates no module.
 *
 * Checks proceed in this order: NULL arguments; state/context and typed-array
 * geometry; SCRIPT/snapshot eligibility; finalized callable identity; actual
 * literal shape/source-map/CFG/definition/operand bounds and hasPoolIndex;
 * compiler/runtime pool counts and actual index; canonical source node then
 * source/result/return type agreement and RETURN-result identity; typed pool
 * tags then signed I64 agreement. Typed arrays require their actual element
 * widths, length <= capacity, storage when capacity is nonzero, and checked
 * multiplication/address spans before lookup. Additional helper inputs require
 * the same checks. Prior validation flags do not authorize contradictory facts.
 * Both pools must contain exactly one row; no index is published because only
 * zero is admitted. Resolve node kind before reading its union, and require both
 * SZrTypeValue tags INT64 before reading either nativeInt64 union member.
 *
 * Every entry clears the entire supplied diagnostic. Every failure returns
 * false and preserves every output byte and every source owner byte. Success
 * publishes a completely zero-initialized SZrExecIrConstant with typeToken equal
 * to the actual CONSTANT typeId, flags zero and bits equal to the compiler
 * nativeInt64 converted to TZrUInt64; the diagnostic remains entirely zero.
 * The unsigned conversion preserves the I64 bit pattern. Source spelling and
 * fixture expected returns are not inputs. No ownership transfers, cached data
 * or retained borrows result. The independent output survives source cleanup.
 *
 * No allocation/deallocation, interning, VM call, compilation, module append,
 * compaction, hash generation, execution or callback occurs. This API does not
 * mutate validation flags and does not serialize arbitrary constant pools.
 *
 * Diagnostics use existing codes. functionToken/blockId/instructionId stay zero;
 * sourceId is the actual safely identified offending SemIR instruction ID or
 * zero before identification. Unrelated payload fields remain zero. Numeric
 * expected/actual facts use expectedHash/actualHash; schema facts use only
 * expectedVersion/actualVersion. Refusal precedence follows the order above:
 * - NULL compiler/output: INVALID_ARGUMENT, all payload zero.
 * - Bad array flag/width/length/capacity: INVALID_RANGE, required flag/width/
 *   capacity versus supplied flag/width/length. Span overflow: CAPACITY_OVERFLOW,
 *   maximum admitted count versus actual count.
 * - Missing certification/SCRIPT/ownership/arity/receiver/effect eligibility:
 *   UNSUPPORTED, 1 versus 0, or 0 versus the isolated nonzero count/flags.
 * - State disagreement: SIGNATURE_MISMATCH, 1 versus 0, sourceId zero.
 * - Identity schema: VERSION_MISMATCH, required versus supplied schema.
 * - Identity type/symbol/noargs/I64 witness: SIGNATURE_MISMATCH, 1 versus 0;
 *   canonical hash mismatch instead reports callable hash versus source hash.
 * - Missing/extra scalar opcode/return shape: UNSUPPORTED, 1 versus 0.
 * - Missing hasPoolIndex: INVALID_VALUE, 1 versus 0 at the CONSTANT.
 * - Pool counts: INVALID_RANGE, compiler versus runtime length if unequal,
 *   otherwise 1 versus compiler length. Out-of-range index: INVALID_RANGE,
 *   pool length versus index at the CONSTANT, before any pool dereference.
 * - Missing canonical source node: INVALID_VALUE, 1 versus unresolved TypeId.
 *   Resolved nonprimitive/non-I64: UNSUPPORTED, 1 versus 0 at the CONSTANT.
 * - Source/result/return type disagreement: INVALID_VALUE, canonical callable
 *   return TypeId versus mismatching TypeId at the offending instruction.
 * - RETURN operand disagreement: INVALID_VALUE, CONSTANT resultValueId versus
 *   bounded RETURN operand at the RETURN.
 * - Non-I64 typed pool: UNSUPPORTED, INT64 tag versus actual tag at CONSTANT.
 * - Pool I64 disagreement: INVALID_VALUE, compiler versus runtime UInt64 bits
 *   at the CONSTANT. This nonallocating API has no OUT_OF_MEMORY result.
 *
 * Finite RED staging: the current linked implementation is a safe refusal stub.
 * It clears the whole optional diagnostic, refuses NULL compiler/output with
 * INVALID_ARGUMENT and all other calls with UNSUPPORTED, with zero payloads,
 * without reading input owners or writing output. Extraction is not yet enabled.
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_MakeSourceScalarConstant(
        const struct SZrCompilerState *compiler,
        SZrExecIrConstant *output, SZrExecIrDiagnostic *diagnostic);

#endif
