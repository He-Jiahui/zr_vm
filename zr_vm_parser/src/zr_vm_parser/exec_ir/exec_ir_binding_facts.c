#include "zr_vm_parser/exec_ir_binding_facts.h"

#include <stdint.h>
#include <string.h>

/* FNV-1a is used here only as a deterministic identity for in-memory facts;
 * it is not a cryptographic integrity check.  Hash fields explicitly rather
 * than hashing struct bytes so padding and host ABI differences cannot leak
 * into a binding identity. */
#define ZR_EXEC_IR_BINDING_FACTS_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_EXEC_IR_BINDING_FACTS_HASH_PRIME UINT64_C(1099511628211)

static void hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 byte;
    if (hash == ZR_NULL) return;
    for (byte = 0u; byte < 4u; ++byte) {
        *hash ^= (TZrUInt64)((value >> (byte * 8u)) & 0xffu);
        *hash *= ZR_EXEC_IR_BINDING_FACTS_HASH_PRIME;
    }
}

static void hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    hash_u32(hash, (TZrUInt32)value);
    hash_u32(hash, (TZrUInt32)(value >> 32u));
}

static void hash_contract(TZrUInt64 *hash, const SZrCallBindingContract *contract) {
    if (hash == ZR_NULL || contract == ZR_NULL) return;
    hash_u32(hash, contract->bindingKind);
    hash_u32(hash, contract->targetMetadataToken);
    hash_u32(hash, contract->signatureToken);
    hash_u32(hash, contract->ownerTypeToken);
    hash_u64(hash, contract->signatureHash);
    hash_u64(hash, contract->moduleSignatureHash);
    hash_u32(hash, contract->layoutVersion);
    hash_u32(hash, contract->dispatchSlot);
    hash_u64(hash, contract->layoutHash);
    hash_u32(hash, contract->operation);
    hash_u32(hash, contract->reserved0);
    hash_u64(hash, contract->reserved1);
}

static void hash_location(TZrUInt64 *hash, const SZrCallBindingLocation *location) {
    if (hash == ZR_NULL || location == ZR_NULL) return;
    hash_u32(hash, location->kind);
    hash_u32(hash, location->targetIndex);
    hash_u32(hash, location->ownerDepth);
    hash_u32(hash, location->flags);
}

static void hash_segment(TZrUInt64 *hash, const SZrExecIrBindingSegment *segment) {
    if (hash == ZR_NULL || segment == ZR_NULL) return;
    hash_u32(hash, segment->index);
    hash_u32(hash, (TZrUInt32)segment->kind);
    hash_u32(hash, segment->flags);
    hash_u32(hash, segment->instructionId);
    hash_u32(hash, segment->receiverInstructionId);
    hash_u32(hash, segment->writebackInstructionId);
    hash_u32(hash, segment->receiverValueId);
    hash_u32(hash, segment->resultValueId);
    hash_u32(hash, segment->memberToken);
    hash_u32(hash, segment->receiverTypeToken);
    hash_u32(hash, segment->resultTypeToken);
    hash_u32(hash, segment->memberId);
    hash_u32(hash, segment->layoutId);
    hash_u32(hash, segment->layoutVersion);
    hash_u64(hash, segment->layoutHash);
    hash_u32(hash, segment->operation);
    hash_u32(hash, segment->bindingRow);
    hash_u32(hash, segment->sourceId);
}

static void hash_row(TZrUInt64 *hash, const SZrExecIrBindingRow *row) {
    if (hash == ZR_NULL || row == ZR_NULL) return;
    hash_u32(hash, row->rowIndex);
    hash_u32(hash, row->instructionId);
    hash_u32(hash, row->segmentIndex);
    hash_contract(hash, &row->contract);
    hash_location(hash, &row->location);
    hash_u32(hash, row->sourceId);
}

void ZrParser_ExecIr_BindingFacts_Init(SZrExecIrBindingFacts *facts) {
    if (facts == ZR_NULL) return;
    memset(facts, 0, sizeof(*facts));
    facts->schemaVersion = ZR_EXEC_IR_BINDING_FACTS_SCHEMA_VERSION;
    facts->finalSegmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
}

TZrUInt64 ZrParser_ExecIr_BindingFacts_Hash(const SZrExecIrBindingFacts *facts) {
    TZrUInt64 hash = ZR_EXEC_IR_BINDING_FACTS_HASH_OFFSET;
    TZrUInt32 index;
    if (facts == ZR_NULL) return 0u;
    hash_u32(&hash, facts->schemaVersion);
    hash_u32(&hash, facts->flags);
    hash_u32(&hash, facts->functionToken);
    hash_u64(&hash, facts->signatureHash);
    hash_u64(&hash, facts->moduleHash);
    hash_u64(&hash, facts->generation);
    hash_u32(&hash, facts->segmentCount);
    hash_u32(&hash, facts->rowCount);
    hash_u32(&hash, facts->finalSegmentIndex);
    for (index = 0u; index < facts->segmentCount; ++index) {
        hash_segment(&hash, facts->segments != ZR_NULL ? &facts->segments[index] : ZR_NULL);
    }
    for (index = 0u; index < facts->rowCount; ++index) {
        hash_row(&hash, facts->rows != ZR_NULL ? &facts->rows[index] : ZR_NULL);
    }
    return hash;
}

