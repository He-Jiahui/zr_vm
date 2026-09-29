#include "artifact_exec_ir_scalar_eis5_internal.h"
#include "zr_vm_common/zr_type_conf.h"

#include <string.h>

static TZrBool eis5_checked_add(TZrUInt64 left, TZrUInt64 right,
                                TZrUInt64 *out) {
    if (out == ZR_NULL || right > UINT64_MAX - left) return ZR_FALSE;
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool eis5_checked_mul(TZrUInt64 left, TZrUInt64 right,
                                TZrUInt64 *out) {
    if (out == ZR_NULL || (left != 0u && right > UINT64_MAX / left))
        return ZR_FALSE;
    *out = left * right;
    return ZR_TRUE;
}

static TZrBool eis5_range_is(SZrExecIrRange range,
                             TZrUInt32 start, TZrUInt32 count) {
    return (TZrBool)(range.start == start && range.count == count);
}

static TZrBool eis5_range_fits(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)((TZrUInt64)range.start + range.count <= count);
}

static EZrArtifactExecIrStatus eis5_limit(
        SZrArtifactExecIrDiagnostic *diagnostic, TZrUInt32 offset) {
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, offset);
}

static TZrBool eis5_contract_is(const SZrExecutionContract *contract,
                                TZrMetadataToken target,
                                TZrUInt64 signatureHash,
                                TZrUInt64 layoutHash,
                                TZrUInt64 moduleHash) {
    return (TZrBool)(contract->schemaVersion ==
                             ZR_EXECUTION_CONTRACT_SCHEMA_VERSION &&
                     contract->abiVersion ==
                             ZR_EXECUTION_CONTRACT_ABI_VERSION &&
                     contract->logicalVersion ==
                             ZR_EXECUTION_CONTRACT_LOGICAL_VERSION &&
                     contract->reserved0 == 0u && contract->reserved1 == 0u &&
                     contract->generation == 1u &&
                     contract->targetToken == target &&
                     contract->signatureHash == signatureHash &&
                     contract->layoutHash == layoutHash && layoutHash != 0u &&
                     contract->moduleHash == moduleHash && moduleHash != 0u &&
                     contract->requiredCapabilities == 0u &&
                     contract->declaredEffects == 0u);
}

