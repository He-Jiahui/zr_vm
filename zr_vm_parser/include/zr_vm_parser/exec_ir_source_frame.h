#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_FRAME_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_FRAME_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/semantic.h"

/* Attach primitive storage to a current, verified, independently owned graph.
 * The context is the same live canonical snapshot used by source compaction,
 * with sorted node IDs and actual readable node allocations. Context and rows
 * must remain unchanged during this call. Shape/address checks cannot establish
 * readability or snapshot provenance. This checks current storage consistency.
 * Graph, context, target table and optional diagnostic storage must be valid,
 * live and independently nonoverlapping. Only canonical PRIMITIVE INT64 values
 * with flags 0, UNKNOWN ownership, NONNULL nullability and an explicit valid
 * nonzero ordinary definition instruction are admitted here.
 * This first slice admits only CONSTANT/NOP/RETURN instructions. Existing frame,
 * places, external values and rich metadata are unsupported. After storage
 * preflight, logical Core verification failures preserve their precise diagnostic
 * before later admission checks; malformed storage is refused during preflight;
 * sealed valid input reports SEALED.
 * Explicit target rows supply target geometry without host defaults or a target
 * ABI proof. Rows require unique nonzero identity/type/hash, positive size
 * and power-of-two alignment. Row geometry supplies whole-function SCALAR
 * lifetimes [0,instructionCount), no reuse, no parameter prefix or return buffer.
 * Zero frameByteLimit adds no limit. Under the independent storage preconditions,
 * failure preserves the complete graph;
 * success transfers a new owned header and slots into frameLayout and publishes
 * contract.layoutHash, preserving every other field and the empty state-map
 * identity header. Existing packed geometry
 * hashing is retained: row identity/fingerprint and ABI are not added to it.
 * AOT still requires the explicit target table and canonical callable context.
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_AttachPrimitiveSourceFrame(
        SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic);

#endif