static void diagnostic_init(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
}

static TZrUInt32 diagnostic_block_for_instruction(const SZrExecIrFunction *function,
                                                  TZrExecIrInstructionId instructionId) {
    TZrUInt32 index;
    if (function == ZR_NULL || function->blocks == ZR_NULL || instructionId == 0u) return 0u;
    for (index = 0u; index < function->blockCount; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 start = block->instructionRange.start;
        TZrUInt32 count = block->instructionRange.count;
        if (count == 0u) continue;
        if ((instructionId >= start && instructionId - start < count) ||
            (start == 0u && instructionId <= count)) {
            return block->id;
        }
    }
    return 0u;
}

static const SZrExecIrBindingSegment *diagnostic_segment_for_facts(
        const SZrExecIrBindingFacts *facts) {
    TZrUInt32 index;
    const SZrExecIrBindingSegment *first = ZR_NULL;
    if (facts == ZR_NULL || facts->segments == ZR_NULL) return ZR_NULL;
    for (index = 0u; index < facts->segmentCount; ++index) {
        const SZrExecIrBindingSegment *segment = &facts->segments[index];
        if (first == ZR_NULL) first = segment;
        if (segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL ||
            (segment->flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL) != 0u) {
            return segment;
        }
    }
    return first;
}

static void diagnostic_set(SZrExecIrDiagnostic *diagnostic,
                           EZrExecutionDiagnosticCode code,
                           const SZrExecIrBindingFacts *facts,
                           const SZrExecIrFunction *function,
                           const SZrExecIrBindingSegment *segment,
                           const SZrExecIrBindingRow *row,
                           TZrUInt64 expected,
                           TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) return;
    diagnostic_init(diagnostic);
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL && function->functionToken != 0u
            ? function->functionToken
            : (facts != ZR_NULL ? facts->functionToken : 0u);
    diagnostic->instructionId = segment != ZR_NULL && segment->instructionId != 0u
            ? segment->instructionId
            : (row != ZR_NULL ? row->instructionId : 0u);
    diagnostic->blockId = diagnostic_block_for_instruction(function,
                                                            diagnostic->instructionId);
    diagnostic->sourceId = segment != ZR_NULL && segment->sourceId != 0u
            ? segment->sourceId
            : (row != ZR_NULL ? row->sourceId : 0u);
    diagnostic->expectedVersion = (TZrUInt32)expected;
    diagnostic->actualVersion = (TZrUInt32)actual;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
}

static EZrExecutionDiagnosticCode diagnostic_code_for_status(
        EZrExecIrBindingFactsStatus status) {
    switch (status) {
        case ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER:
        case ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER:
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN:
            return ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH;
        case ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH;
        case ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH:
            return ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH;
        case ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH:
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_SLOT:
            return ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH;
        case ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION:
            return ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION;
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT:
            return ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        case ZR_EXEC_IR_BINDING_FACTS_SEALED:
            return ZR_EXEC_IR_DIAGNOSTIC_SEALED;
        case ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT:
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN:
        default:
            return ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION;
    }
}

static EZrExecIrBindingFactsStatus status_from_call_binding(EZrCallBindingStatus status) {
    switch (status) {
        case ZR_CALL_BINDING_OK: return ZR_EXEC_IR_BINDING_FACTS_OK;
        case ZR_CALL_BINDING_MISSING_CONTRACT: return ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT;
        case ZR_CALL_BINDING_INVALID_TOKEN: return ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN;
        case ZR_CALL_BINDING_TARGET_NOT_FOUND: return ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER;
        case ZR_CALL_BINDING_AMBIGUOUS_TARGET: return ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER;
        case ZR_CALL_BINDING_SIGNATURE_MISMATCH: return ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH;
        case ZR_CALL_BINDING_MODULE_MISMATCH: return ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH;
        case ZR_CALL_BINDING_LAYOUT_MISMATCH: return ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH;
        case ZR_CALL_BINDING_INVALID_SLOT: return ZR_EXEC_IR_BINDING_FACTS_INVALID_SLOT;
        case ZR_CALL_BINDING_STALE_GENERATION: return ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION;
        default: return ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT;
    }
}

static EZrExecIrBindingFactsStatus fail(
        EZrExecIrBindingFactsStatus status,
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        const SZrExecIrBindingSegment *segment,
        const SZrExecIrBindingRow *row,
        TZrUInt64 expected,
        TZrUInt64 actual,
        SZrExecIrDiagnostic *diagnostic) {
    diagnostic_set(diagnostic, diagnostic_code_for_status(status), facts, function,
                   segment, row, expected, actual);
    return status;
}

