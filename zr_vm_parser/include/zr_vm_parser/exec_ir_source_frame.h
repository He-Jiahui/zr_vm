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

/* Validate an already attached primitive INT64 frame against the existing
 * packed producer's regenerated result, without changing any input storage.
 * Function, context, explicit target rows and their reachable typed storage
 * must be initialized, readable, live, stable, serialized and independently
 * nonoverlapping. An optional diagnostic must be independent writable storage.
 * Address/shape checks cannot establish arbitrary-pointer readability or source
 * provenance. Context must have valid typed canonical storage with sorted,
 * nonzero node IDs; numeric IDs and hashes do not certify Binder provenance.
 * Both sealed and unsealed Functions are eligible for this const operation.
 * The body/value/target profile and whole-function SCALAR request are those of
 * Attach above, allowing an existing frame and contract.layoutHash. Target rows
 * provide geometry, not a target ABI certificate. Zero frameByteLimit is unbounded.
 *
 * Intended first-error order: NULL arguments; storage spans and capacities;
 * temporary count products; Core VERIFY_ALL; body profile; target rows;
 * sorted canonical IDs; value/type predicates; missing frame; request-values
 * allocation; existing LayoutPackedFrame; logical frame fields; frame hash;
 * contract hash; physical slot fields; actual-frame geometry. Validation may
 * allocate temporary verification/request/packed storage, all released before
 * return. It never allocates or publishes a persistent frame header or owner.
 * Compare logical content, excluding capacity equality, spare bytes and padding;
 * storage preflight still requires each live count to fit its readable capacity.
 * The existing packed hash domain is retained without adding row identity/ABI.
 *
 * Every wrapper refusal resets all nine diagnostic fields. NULL Function/context,
 * or nonzero layoutCount with NULL layouts, reports INVALID_ARGUMENT with token
 * and all payload zero. Other wrapper refusals use the actual functionToken:
 * invalid storage/sorted IDs use INVALID_RANGE; arithmetic uses CAPACITY_OVERFLOW;
 * body/row/value admission uses UNSUPPORTED; request allocation uses OUT_OF_MEMORY;
 * frame comparisons/geometry use LAYOUT_MISMATCH. Value admission additionally
 * reports actualVersion=value.id, instructionId=value.definition and sourceId
 * from a bounded defining instruction. Missing frame reports versions 1/0;
 * logical/slot comparisons report expected/actual versions, hash comparisons
 * expected/actual hashes. Other locations and payload fields remain zero.
 * Core verification and packed-layout failures preserve all nine lower fields.
 * Intended success clears all nine fields; NULL diagnostic is supported.
 * Every return preserves all input logical content, owners, pointers and capacities.
 *
 * Current RED staging: after the NULL guards, this entry returns false with
 * UNSUPPORTED/functionToken and all other diagnostic fields zero. It allocates
 * nothing and performs no structural validation yet.
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_ValidatePrimitiveSourceFrame(
        const SZrExecIrFunction *function, const SZrSemanticContext *context,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        TZrUInt32 frameByteLimit, SZrExecIrDiagnostic *diagnostic);

#endif