static TZrBool eis5_block_aux_is(const SZrExecIrBlock *block) {
    if (!eis5_range_is(block->phis, 0u, 0u) ||
        block->effectPhiResult != 0u ||
        !eis5_range_is(block->effectPhiIncomings, 0u, 0u))
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < ZR_EXEC_IR_MEMORY_CLASS_COUNT;
         ++index) {
        if (block->memoryPhiResults[index] != 0u ||
            !eis5_range_is(block->memoryPhiIncomings[index], 0u, 0u))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool eis5_function_side_tables_are_empty(
        const SZrExecIrFunction *function) {
    return (TZrBool)(function->memoryTokenCount == 0u &&
                     function->memoryTokenPool == ZR_NULL &&
                     function->phiCount == 0u && function->phiPool == ZR_NULL &&
                     function->phiIncomingCount == 0u &&
                     function->phiIncoming == ZR_NULL &&
                     function->frameLayout == ZR_NULL &&
                     function->gcMap == ZR_NULL && function->gcMapCount == 0u &&
                     function->gcRootCount == 0u && function->gcRoots == ZR_NULL &&
                     function->deoptStateCount == 0u &&
                     function->deoptStates == ZR_NULL &&
                     function->deoptValueCount == 0u &&
                     function->deoptValues == ZR_NULL &&
                     function->deoptAggregateCount == 0u &&
                     function->deoptAggregates == ZR_NULL &&
                     function->deoptAggregateFieldCount == 0u &&
                     function->deoptAggregateFields == ZR_NULL &&
                     function->sourceMapCount == 0u &&
                     function->sourceMaps == ZR_NULL &&
                     function->stateMap == ZR_NULL);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_Fail(
        SZrArtifactExecIrDiagnostic *diagnostic,
        EZrArtifactExecIrStatus status, TZrUInt32 offset) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
        diagnostic->sectionKind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR;
        diagnostic->byteOffset = offset;
    }
    return status;
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_ComputeLayout(
        const SEis5Counts *counts, SEis5Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrUInt64 scalarNodes, poolItems, total, bytes;
    TZrUInt64 next;
    if (counts == ZR_NULL || layout == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    memset(layout, 0, sizeof(*layout));
    layout->counts = *counts;

    if (counts->blocks > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BLOCKS)
        return eis5_limit(diagnostic,
                          ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_COUNT_OFFSET);
    if (counts->constants == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_COUNT_OFFSET);
    if (counts->values == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_COUNT_OFFSET);
    if (counts->blocks == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_COUNT_OFFSET);
    if (counts->instructions < 2u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_COUNT_OFFSET);
    if (counts->operands == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_OPERAND_COUNT_OFFSET);
    if (counts->results == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_RESULT_COUNT_OFFSET);

    scalarNodes = counts->constants;
    if (!eis5_checked_add(scalarNodes, counts->values, &next) ||
        next > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES)
        return eis5_limit(diagnostic,
                          ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_COUNT_OFFSET);
    scalarNodes = next;
    if (!eis5_checked_add(scalarNodes, counts->instructions, &next) ||
        next > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES)
        return eis5_limit(diagnostic,
                          ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_COUNT_OFFSET);

    poolItems = counts->operands;
    if (!eis5_checked_add(poolItems, counts->results, &next) ||
        next > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_POOL_ITEMS)
        return eis5_limit(diagnostic,
                          ZR_ARTIFACT_EXEC_IR_EIS5_RESULT_COUNT_OFFSET);
    poolItems = next;
    if (!eis5_checked_add(poolItems, counts->successors, &next) ||
        next > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_POOL_ITEMS)
        return eis5_limit(diagnostic,
                          ZR_ARTIFACT_EXEC_IR_EIS5_SUCCESSOR_COUNT_OFFSET);
    poolItems = next;
    if (!eis5_checked_add(poolItems, counts->predecessors, &next) ||
        next > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_POOL_ITEMS)
        return eis5_limit(diagnostic,
                          ZR_ARTIFACT_EXEC_IR_EIS5_PREDECESSOR_COUNT_OFFSET);

    total = (TZrUInt64)ZR_ARTIFACT_EXEC_IR_EIS5_HEADER_SIZE +
            ZR_ARTIFACT_EXEC_IR_EIS5_FIXED_PREFIX_SIZE;
    layout->constantsOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->constants,
                          ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->valuesOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->values,
                          ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->blocksOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->blocks,
                          ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->instructionsOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->instructions,
                          ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->resultsOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->results,
                          ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->operandsOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->operands,
                          ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->successorsOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->successors,
                          ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > UINT32_MAX) return eis5_limit(
            diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->predecessorsOffset = (TZrUInt32)total;
    if (!eis5_checked_mul(counts->predecessors,
                          ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE, &bytes) ||
        !eis5_checked_add(total, bytes, &total))
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (total > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BYTES ||
        total > UINT32_MAX)
        return eis5_limit(diagnostic, ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    layout->totalSize = (TZrUInt32)total;
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

TZrUInt16 ZrCore_ArtifactExecIrScalarEis5_Get16(SEis5Cursor *cursor) {
    const TZrByte *bytes = cursor->bytes + cursor->offset;
    cursor->offset += 2u;
    return (TZrUInt16)((TZrUInt16)bytes[0] | ((TZrUInt16)bytes[1] << 8u));
}

TZrUInt32 ZrCore_ArtifactExecIrScalarEis5_Get32(SEis5Cursor *cursor) {
    TZrUInt32 value = 0u;
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        value |= (TZrUInt32)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

TZrUInt64 ZrCore_ArtifactExecIrScalarEis5_Get64(SEis5Cursor *cursor) {
    TZrUInt64 value = 0u;
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        value |= (TZrUInt64)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

void ZrCore_ArtifactExecIrScalarEis5_Put16(SEis5Cursor *cursor,
                                            TZrUInt16 value) {
    cursor->bytes[cursor->offset++] = (TZrByte)value;
    cursor->bytes[cursor->offset++] = (TZrByte)(value >> 8u);
}

void ZrCore_ArtifactExecIrScalarEis5_Put32(SEis5Cursor *cursor,
                                            TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

void ZrCore_ArtifactExecIrScalarEis5_Put64(SEis5Cursor *cursor,
                                            TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

SZrExecIrRange ZrCore_ArtifactExecIrScalarEis5_GetRange(
        SEis5Cursor *cursor) {
    SZrExecIrRange range;
    range.start = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    range.count = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    return range;
}

void ZrCore_ArtifactExecIrScalarEis5_PutRange(
        SEis5Cursor *cursor, SZrExecIrRange range) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, range.start);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, range.count);
}

void ZrCore_ArtifactExecIrScalarEis5_GetContract(
        SEis5Cursor *cursor, SZrExecutionContract *contract) {
    contract->schemaVersion = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->abiVersion = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->logicalVersion = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->reserved0 = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->generation = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    contract->targetToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->reserved1 = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->signatureHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    contract->layoutHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    contract->moduleHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    contract->requiredCapabilities =
            ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    contract->declaredEffects = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
}

void ZrCore_ArtifactExecIrScalarEis5_PutContract(
        SEis5Cursor *cursor, const SZrExecutionContract *contract) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->schemaVersion);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->abiVersion);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->logicalVersion);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->reserved0);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, contract->generation);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->targetToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->reserved1);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, contract->signatureHash);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, contract->layoutHash);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, contract->moduleHash);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            cursor, contract->requiredCapabilities);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, contract->declaredEffects);
}