static const SZrExecIrBindingSegment *segment_for_row(
        const SZrExecIrBindingFacts *facts, TZrUInt32 rowIndex) {
    TZrUInt32 index;
    if (facts == ZR_NULL || facts->segments == ZR_NULL) return ZR_NULL;
    for (index = 0u; index < facts->segmentCount; ++index) {
        if (facts->segments[index].bindingRow == rowIndex) return &facts->segments[index];
    }
    /* Producers may put the association on the row only.  Accept that
     * spelling as long as the row's segment index is explicit; the resulting
     * projection is identical and remains pointer-free. */
    if (facts->rows != ZR_NULL && rowIndex < facts->rowCount &&
        facts->rows[rowIndex].segmentIndex != ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE) {
        TZrUInt32 segmentIndex = facts->rows[rowIndex].segmentIndex;
        if (segmentIndex < facts->segmentCount &&
            facts->segments[segmentIndex].bindingRow == ZR_EXEC_IR_BINDING_ROW_NONE) {
            return &facts->segments[segmentIndex];
        }
    }
    return ZR_NULL;
}

static TZrUInt32 segment_binding_row(const SZrExecIrBindingFacts *facts,
                                     const SZrExecIrBindingSegment *segment) {
    TZrUInt32 index;
    if (segment == ZR_NULL) return ZR_EXEC_IR_BINDING_ROW_NONE;
    if (segment->bindingRow != ZR_EXEC_IR_BINDING_ROW_NONE) return segment->bindingRow;
    if (facts == ZR_NULL || facts->rows == ZR_NULL) return ZR_EXEC_IR_BINDING_ROW_NONE;
    for (index = 0u; index < facts->rowCount; ++index) {
        if (facts->rows[index].segmentIndex == segment->index) return index;
    }
    return ZR_EXEC_IR_BINDING_ROW_NONE;
}

static const SZrExecIrBindingRow *row_at_index(const SZrExecIrBindingFacts *facts,
                                                TZrUInt32 rowIndex) {
    const SZrExecIrBindingRow *row;
    if (facts == ZR_NULL || facts->rows == ZR_NULL || rowIndex >= facts->rowCount) return ZR_NULL;
    row = &facts->rows[rowIndex];
    /* Rows are ordered by index.  This check is intentionally done by the
     * validator; lookup remains O(1) for the hot projection path. */
    return row;
}

static TZrExecIrInstructionId row_instruction_id(const SZrExecIrBindingFacts *facts,
                                                  TZrUInt32 rowIndex) {
    const SZrExecIrBindingRow *row = row_at_index(facts, rowIndex);
    const SZrExecIrBindingSegment *segment;
    if (row == ZR_NULL) return ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
    if (row->instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) return row->instructionId;
    segment = segment_for_row(facts, rowIndex);
    return segment != ZR_NULL ? segment->instructionId : ZR_EXEC_IR_INSTRUCTION_ID_INVALID;
}

static EZrExecIrBindingFactsStatus validate_function_shape(
        const SZrExecIrFunction *function,
        const SZrExecIrBindingFacts *facts,
        SZrExecIrDiagnostic *diagnostic) {
    if (function == ZR_NULL) return ZR_EXEC_IR_BINDING_FACTS_OK;
    if ((function->valueCount != 0u && function->values == ZR_NULL) ||
        function->valueCount > function->valueCapacity ||
        (function->instructionCount != 0u && function->instructions == ZR_NULL) ||
        function->instructionCount > function->instructionCapacity ||
        (function->blockCount != 0u && function->blocks == ZR_NULL) ||
        function->blockCount > function->blockCapacity) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    ZR_NULL, ZR_NULL, 0u, 0u, diagnostic);
    }
    if (facts != ZR_NULL && facts->functionToken != 0u && function->functionToken != 0u &&
        facts->functionToken != function->functionToken) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                    diagnostic_segment_for_facts(facts), ZR_NULL,
                    facts->functionToken, function->functionToken,
                    diagnostic);
    }
    if (facts != ZR_NULL && facts->signatureHash != 0u && function->signatureHash != 0u &&
        facts->signatureHash != function->signatureHash) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH, facts, function,
                    diagnostic_segment_for_facts(facts), ZR_NULL,
                    facts->signatureHash, function->signatureHash,
                    diagnostic);
    }
    if (facts != ZR_NULL && facts->moduleHash != 0u && function->contract.moduleHash != 0u &&
        facts->moduleHash != function->contract.moduleHash) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH, facts, function,
                    diagnostic_segment_for_facts(facts), ZR_NULL,
                    facts->moduleHash, function->contract.moduleHash,
                    diagnostic);
    }
    if (facts != ZR_NULL && facts->generation != 0u && function->contract.generation != 0u &&
        facts->generation != function->contract.generation) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION, facts, function,
                    diagnostic_segment_for_facts(facts), ZR_NULL,
                    facts->generation, function->contract.generation,
                    diagnostic);
    }
    return ZR_EXEC_IR_BINDING_FACTS_OK;
}

