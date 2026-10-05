#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_FRAME_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_FRAME_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/semantic.h"

/* Attach primitive storage to a current, verified, independently owned graph.
 * The context is the same live canonical snapshot used by source compaction;
 * this call checks current storage consistency, not original source provenance.
 * Graph, context, target table and optional diagnostic storage must be valid,
 * live and independently nonoverlapping. Only canonical PRIMITIVE INT64 values
 * with flags 0, UNKNOWN ownership and NONNULL nullability are admitted here.
 * Existing frame, sealed graph, places, external values and rich metadata are
 * unsupported. Malformed Core input preserves its precise verifier diagnostic
 * before later admission checks; sealed valid input reports SEALED.
 * Explicit target rows require unique nonzero identity/type/hash, positive size
 * and power-of-two alignment. Row geometry supplies whole-function SCALAR
 * lifetimes [0,instructionCount), no reuse, no parameter prefix or return buffer.
 * Zero frameByteLimit adds no limit. Failure preserves the complete graph;
 * success publishes frame and contract.layoutHash together, preserving every
 * other field and the empty state-map identity header. Existing packed geometry
 * hashing is retained: row identity/fingerprint and ABI are not added to it.
 * AOT still requires the explicit target table and canonical callable context.
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_AttachPrimitiveSourceFrame(
        SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic);

#endif