void ZrCore_ArtifactExecIrScalarEis5_GetConstant(
        SEis5Cursor *cursor, SZrExecIrConstant *constant) {
    constant->typeToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    constant->flags = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    constant->bits = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
}

void ZrCore_ArtifactExecIrScalarEis5_PutConstant(
        SEis5Cursor *cursor, const SZrExecIrConstant *constant) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, constant->typeToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, constant->flags);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, constant->bits);
}

void ZrCore_ArtifactExecIrScalarEis5_GetValue(
        SEis5Cursor *cursor, SZrExecIrValue *value) {
    value->id = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    value->definition = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    value->typeToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    value->ownership = (EZrExecIrOwnership)
            ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    value->nullability = (EZrExecIrNullability)
            ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    value->flags = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
}

void ZrCore_ArtifactExecIrScalarEis5_PutValue(
        SEis5Cursor *cursor, const SZrExecIrValue *value) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, value->id);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, value->definition);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, value->typeToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, value->ownership);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, value->nullability);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, value->flags);
}

void ZrCore_ArtifactExecIrScalarEis5_GetBlock(
        SEis5Cursor *cursor, SZrExecIrBlock *block) {
    block->id = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    block->flags = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    block->instructions = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    block->predecessors = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    block->successors = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    block->immediateDominator = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    block->terminatorInstructionId =
            ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
}

void ZrCore_ArtifactExecIrScalarEis5_PutBlock(
        SEis5Cursor *cursor, const SZrExecIrBlock *block) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, block->id);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, block->flags);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, block->instructions);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, block->predecessors);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, block->successors);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, block->immediateDominator);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            cursor, block->terminatorInstructionId);
}

void ZrCore_ArtifactExecIrScalarEis5_GetInstruction(
        SEis5Cursor *cursor, SZrExecIrInstruction *instruction) {
    instruction->opcode = ZrCore_ArtifactExecIrScalarEis5_Get16(cursor);
    instruction->flags = ZrCore_ArtifactExecIrScalarEis5_Get16(cursor);
    instruction->results = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    instruction->operands = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    instruction->phiRange = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    instruction->successorRange =
            ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    instruction->typeToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->matchTypeToken =
            ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->layoutId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->memoryIn = ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    instruction->memoryOut =
            ZrCore_ArtifactExecIrScalarEis5_GetRange(cursor);
    instruction->effectIn = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->effectOut = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->sourceId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->deoptId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    instruction->bindingRow = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
}