static EZrExecIrBindingFactsStatus validate_location(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrBindingRow *row,
        SZrExecIrDiagnostic *diagnostic) {
    if (row->location.kind > ZR_CALL_BINDING_RELOCATION_VM_MODULE) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, ZR_NULL,
                    ZR_NULL, row, row->location.kind, ZR_CALL_BINDING_RELOCATION_VM_MODULE,
                    diagnostic);
    }
    /* Location records are persisted scalar data.  The existing artifact
     * contract reserves all location flags and uses all-ones only for an
     * invalid owner-depth sentinel; accepting either would make a facts row
     * impossible to relocate deterministically. */
    if (row->location.ownerDepth == ZR_CALL_BINDING_SLOT_NONE ||
        row->location.flags != 0u) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, ZR_NULL,
                    ZR_NULL, row, 0u, row->location.ownerDepth != 0u
                            ? row->location.ownerDepth : row->location.flags,
                    diagnostic);
    }
    if (row->location.kind == ZR_CALL_BINDING_RELOCATION_NONE &&
        row->location.targetIndex != ZR_CALL_BINDING_SLOT_NONE) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, ZR_NULL,
                    ZR_NULL, row, ZR_CALL_BINDING_SLOT_NONE,
                    row->location.targetIndex, diagnostic);
    }
    if (row->location.kind != ZR_CALL_BINDING_RELOCATION_NONE &&
        row->location.targetIndex == ZR_CALL_BINDING_SLOT_NONE) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, ZR_NULL,
                    ZR_NULL, row, 1u, row->location.targetIndex, diagnostic);
    }
    if (row->contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
        (row->location.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
         row->location.targetIndex != ZR_CALL_BINDING_SLOT_NONE ||
         row->location.ownerDepth != 0u)) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, ZR_NULL,
                    ZR_NULL, row, ZR_CALL_BINDING_RELOCATION_NONE,
                    row->location.kind, diagnostic);
    }
    return ZR_EXEC_IR_BINDING_FACTS_OK;
}

static EZrExecIrBindingFactsStatus validate_contract(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrBindingRow *row,
        SZrExecIrDiagnostic *diagnostic) {
    SZrCallBindingDiagnostic callDiagnostic;
    EZrCallBindingStatus callStatus;
    if (row == ZR_NULL) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, ZR_NULL,
                    ZR_NULL, row, 0u, 0u, diagnostic);
    }
    callStatus = ZrCore_CallBinding_CheckContract(&row->contract, &callDiagnostic);
    if (callStatus != ZR_CALL_BINDING_OK) {
        EZrExecIrBindingFactsStatus status = status_from_call_binding(callStatus);
        return fail(status, facts, ZR_NULL, ZR_NULL, row,
                    callDiagnostic.expected, callDiagnostic.actual, diagnostic);
    }
    if (facts != ZR_NULL && facts->moduleHash != 0u &&
        row->contract.moduleSignatureHash != facts->moduleHash) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH, facts, ZR_NULL,
                    ZR_NULL, row, facts->moduleHash,
                    row->contract.moduleSignatureHash, diagnostic);
    }
    return validate_location(facts, row, diagnostic);
}

static EZrExecIrBindingFactsStatus validate_segment(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        const SZrExecIrBindingSegment *segment,
        TZrUInt32 expectedIndex,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 rowIndex;
    if (segment->index != expectedIndex) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                    segment, ZR_NULL, expectedIndex, segment->index, diagnostic);
    }
    if ((int)segment->kind < 0 ||
        segment->kind >= ZR_EXEC_IR_BINDING_SEGMENT_KIND_COUNT ||
        (segment->flags & ~ZR_EXEC_IR_BINDING_SEGMENT_FLAG_KNOWN_MASK) != 0u) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                    segment, ZR_NULL, ZR_EXEC_IR_BINDING_SEGMENT_KIND_COUNT,
                    segment->kind, diagnostic);
    }
    if (segment->operation > ZR_CALL_BINDING_OPERATION_META) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                    segment, ZR_NULL, ZR_CALL_BINDING_OPERATION_META,
                    segment->operation, diagnostic);
    }
    if (segment->receiverValueId != ZR_EXEC_IR_VALUE_ID_INVALID && function != ZR_NULL &&
        segment->receiverValueId > function->valueCount) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    segment, ZR_NULL, function->valueCount, segment->receiverValueId,
                    diagnostic);
    }
    if (segment->resultValueId != ZR_EXEC_IR_VALUE_ID_INVALID && function != ZR_NULL &&
        segment->resultValueId > function->valueCount) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    segment, ZR_NULL, function->valueCount, segment->resultValueId,
                    diagnostic);
    }
    if (function != ZR_NULL) {
        if ((segment->instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
             segment->instructionId > function->instructionCount) ||
            (segment->receiverInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
             segment->receiverInstructionId > function->instructionCount) ||
            (segment->writebackInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID &&
             segment->writebackInstructionId > function->instructionCount)) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                        segment, ZR_NULL, function->instructionCount,
                        segment->instructionId, diagnostic);
        }
    }
    if (segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD) {
        if (segment->operation != ZR_CALL_BINDING_OPERATION_CALL) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment, ZR_NULL, ZR_CALL_BINDING_OPERATION_CALL,
                        segment->operation, diagnostic);
        }
        if ((segment->flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL) != 0u) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment, ZR_NULL, 0u,
                        ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL, diagnostic);
        }
        if (segment->memberToken == 0u && segment->memberId == ZR_EXEC_IR_BINDING_MEMBER_NONE) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER, facts, function,
                        segment, ZR_NULL, 1u, 0u, diagnostic);
        }
        if (segment->memberToken != 0u &&
            (ZR_METADATA_TOKEN_RID(segment->memberToken) == 0u ||
             (ZR_METADATA_TOKEN_TABLE(segment->memberToken) != ZR_METADATA_TABLE_MEMBER_DEF &&
              ZR_METADATA_TOKEN_TABLE(segment->memberToken) != ZR_METADATA_TABLE_MEMBER_REF))) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN, facts, function,
                        segment, ZR_NULL, ZR_METADATA_TABLE_MEMBER_DEF,
                        ZR_METADATA_TOKEN_TABLE(segment->memberToken), diagnostic);
        }
        if (segment->layoutId == 0u &&
            (segment->layoutVersion == 0u || segment->layoutHash == 0u)) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH, facts, function,
                        segment, ZR_NULL, 1u, 0u, diagnostic);
        }
        if ((segment->layoutVersion == 0u) != (segment->layoutHash == 0u)) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH, facts, function,
                        segment, ZR_NULL, segment->layoutVersion,
                        segment->layoutHash, diagnostic);
        }
        if (function != ZR_NULL && function->instructions != ZR_NULL &&
            segment->instructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            TZrUInt16 opcode = function->instructions[segment->instructionId - 1u].opcode;
            if (opcode != ZR_EXEC_IR_OPCODE_PLACE_PROJECT &&
                opcode != ZR_EXEC_IR_OPCODE_LOAD) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment, ZR_NULL, ZR_EXEC_IR_OPCODE_LOAD, opcode, diagnostic);
            }
            if (segment->receiverInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                TZrUInt16 receiverOpcode = function->instructions[
                        segment->receiverInstructionId - 1u].opcode;
                if (receiverOpcode != ZR_EXEC_IR_OPCODE_PLACE_BASE &&
                    receiverOpcode != ZR_EXEC_IR_OPCODE_PLACE_PROJECT &&
                    receiverOpcode != ZR_EXEC_IR_OPCODE_LOAD &&
                    receiverOpcode != ZR_EXEC_IR_OPCODE_COPY &&
                    receiverOpcode != ZR_EXEC_IR_OPCODE_MOVE) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, ZR_NULL, ZR_EXEC_IR_OPCODE_PLACE_PROJECT,
                                receiverOpcode, diagnostic);
                }
            }
        }
        if (segment->bindingRow != ZR_EXEC_IR_BINDING_ROW_NONE) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment, ZR_NULL, ZR_EXEC_IR_BINDING_ROW_NONE,
                        segment->bindingRow, diagnostic);
        }
        return ZR_EXEC_IR_BINDING_FACTS_OK;
    }
    if (segment->kind != ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE &&
        segment->memberToken == 0u && segment->memberId == ZR_EXEC_IR_BINDING_MEMBER_NONE) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER, facts, function,
                    segment, ZR_NULL, 1u, 0u, diagnostic);
    }
    if (segment->memberToken != 0u &&
        (ZR_METADATA_TOKEN_RID(segment->memberToken) == 0u ||
         (ZR_METADATA_TOKEN_TABLE(segment->memberToken) != ZR_METADATA_TABLE_MEMBER_DEF &&
          ZR_METADATA_TOKEN_TABLE(segment->memberToken) != ZR_METADATA_TABLE_MEMBER_REF))) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN, facts, function,
                    segment, ZR_NULL, ZR_METADATA_TABLE_MEMBER_DEF,
                    ZR_METADATA_TOKEN_TABLE(segment->memberToken), diagnostic);
    }
    rowIndex = segment_binding_row(facts, segment);
    if (rowIndex == ZR_EXEC_IR_BINDING_ROW_NONE ||
        facts == ZR_NULL || rowIndex >= facts->rowCount) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT, facts, function,
                    segment, ZR_NULL, facts != ZR_NULL ? facts->rowCount : 0u,
                    rowIndex, diagnostic);
    }
    {
        const SZrExecIrBindingRow *row = row_at_index(facts, rowIndex);
        EZrExecIrBindingFactsStatus status;
        if (row == ZR_NULL) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT, facts, function,
                        segment, row, rowIndex, 0u, diagnostic);
        }
        status = validate_contract(facts, row, diagnostic);
        if (status != ZR_EXEC_IR_BINDING_FACTS_OK) {
            if (diagnostic != ZR_NULL) {
                diagnostic->functionToken = function != ZR_NULL && function->functionToken != 0u
                        ? function->functionToken : facts->functionToken;
                diagnostic->instructionId = segment->instructionId != 0u
                        ? segment->instructionId : row->instructionId;
                diagnostic->blockId = diagnostic_block_for_instruction(function,
                                                                        diagnostic->instructionId);
                diagnostic->sourceId = segment->sourceId != 0u ? segment->sourceId : row->sourceId;
            }
            return status;
        }
        if (row->segmentIndex != ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE &&
            row->segmentIndex != segment->index) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment, row, segment->index, row->segmentIndex, diagnostic);
        }
        if (segment->memberToken != 0u &&
            row->contract.targetMetadataToken != 0u &&
            segment->memberToken != row->contract.targetMetadataToken) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER, facts, function,
                        segment, row, row->contract.targetMetadataToken,
                        segment->memberToken, diagnostic);
        }
        if (segment->layoutVersion != 0u &&
            segment->layoutVersion != row->contract.layoutVersion) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH, facts, function,
                        segment, row, row->contract.layoutVersion,
                        segment->layoutVersion, diagnostic);
        }
        if (segment->layoutHash != 0u &&
            segment->layoutHash != row->contract.layoutHash) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH, facts, function,
                        segment, row, row->contract.layoutHash,
                        segment->layoutHash, diagnostic);
        }
        if (segment->operation != ZR_CALL_BINDING_OPERATION_CALL &&
            segment->operation != row->contract.operation) {
            /* operation==CALL (zero) is the default for callers that do not
             * repeat a non-call row operation.  An explicit GET/SET/META
             * value, however, must agree with the contract. */
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment, row, row->contract.operation,
                        segment->operation, diagnostic);
        }
        switch (segment->kind) {
            case ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR:
                if (row->contract.operation != ZR_CALL_BINDING_OPERATION_GET &&
                    row->contract.operation != ZR_CALL_BINDING_OPERATION_SET) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, ZR_CALL_BINDING_OPERATION_GET,
                                row->contract.operation, diagnostic);
                }
                if (row->contract.operation == ZR_CALL_BINDING_OPERATION_SET &&
                    (segment->flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK) == 0u) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK,
                                segment->flags, diagnostic);
                }
                if (row->contract.operation == ZR_CALL_BINDING_OPERATION_SET &&
                    segment->writebackInstructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, 1u, 0u, diagnostic);
                }
                if (row->contract.operation == ZR_CALL_BINDING_OPERATION_GET &&
                    (segment->flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK) != 0u) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, 0u,
                                ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK, diagnostic);
                }
                if (function != ZR_NULL && function->instructions != ZR_NULL &&
                    segment->writebackInstructionId != ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
                    TZrUInt16 writebackOpcode = function->instructions[
                            segment->writebackInstructionId - 1u].opcode;
                    if (writebackOpcode != ZR_EXEC_IR_OPCODE_STORE &&
                        writebackOpcode != ZR_EXEC_IR_OPCODE_CALL &&
                        writebackOpcode != ZR_EXEC_IR_OPCODE_INVOKE) {
                        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                    segment, row, ZR_EXEC_IR_OPCODE_STORE,
                                    writebackOpcode, diagnostic);
                    }
                }
                break;
            case ZR_EXEC_IR_BINDING_SEGMENT_META:
                if (row->contract.operation != ZR_CALL_BINDING_OPERATION_META) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, ZR_CALL_BINDING_OPERATION_META,
                                row->contract.operation, diagnostic);
                }
                break;
            case ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE:
                if (row->contract.bindingKind != ZR_CALL_BINDING_TYPED_FUNCTION) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, ZR_CALL_BINDING_TYPED_FUNCTION,
                                row->contract.bindingKind, diagnostic);
                }
                if (row->contract.operation != ZR_CALL_BINDING_OPERATION_CALL) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, ZR_CALL_BINDING_OPERATION_CALL,
                                row->contract.operation, diagnostic);
                }
                break;
            case ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL:
                if (row->contract.operation != ZR_CALL_BINDING_OPERATION_CALL &&
                    row->contract.operation != ZR_CALL_BINDING_OPERATION_META) {
                    return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                                segment, row, ZR_CALL_BINDING_OPERATION_CALL,
                                row->contract.operation, diagnostic);
                }
                break;
            default:
                break;
        }
    }
    return ZR_EXEC_IR_BINDING_FACTS_OK;
}