void ZrCore_ArtifactExecIrScalarEis5_PutInstruction(
        SEis5Cursor *cursor, const SZrExecIrInstruction *instruction) {
    ZrCore_ArtifactExecIrScalarEis5_Put16(cursor, instruction->opcode);
    ZrCore_ArtifactExecIrScalarEis5_Put16(cursor, instruction->flags);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, instruction->results);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, instruction->operands);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, instruction->phiRange);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(
            cursor, instruction->successorRange);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->typeToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor,
                                           instruction->matchTypeToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->layoutId);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, instruction->memoryIn);
    ZrCore_ArtifactExecIrScalarEis5_PutRange(cursor, instruction->memoryOut);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->effectIn);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->effectOut);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->sourceId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->deoptId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, instruction->bindingRow);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_ValidateModule(
        const SZrExecIrModule *module, SEis5Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis5Counts counts;
    const SZrExecIrFunction *function;
    TZrUInt32 definitionByValue[ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES];
    TZrUInt32 *definitions = definitionByValue;
    EZrArtifactExecIrStatus status;
    if (module == ZR_NULL || layout == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (module->functionCount != 1u || module->functions == ZR_NULL ||
        module->layoutCount != 0u || module->layouts != ZR_NULL ||
        module->sourceMapCount != 0u || module->sourceMaps != ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    function = &module->functions[0];
    counts.constants = module->constantCount;
    counts.values = function->valueCount;
    counts.blocks = function->blockCount;
    counts.instructions = function->instructionCount;
    counts.operands = function->operandCount;
    counts.results = function->resultCount;
    counts.successors = function->successorCount;
    counts.predecessors = function->predecessorCount;
    status = ZrCore_ArtifactExecIrScalarEis5_ComputeLayout(
            &counts, layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (module->constants == ZR_NULL || function->values == ZR_NULL ||
        function->blocks == ZR_NULL || function->instructions == ZR_NULL ||
        function->operands == ZR_NULL || function->results == ZR_NULL ||
        (counts.successors != 0u && function->successors == ZR_NULL) ||
        (counts.predecessors != 0u && function->predecessors == ZR_NULL) ||
        !eis5_function_side_tables_are_empty(function) ||
        (function->sealed != ZR_FALSE && function->sealed != ZR_TRUE))
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    if (module->id != 1u || module->moduleToken == 0u || module->moduleHash == 0u ||
        function->id != 1u || function->functionToken == 0u ||
        function->signatureHash == 0u || function->entryBlockId != 1u ||
        !eis5_contract_is(&module->contract, module->moduleToken,
                          function->signatureHash,
                          function->contract.layoutHash, module->moduleHash) ||
        !eis5_contract_is(&function->contract, function->functionToken,
                          function->signatureHash,
                          module->contract.layoutHash, module->moduleHash))
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    if (function->resultCount != function->valueCount)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);

    for (TZrUInt32 index = 0u; index < counts.constants; ++index) {
        const SZrExecIrConstant *constant = &module->constants[index];
        if ((constant->typeToken != ZR_VALUE_TYPE_INT64 &&
             constant->typeToken != ZR_VALUE_TYPE_BOOL) ||
            constant->flags != 0u)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->constantsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_SIZE);
        if (constant->typeToken == ZR_VALUE_TYPE_BOOL && constant->bits > 1u)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->constantsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_SIZE + 8u);
    }
    memset(definitions, 0, sizeof(definitionByValue));
    for (TZrUInt32 index = 0u; index < counts.values; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        if (value->id != index + 1u ||
            (value->typeToken != ZR_VALUE_TYPE_INT64 &&
             value->typeToken != ZR_VALUE_TYPE_BOOL) ||
            value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            value->flags != 0u || value->definition == 0u ||
            value->definition > counts.instructions)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->valuesOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
    }
    for (TZrUInt32 index = 0u; index < counts.blocks; ++index) {
        const SZrExecIrBlock *block = &function->blocks[index];
        TZrUInt32 expectedFlags = index == 0u
                                          ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY
                                          : 0u;
        if (block->id != index + 1u || block->flags != expectedFlags ||
            !eis5_range_fits(block->instructions, counts.instructions) ||
            !eis5_range_fits(block->predecessors, counts.predecessors) ||
            !eis5_range_fits(block->successors, counts.successors) ||
            block->immediateDominator > counts.blocks ||
            !eis5_block_aux_is(block))
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->blocksOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_SIZE);
    }
    for (TZrUInt32 index = 0u; index < counts.instructions; ++index) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[index];
        TZrUInt32 expectedResults, expectedOperands, expectedSuccessors;
        switch ((EZrExecIrOpcode)instruction->opcode) {
            case ZR_EXEC_IR_OPCODE_CONSTANT:
                expectedResults = 1u;
                expectedOperands = 0u;
                expectedSuccessors = 0u;
                if (instruction->layoutId >= counts.constants)
                    return ZrCore_ArtifactExecIrScalarEis5_Fail(
                            diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                            layout->instructionsOffset + index *
                                    ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE +
                                    44u);
                break;
            case ZR_EXEC_IR_OPCODE_ADD:
                expectedResults = 1u;
                expectedOperands = 2u;
                expectedSuccessors = 0u;
                break;
            case ZR_EXEC_IR_OPCODE_BRANCH:
                expectedResults = 0u;
                expectedOperands = 0u;
                expectedSuccessors = 1u;
                break;
            case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
                expectedResults = 0u;
                expectedOperands = 1u;
                expectedSuccessors = 2u;
                break;
            case ZR_EXEC_IR_OPCODE_RETURN:
                expectedResults = 0u;
                expectedOperands = 1u;
                expectedSuccessors = 0u;
                break;
            default:
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->instructionsOffset + index *
                                ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE);
        }
        if (instruction->flags != 0u ||
            instruction->results.count != expectedResults ||
            instruction->operands.count != expectedOperands ||
            instruction->successorRange.count != expectedSuccessors ||
            !eis5_range_fits(instruction->results, counts.results) ||
            !eis5_range_fits(instruction->operands, counts.operands) ||
            !eis5_range_fits(instruction->phiRange, 0u) ||
            !eis5_range_fits(instruction->successorRange, counts.successors) ||
            instruction->typeToken != 0u || instruction->matchTypeToken != 0u ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT &&
             instruction->layoutId != 0u) ||
            !eis5_range_is(instruction->memoryIn, 0u, 0u) ||
            !eis5_range_is(instruction->memoryOut, 0u, 0u) ||
            instruction->effectIn != 0u || instruction->effectOut != 0u ||
            instruction->deoptId != 0u || instruction->bindingRow != 0u)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->instructionsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE);
        for (TZrUInt32 item = instruction->results.start;
             item < instruction->results.start + instruction->results.count;
             ++item) {
            TZrExecIrValueId valueId = function->results[item];
            TZrExecIrTypeToken expectedType;
            if (valueId == 0u || valueId > counts.values ||
                definitions[valueId - 1u] != 0u)
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->resultsOffset + item *
                                ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
            expectedType = instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT
                                   ? module->constants[instruction->layoutId]
                                             .typeToken
                                   : (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64;
            if (function->values[valueId - 1u].typeToken != expectedType)
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->valuesOffset + (valueId - 1u) *
                                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
            definitions[valueId - 1u] = index + 1u;
        }
        for (TZrUInt32 item = instruction->operands.start;
             item < instruction->operands.start + instruction->operands.count;
             ++item) {
            TZrExecIrValueId valueId = function->operands[item];
            TZrExecIrTypeToken operandType;
            if (valueId == 0u || valueId > counts.values)
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->operandsOffset + item *
                                ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
            operandType = function->values[valueId - 1u].typeToken;
            if ((instruction->opcode ==
                         ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                 operandType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL &&
                 operandType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64) ||
                (instruction->opcode !=
                         ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                 operandType != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64))
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->valuesOffset + (valueId - 1u) *
                                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
        }
    }
    for (TZrUInt32 index = 0u; index < counts.values; ++index) {
        if (definitions[index] == 0u ||
            function->values[index].definition != definitions[index])
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->valuesOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE + 4u);
    }
    for (TZrUInt32 index = 0u; index < counts.successors; ++index) {
        if (function->successors[index] == 0u ||
            function->successors[index] > counts.blocks)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->successorsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
    }
    for (TZrUInt32 index = 0u; index < counts.predecessors; ++index) {
        if (function->predecessors[index] == 0u ||
            function->predecessors[index] > counts.blocks)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->predecessorsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
    }
    if (!ZrCore_ExecIr_VerifyModule(module, ZR_NULL))
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis5Layout layout;
    EZrArtifactExecIrStatus status;
    if (module == ZR_NULL || outSize == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalarEis5_ValidateModule(
            module, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    *outSize = layout.totalSize;
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}