static EZrExecIrBindingFactsStatus validate_rows(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index;
    if (facts->rowCount != 0u && facts->rows == ZR_NULL) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    ZR_NULL, ZR_NULL, facts->rowCount, 0u, diagnostic);
    }
    for (index = 0u; index < facts->rowCount; ++index) {
        const SZrExecIrBindingRow *row = &facts->rows[index];
        EZrExecIrBindingFactsStatus status;
        TZrExecIrInstructionId instructionId;
        if (row->rowIndex != index) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        ZR_NULL, row, index, row->rowIndex, diagnostic);
        }
        status = validate_contract(facts, row, diagnostic);
        if (status != ZR_EXEC_IR_BINDING_FACTS_OK) {
            if (diagnostic != ZR_NULL) {
                diagnostic->functionToken = function != ZR_NULL && function->functionToken != 0u
                        ? function->functionToken : facts->functionToken;
                diagnostic->instructionId = row->instructionId;
                diagnostic->blockId = diagnostic_block_for_instruction(function, row->instructionId);
                diagnostic->sourceId = row->sourceId;
            }
            return status;
        }
        instructionId = row_instruction_id(facts, index);
        if (instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        segment_for_row(facts, index), row, 1u, 0u, diagnostic);
        }
        if (function != ZR_NULL && instructionId > function->instructionCount) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                        segment_for_row(facts, index), row, function->instructionCount,
                        instructionId, diagnostic);
        }
        {
            const SZrExecIrBindingSegment *segment = segment_for_row(facts, index);
            if (segment != ZR_NULL &&
                segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                            segment, row, 0u, segment->kind, diagnostic);
            }
        }
        if (function != ZR_NULL && function->instructions != ZR_NULL) {
            const SZrExecIrBindingSegment *segment = segment_for_row(facts, index);
            TZrBool callLike = (TZrBool)(segment == ZR_NULL ||
                segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR ||
                segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_META ||
                segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE ||
                segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL);
            if (callLike &&
                function->instructions[instructionId - 1u].opcode != ZR_EXEC_IR_OPCODE_CALL &&
                function->instructions[instructionId - 1u].opcode != ZR_EXEC_IR_OPCODE_INVOKE) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                            segment, row, ZR_EXEC_IR_OPCODE_CALL,
                            function->instructions[instructionId - 1u].opcode,
                            diagnostic);
            }
            if (function->instructions[instructionId - 1u].bindingRow != 0u &&
                function->instructions[instructionId - 1u].bindingRow !=
                    ZR_EXEC_IR_BINDING_ROW_NONE &&
                function->instructions[instructionId - 1u].bindingRow != index) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER, facts, function,
                            segment, row,
                            function->instructions[instructionId - 1u].bindingRow,
                            index, diagnostic);
            }
        }
        for (TZrUInt32 prior = 0u; prior < index; ++prior) {
            TZrExecIrInstructionId priorId = row_instruction_id(facts, prior);
            if (priorId == instructionId) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER, facts, function,
                            segment_for_row(facts, index), row, prior, index, diagnostic);
            }
            if (row->segmentIndex != ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE &&
                row->segmentIndex == facts->rows[prior].segmentIndex) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER, facts, function,
                            segment_for_row(facts, index), row, prior, index, diagnostic);
            }
        }
    }
    return ZR_EXEC_IR_BINDING_FACTS_OK;
}

static EZrExecIrBindingFactsStatus validate_final_segment(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 finalCount = 0u;
    TZrUInt32 finalIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
    TZrUInt32 index;
    if (facts->segmentCount == 0u) {
        if (facts->finalSegmentIndex != ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE) {
            return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                        ZR_NULL, ZR_NULL, ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE,
                        facts->finalSegmentIndex, diagnostic);
        }
        return facts->rowCount == 1u ? ZR_EXEC_IR_BINDING_FACTS_OK
                                     : (facts->rowCount == 0u
                                            ? ZR_EXEC_IR_BINDING_FACTS_OK
                                            : fail(ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER,
                                                   facts, function, ZR_NULL, ZR_NULL,
                                                   1u, facts->rowCount, diagnostic));
    }
    for (index = 0u; index < facts->segmentCount; ++index) {
        const SZrExecIrBindingSegment *segment = &facts->segments[index];
        if (segment->kind == ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL ||
            (segment->flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL) != 0u) {
            ++finalCount;
            finalIndex = index;
        }
    }
    if (finalCount != 1u) {
        const SZrExecIrBindingSegment *diagnosticSegment = ZR_NULL;
        for (index = 0u; index < facts->segmentCount; ++index) {
            if (facts->segments[index].kind == ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL ||
                (facts->segments[index].flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL) != 0u) {
                diagnosticSegment = &facts->segments[index];
            }
        }
        return fail(finalCount == 0u ? ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT
                                     : ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER,
                    facts, function, diagnosticSegment, ZR_NULL, 1u, finalCount,
                    diagnostic);
    }
    if (facts->finalSegmentIndex != ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE &&
        facts->finalSegmentIndex != finalIndex) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN, facts, function,
                    &facts->segments[finalIndex], ZR_NULL, finalIndex,
                    facts->finalSegmentIndex, diagnostic);
    }
    return ZR_EXEC_IR_BINDING_FACTS_OK;
}

EZrExecIrBindingFactsStatus ZrParser_ExecIr_BindingFacts_ValidateEx(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    EZrExecIrBindingFactsStatus status;
    TZrUInt32 index;
    diagnostic_init(diagnostic);
    if (facts == ZR_NULL) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    ZR_NULL, ZR_NULL, 1u, 0u, diagnostic);
    }
    if (facts->schemaVersion != ZR_EXEC_IR_BINDING_FACTS_SCHEMA_VERSION) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    ZR_NULL, ZR_NULL, ZR_EXEC_IR_BINDING_FACTS_SCHEMA_VERSION,
                    facts->schemaVersion, diagnostic);
    }
    if (facts->segmentCount != 0u && facts->segments == ZR_NULL) {
        return fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                    ZR_NULL, ZR_NULL, facts->segmentCount, 0u, diagnostic);
    }
    status = validate_function_shape(function, facts, diagnostic);
    if (status != ZR_EXEC_IR_BINDING_FACTS_OK) return status;
    if (facts->expectedHash != 0u &&
        facts->expectedHash != ZrParser_ExecIr_BindingFacts_Hash(facts)) {
        const SZrExecIrBindingSegment *finalSegment = ZR_NULL;
        for (index = 0u; index < facts->segmentCount; ++index) {
            if (facts->segments[index].kind == ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL ||
                facts->segments[index].kind == ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE ||
                (facts->segments[index].flags & ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL) != 0u) {
                finalSegment = &facts->segments[index];
                break;
            }
        }
        return fail(ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH, facts, function,
                    finalSegment, ZR_NULL, facts->expectedHash,
                    ZrParser_ExecIr_BindingFacts_Hash(facts), diagnostic);
    }
    status = validate_rows(facts, function, diagnostic);
    if (status != ZR_EXEC_IR_BINDING_FACTS_OK) return status;
    for (index = 0u; index < facts->segmentCount; ++index) {
        status = validate_segment(facts, function, &facts->segments[index], index, diagnostic);
        if (status != ZR_EXEC_IR_BINDING_FACTS_OK) return status;
        for (TZrUInt32 prior = 0u; prior < index; ++prior) {
            if (facts->segments[prior].bindingRow != ZR_EXEC_IR_BINDING_ROW_NONE &&
                facts->segments[prior].bindingRow == facts->segments[index].bindingRow) {
                return fail(ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER, facts, function,
                            &facts->segments[index], ZR_NULL, prior, index, diagnostic);
            }
        }
    }
    return validate_final_segment(facts, function, diagnostic);
}

TZrBool ZrParser_ExecIr_BindingFacts_Validate(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_BindingFacts_ValidateEx(facts, function, diagnostic) ==
           ZR_EXEC_IR_BINDING_FACTS_OK;
}

TZrBool ZrParser_ExecIr_ProjectBindingFacts(
        const SZrExecIrBindingFacts *facts,
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    EZrExecIrBindingFactsStatus status;
    TZrUInt32 index;
    diagnostic_init(diagnostic);
    if (function == ZR_NULL || facts == ZR_NULL) {
        (void)fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                   ZR_NULL, ZR_NULL, 1u, 0u, diagnostic);
        return ZR_FALSE;
    }
    if (function->sealed) {
        fail(ZR_EXEC_IR_BINDING_FACTS_SEALED, facts, function,
             ZR_NULL, ZR_NULL, 0u, 1u, diagnostic);
        return ZR_FALSE;
    }
    status = ZrParser_ExecIr_BindingFacts_ValidateEx(facts, function, diagnostic);
    if (status != ZR_EXEC_IR_BINDING_FACTS_OK) return ZR_FALSE;

    /* Validation above is complete before this loop, so projection is
     * transactional with respect to malformed facts: either all rows are
     * copied or none are touched. */
    for (index = 0u; index < facts->rowCount; ++index) {
        TZrExecIrInstructionId instructionId = row_instruction_id(facts, index);
        if (instructionId == ZR_EXEC_IR_INSTRUCTION_ID_INVALID ||
            instructionId > function->instructionCount) {
            /* This should be unreachable after ValidateEx; retain a guarded
             * check in case a producer mutates borrowed arrays concurrently. */
            fail(ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT, facts, function,
                 segment_for_row(facts, index), &facts->rows[index],
                 function->instructionCount, instructionId, diagnostic);
            return ZR_FALSE;
        }
        function->instructions[instructionId - 1u].bindingRow = index;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BindingFacts_Project(
        const SZrExecIrBindingFacts *facts,
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrParser_ExecIr_ProjectBindingFacts(facts, function, diagnostic);
}

const SZrExecIrBindingRow *ZrParser_ExecIr_BindingFacts_RowAt(
        const SZrExecIrBindingFacts *facts, TZrUInt32 rowIndex) {
    return row_at_index(facts, rowIndex);
}

const SZrExecIrBindingSegment *ZrParser_ExecIr_BindingFacts_SegmentAt(
        const SZrExecIrBindingFacts *facts, TZrUInt32 segmentIndex) {
    if (facts == ZR_NULL || facts->segments == ZR_NULL ||
        segmentIndex >= facts->segmentCount) return ZR_NULL;
    return &facts->segments[segmentIndex];
}

const TZrChar *ZrParser_ExecIr_BindingFacts_StatusName(
        EZrExecIrBindingFactsStatus status) {
    switch (status) {
        case ZR_EXEC_IR_BINDING_FACTS_OK: return "ok";
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER: return "unknown-member";
        case ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER: return "ambiguous-member";
        case ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT: return "missing-contract";
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN: return "invalid-token";
        case ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH: return "signature-mismatch";
        case ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH: return "module-mismatch";
        case ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH: return "layout-mismatch";
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_SLOT: return "invalid-slot";
        case ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION: return "stale-generation";
        case ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN: return "invalid-chain";
        case ZR_EXEC_IR_BINDING_FACTS_SEALED: return "sealed";
        default: return "unknown-binding-facts-error";
    }
}
