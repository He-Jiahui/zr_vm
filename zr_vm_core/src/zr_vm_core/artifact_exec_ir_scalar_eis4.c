#include "artifact_exec_ir_scalar_eis4.h"
#include "zr_vm_common/zr_type_conf.h"

#include <string.h>

typedef struct SEis4Cursor {
    TZrByte *bytes;
    TZrUInt32 offset;
} SEis4Cursor;

typedef struct SEis4Record {
    TZrUInt32 moduleId;
    TZrMetadataToken moduleToken;
    TZrUInt64 moduleHash;
    SZrExecutionContract moduleContract;
    TZrUInt32 functionId;
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    TZrUInt32 entryBlockId;
    TZrUInt32 sealed;
    SZrExecutionContract functionContract;
    TZrUInt32 counts[8];
    SZrExecIrConstant constants[2];
    SZrExecIrValue values[3];
    SZrExecIrBlock block;
    SZrExecIrInstruction instructions[4];
    TZrUInt32 resultIds[3];
    TZrUInt32 operandIds[3];
} SEis4Record;

#define EIS4_MODULE_ID_OFFSET 12u
#define EIS4_FUNCTION_ID_OFFSET 92u
#define EIS4_FUNCTION_ENTRY_BLOCK_OFFSET 108u
#define EIS4_COUNTS_OFFSET 180u
#define EIS4_BLOCK_OFFSET 316u
#define EIS4_BLOCK_INSTRUCTION_START_OFFSET 324u
#define EIS4_BLOCK_INSTRUCTION_COUNT_OFFSET 328u
#define EIS4_BLOCK_PREDECESSOR_START_OFFSET 332u
#define EIS4_BLOCK_PREDECESSOR_COUNT_OFFSET 336u
#define EIS4_BLOCK_SUCCESSOR_START_OFFSET 340u
#define EIS4_BLOCK_SUCCESSOR_COUNT_OFFSET 344u
#define EIS4_BLOCK_DOMINATOR_OFFSET 348u
#define EIS4_BLOCK_TERMINATOR_OFFSET 352u
#define EIS4_CONSTANTS_OFFSET 212u
#define EIS4_FIRST_CONSTANT_BITS_OFFSET 220u
#define EIS4_SECOND_CONSTANT_BITS_OFFSET 236u
#define EIS4_VALUES_OFFSET 244u
#define EIS4_INSTRUCTIONS_OFFSET 356u
#define EIS4_ADD_OPCODE_OFFSET 524u
#define EIS4_ADD_RESULTS_OFFSET 528u
#define EIS4_ADD_OPERANDS_OFFSET 536u
#define EIS4_RESULTS_OFFSET 692u
#define EIS4_THIRD_RESULT_ID_OFFSET 700u
#define EIS4_OPERANDS_OFFSET 704u

static EZrArtifactExecIrStatus eis4_fail(
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

static void eis4_put16(SEis4Cursor *cursor, TZrUInt16 value) {
    cursor->bytes[cursor->offset++] = (TZrByte)value;
    cursor->bytes[cursor->offset++] = (TZrByte)(value >> 8u);
}

static void eis4_put32(SEis4Cursor *cursor, TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

static void eis4_put64(SEis4Cursor *cursor, TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

static TZrUInt16 eis4_get16(SEis4Cursor *cursor) {
    const TZrByte *bytes = cursor->bytes + cursor->offset;
    cursor->offset += 2u;
    return (TZrUInt16)((TZrUInt16)bytes[0] | ((TZrUInt16)bytes[1] << 8u));
}

static TZrUInt32 eis4_get32(SEis4Cursor *cursor) {
    TZrUInt32 value = 0u;
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        value |= (TZrUInt32)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

static TZrUInt64 eis4_get64(SEis4Cursor *cursor) {
    TZrUInt64 value = 0u;
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        value |= (TZrUInt64)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

static void eis4_put_range(SEis4Cursor *cursor, SZrExecIrRange range) {
    eis4_put32(cursor, range.start);
    eis4_put32(cursor, range.count);
}

static SZrExecIrRange eis4_get_range(SEis4Cursor *cursor) {
    SZrExecIrRange range;
    range.start = eis4_get32(cursor);
    range.count = eis4_get32(cursor);
    return range;
}

static void eis4_put_contract(SEis4Cursor *cursor,
                              const SZrExecutionContract *contract) {
    eis4_put32(cursor, contract->schemaVersion);
    eis4_put32(cursor, contract->abiVersion);
    eis4_put32(cursor, contract->logicalVersion);
    eis4_put32(cursor, contract->reserved0);
    eis4_put64(cursor, contract->generation);
    eis4_put32(cursor, contract->targetToken);
    eis4_put32(cursor, contract->reserved1);
    eis4_put64(cursor, contract->signatureHash);
    eis4_put64(cursor, contract->layoutHash);
    eis4_put64(cursor, contract->moduleHash);
    eis4_put32(cursor, contract->requiredCapabilities);
    eis4_put32(cursor, contract->declaredEffects);
}

static void eis4_get_contract(SEis4Cursor *cursor,
                              SZrExecutionContract *contract) {
    contract->schemaVersion = eis4_get32(cursor);
    contract->abiVersion = eis4_get32(cursor);
    contract->logicalVersion = eis4_get32(cursor);
    contract->reserved0 = eis4_get32(cursor);
    contract->generation = eis4_get64(cursor);
    contract->targetToken = eis4_get32(cursor);
    contract->reserved1 = eis4_get32(cursor);
    contract->signatureHash = eis4_get64(cursor);
    contract->layoutHash = eis4_get64(cursor);
    contract->moduleHash = eis4_get64(cursor);
    contract->requiredCapabilities = eis4_get32(cursor);
    contract->declaredEffects = eis4_get32(cursor);
}

static void eis4_put_constant(SEis4Cursor *cursor,
                              const SZrExecIrConstant *constant) {
    eis4_put32(cursor, constant->typeToken);
    eis4_put32(cursor, constant->flags);
    eis4_put64(cursor, constant->bits);
}

static void eis4_get_constant(SEis4Cursor *cursor,
                              SZrExecIrConstant *constant) {
    constant->typeToken = eis4_get32(cursor);
    constant->flags = eis4_get32(cursor);
    constant->bits = eis4_get64(cursor);
}

static void eis4_put_value(SEis4Cursor *cursor,
                           const SZrExecIrValue *value) {
    eis4_put32(cursor, value->id);
    eis4_put32(cursor, value->definition);
    eis4_put32(cursor, value->typeToken);
    eis4_put32(cursor, value->ownership);
    eis4_put32(cursor, value->nullability);
    eis4_put32(cursor, value->flags);
}

static void eis4_get_value(SEis4Cursor *cursor, SZrExecIrValue *value) {
    value->id = eis4_get32(cursor);
    value->definition = eis4_get32(cursor);
    value->typeToken = eis4_get32(cursor);
    value->ownership = (EZrExecIrOwnership)eis4_get32(cursor);
    value->nullability = (EZrExecIrNullability)eis4_get32(cursor);
    value->flags = eis4_get32(cursor);
}

static void eis4_put_block(SEis4Cursor *cursor,
                           const SZrExecIrBlock *block) {
    eis4_put32(cursor, block->id);
    eis4_put32(cursor, block->flags);
    eis4_put_range(cursor, block->instructions);
    eis4_put_range(cursor, block->predecessors);
    eis4_put_range(cursor, block->successors);
    eis4_put32(cursor, block->immediateDominator);
    eis4_put32(cursor, block->terminatorInstructionId);
}

static void eis4_get_block(SEis4Cursor *cursor, SZrExecIrBlock *block) {
    block->id = eis4_get32(cursor);
    block->flags = eis4_get32(cursor);
    block->instructions = eis4_get_range(cursor);
    block->predecessors = eis4_get_range(cursor);
    block->successors = eis4_get_range(cursor);
    block->immediateDominator = eis4_get32(cursor);
    block->terminatorInstructionId = eis4_get32(cursor);
}

static void eis4_put_instruction(
        SEis4Cursor *cursor, const SZrExecIrInstruction *instruction) {
    eis4_put16(cursor, instruction->opcode);
    eis4_put16(cursor, instruction->flags);
    eis4_put_range(cursor, instruction->results);
    eis4_put_range(cursor, instruction->operands);
    eis4_put_range(cursor, instruction->phiRange);
    eis4_put_range(cursor, instruction->successorRange);
    eis4_put32(cursor, instruction->typeToken);
    eis4_put32(cursor, instruction->matchTypeToken);
    eis4_put32(cursor, instruction->layoutId);
    eis4_put_range(cursor, instruction->memoryIn);
    eis4_put_range(cursor, instruction->memoryOut);
    eis4_put32(cursor, instruction->effectIn);
    eis4_put32(cursor, instruction->effectOut);
    eis4_put32(cursor, instruction->sourceId);
    eis4_put32(cursor, instruction->deoptId);
    eis4_put32(cursor, instruction->bindingRow);
}

static void eis4_get_instruction(
        SEis4Cursor *cursor, SZrExecIrInstruction *instruction) {
    instruction->opcode = eis4_get16(cursor);
    instruction->flags = eis4_get16(cursor);
    instruction->results = eis4_get_range(cursor);
    instruction->operands = eis4_get_range(cursor);
    instruction->phiRange = eis4_get_range(cursor);
    instruction->successorRange = eis4_get_range(cursor);
    instruction->typeToken = eis4_get32(cursor);
    instruction->matchTypeToken = eis4_get32(cursor);
    instruction->layoutId = eis4_get32(cursor);
    instruction->memoryIn = eis4_get_range(cursor);
    instruction->memoryOut = eis4_get_range(cursor);
    instruction->effectIn = eis4_get32(cursor);
    instruction->effectOut = eis4_get32(cursor);
    instruction->sourceId = eis4_get32(cursor);
    instruction->deoptId = eis4_get32(cursor);
    instruction->bindingRow = eis4_get32(cursor);
}

static TZrBool eis4_range_is(SZrExecIrRange range, TZrUInt32 start,
                             TZrUInt32 count) {
    return (TZrBool)(range.start == start && range.count == count);
}

static TZrBool eis4_contract_is(const SZrExecutionContract *contract,
                                TZrMetadataToken target,
                                TZrUInt64 signature, TZrUInt64 layout,
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
                    contract->signatureHash == signature &&
                    contract->layoutHash == layout && layout != 0u &&
                    contract->moduleHash == moduleHash && moduleHash != 0u &&
                    contract->requiredCapabilities == 0u &&
                    contract->declaredEffects == 0u);
}

static TZrBool eis4_block_aux_is(const SZrExecIrBlock *block) {
    if (block->id == 0u || !eis4_range_is(block->phis, 0u, 0u) ||
        block->effectPhiResult != 0u ||
        !eis4_range_is(block->effectPhiIncomings, 0u, 0u))
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++index) {
        if (block->memoryPhiResults[index] != 0u ||
            !eis4_range_is(block->memoryPhiIncomings[index], 0u, 0u))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool eis4_instruction_is(
        const SZrExecIrInstruction *instruction, EZrExecIrOpcode opcode,
        SZrExecIrRange results, SZrExecIrRange operands, TZrUInt32 layoutId) {
    return (TZrBool)(instruction->opcode == opcode && instruction->flags == 0u &&
                    eis4_range_is(instruction->results, results.start,
                                  results.count) &&
                    eis4_range_is(instruction->operands, operands.start,
                                  operands.count) &&
                    eis4_range_is(instruction->phiRange, 0u, 0u) &&
                    eis4_range_is(instruction->successorRange, 0u, 0u) &&
                    instruction->typeToken == 0u &&
                    instruction->matchTypeToken == 0u &&
                    instruction->layoutId == layoutId &&
                    eis4_range_is(instruction->memoryIn, 0u, 0u) &&
                    eis4_range_is(instruction->memoryOut, 0u, 0u) &&
                    instruction->effectIn == 0u &&
                    instruction->effectOut == 0u &&
                    instruction->deoptId == 0u &&
                    instruction->bindingRow == 0u);
}

static TZrBool eis4_shape_is(const SZrExecIrModule *module) {
    static const TZrUInt32 definitions[3] = {1u, 2u, 3u};
    const SZrExecIrFunction *function;
    const SZrExecIrBlock *block;
    const SZrExecIrInstruction *instructions;
    const SZrExecIrRange empty = {.start = 0u, .count = 0u};
    if (module == ZR_NULL || module->id != 1u || module->moduleToken == 0u ||
        module->moduleHash == 0u || module->functionCount != 1u ||
        module->functions == ZR_NULL || module->constantCount != 2u ||
        module->constants == ZR_NULL || module->layoutCount != 0u ||
        module->layouts != ZR_NULL || module->sourceMapCount != 0u ||
        module->sourceMaps != ZR_NULL)
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        if (module->constants[index].typeToken != ZR_VALUE_TYPE_INT64 ||
            module->constants[index].flags != 0u ||
            module->constants[index].bits != (index == 0u ? 20u : 22u))
            return ZR_FALSE;
    }
    function = &module->functions[0];
    if (function->id != 1u || function->functionToken == 0u ||
        function->bindingRowsSchemaVersion !=
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY ||
        function->bindingRows != ZR_NULL || function->bindingRowCount != 0u ||
        function->bindingRowCapacity != 0u ||
        function->signatureHash == 0u || function->entryBlockId != 1u ||
        function->valueCount != 3u || function->values == ZR_NULL ||
        function->instructionCount != 4u || function->instructions == ZR_NULL ||
        function->blockCount != 1u || function->blocks == ZR_NULL ||
        function->operandCount != 3u || function->operands == ZR_NULL ||
        function->resultCount != 3u || function->results == ZR_NULL ||
        function->memoryTokenCount != 0u || function->memoryTokenPool != ZR_NULL ||
        function->phiCount != 0u || function->phiPool != ZR_NULL ||
        function->phiIncomingCount != 0u || function->phiIncoming != ZR_NULL ||
        function->predecessorCount != 0u || function->predecessors != ZR_NULL ||
        function->successorCount != 0u || function->successors != ZR_NULL ||
        function->frameLayout != ZR_NULL || function->gcMap != ZR_NULL ||
        function->gcMapCount != 0u || function->gcRootCount != 0u ||
        function->gcRoots != ZR_NULL || function->deoptStateCount != 0u ||
        function->deoptStates != ZR_NULL || function->deoptValueCount != 0u ||
        function->deoptValues != ZR_NULL || function->deoptAggregateCount != 0u ||
        function->deoptAggregates != ZR_NULL ||
        function->deoptAggregateFieldCount != 0u ||
        function->deoptAggregateFields != ZR_NULL ||
        function->sourceMapCount != 0u || function->sourceMaps != ZR_NULL ||
        function->stateMap != ZR_NULL ||
        (function->sealed != ZR_FALSE && function->sealed != ZR_TRUE))
        return ZR_FALSE;
    if (!eis4_contract_is(&module->contract, module->moduleToken,
                          function->signatureHash,
                          function->contract.layoutHash, module->moduleHash) ||
        !eis4_contract_is(&function->contract, function->functionToken,
                          function->signatureHash,
                          module->contract.layoutHash, module->moduleHash))
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        if (value->id != index + 1u || value->definition != definitions[index] ||
            value->typeToken != ZR_VALUE_TYPE_INT64 ||
            value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            value->flags != 0u || function->results[index] != index + 1u ||
            function->operands[index] != index + 1u)
            return ZR_FALSE;
    }
    block = &function->blocks[0];
    if (block->id != 1u || block->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        !eis4_range_is(block->instructions, 0u, 4u) ||
        !eis4_range_is(block->predecessors, 0u, 0u) ||
        !eis4_range_is(block->successors, 0u, 0u) ||
        block->immediateDominator != 0u ||
        block->terminatorInstructionId != 4u || !eis4_block_aux_is(block))
        return ZR_FALSE;
    instructions = function->instructions;
    if (!eis4_instruction_is(&instructions[0], ZR_EXEC_IR_OPCODE_CONSTANT,
                              (SZrExecIrRange){.start = 0u, .count = 1u},
                              empty, 0u) ||
        !eis4_instruction_is(&instructions[1], ZR_EXEC_IR_OPCODE_CONSTANT,
                              (SZrExecIrRange){.start = 1u, .count = 1u},
                              empty, 1u) ||
        !eis4_instruction_is(&instructions[2], ZR_EXEC_IR_OPCODE_ADD,
                              (SZrExecIrRange){.start = 2u, .count = 1u},
                              (SZrExecIrRange){.start = 0u, .count = 2u}, 0u) ||
        !eis4_instruction_is(&instructions[3], ZR_EXEC_IR_OPCODE_RETURN, empty,
                              (SZrExecIrRange){.start = 2u, .count = 1u}, 0u))
        return ZR_FALSE;
    return ZrCore_ExecIr_VerifyModule(module, ZR_NULL);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    if (module == ZR_NULL || outSize == ZR_NULL)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (!eis4_shape_is(module))
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    *outSize = ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE;
    return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static void eis4_write_record(SEis4Cursor *cursor,
                              const SZrExecIrModule *module) {
    const SZrExecIrFunction *function = &module->functions[0];
    eis4_put32(cursor, ZR_ARTIFACT_EXEC_IR_ADD_MAGIC);
    eis4_put16(cursor, ZR_ARTIFACT_EXEC_IR_ADD_VERSION);
    eis4_put16(cursor, 0u);
    eis4_put32(cursor, ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE);
    eis4_put32(cursor, module->id);
    eis4_put32(cursor, module->moduleToken);
    eis4_put64(cursor, module->moduleHash);
    eis4_put_contract(cursor, &module->contract);
    eis4_put32(cursor, function->id);
    eis4_put32(cursor, function->functionToken);
    eis4_put64(cursor, function->signatureHash);
    eis4_put32(cursor, function->entryBlockId);
    eis4_put32(cursor, function->sealed);
    eis4_put_contract(cursor, &function->contract);
    eis4_put32(cursor, module->constantCount);
    eis4_put32(cursor, function->valueCount);
    eis4_put32(cursor, function->blockCount);
    eis4_put32(cursor, function->instructionCount);
    eis4_put32(cursor, function->resultCount);
    eis4_put32(cursor, function->operandCount);
    eis4_put32(cursor, function->successorCount);
    eis4_put32(cursor, function->predecessorCount);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        eis4_put_constant(cursor, &module->constants[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis4_put_value(cursor, &function->values[index]);
    eis4_put_block(cursor, &function->blocks[0]);
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        eis4_put_instruction(cursor, &function->instructions[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis4_put32(cursor, function->results[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis4_put32(cursor, function->operands[index]);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis4_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrByte temporary[ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE];
    SEis4Cursor cursor = {temporary, 0u};
    TZrUInt32 size;
    EZrArtifactExecIrStatus status;
    if (module == ZR_NULL || bytes == ZR_NULL)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize(
            module, &size, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (capacity < size)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    eis4_write_record(&cursor, module);
    if (cursor.offset != ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         cursor.offset);
    memcpy(bytes, temporary, size);
    return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static void eis4_read_record(SEis4Cursor *cursor, SEis4Record *record) {
    record->moduleId = eis4_get32(cursor);
    record->moduleToken = eis4_get32(cursor);
    record->moduleHash = eis4_get64(cursor);
    eis4_get_contract(cursor, &record->moduleContract);
    record->functionId = eis4_get32(cursor);
    record->functionToken = eis4_get32(cursor);
    record->signatureHash = eis4_get64(cursor);
    record->entryBlockId = eis4_get32(cursor);
    record->sealed = eis4_get32(cursor);
    eis4_get_contract(cursor, &record->functionContract);
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        record->counts[index] = eis4_get32(cursor);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        eis4_get_constant(cursor, &record->constants[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis4_get_value(cursor, &record->values[index]);
    eis4_get_block(cursor, &record->block);
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        eis4_get_instruction(cursor, &record->instructions[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        record->resultIds[index] = eis4_get32(cursor);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        record->operandIds[index] = eis4_get32(cursor);
}

static EZrArtifactExecIrStatus eis4_counts_validate(
        const TZrUInt32 counts[8],
        SZrArtifactExecIrDiagnostic *diagnostic) {
    static const TZrUInt32 expected[8] = {2u, 3u, 1u, 4u, 3u, 3u, 0u, 0u};
    for (TZrUInt32 index = 0u; index < 8u; ++index) {
        TZrUInt32 offset = EIS4_COUNTS_OFFSET + index * 4u;
        if (counts[index] > expected[index])
            return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, offset);
        if (counts[index] != expected[index])
            return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                             offset);
    }
    return ZR_ARTIFACT_EXEC_IR_OK;
}

static TZrBool eis4_record_shape_is(const SEis4Record *record) {
    static const TZrUInt32 definitions[3] = {1u, 2u, 3u};
    static const TZrUInt32 ids[3] = {1u, 2u, 3u};
    const SZrExecIrRange empty = {.start = 0u, .count = 0u};
    if (record->moduleId != 1u || record->functionId != 1u ||
        record->entryBlockId != 1u || record->sealed > 1u ||
        record->block.id != 1u ||
        record->block.flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        !eis4_range_is(record->block.instructions, 0u, 4u) ||
        !eis4_range_is(record->block.predecessors, 0u, 0u) ||
        !eis4_range_is(record->block.successors, 0u, 0u) ||
        record->block.immediateDominator != 0u ||
        record->block.terminatorInstructionId != 4u)
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        if (record->constants[index].typeToken != ZR_VALUE_TYPE_INT64 ||
            record->constants[index].flags != 0u ||
            record->constants[index].bits != (index == 0u ? 20u : 22u))
            return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        if (record->values[index].id != ids[index] ||
            record->values[index].definition != definitions[index] ||
            record->values[index].typeToken != ZR_VALUE_TYPE_INT64 ||
            record->values[index].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            record->values[index].nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            record->values[index].flags != 0u ||
            record->resultIds[index] != ids[index] ||
            record->operandIds[index] != ids[index])
            return ZR_FALSE;
    }
    return eis4_instruction_is(
                   &record->instructions[0], ZR_EXEC_IR_OPCODE_CONSTANT,
                   (SZrExecIrRange){.start = 0u, .count = 1u}, empty, 0u) &&
           eis4_instruction_is(
                   &record->instructions[1], ZR_EXEC_IR_OPCODE_CONSTANT,
                   (SZrExecIrRange){.start = 1u, .count = 1u}, empty, 1u) &&
           eis4_instruction_is(
                   &record->instructions[2], ZR_EXEC_IR_OPCODE_ADD,
                   (SZrExecIrRange){.start = 2u, .count = 1u},
                   (SZrExecIrRange){.start = 0u, .count = 2u}, 0u) &&
           eis4_instruction_is(
                   &record->instructions[3], ZR_EXEC_IR_OPCODE_RETURN, empty,
                   (SZrExecIrRange){.start = 2u, .count = 1u}, 0u);
}

static EZrArtifactExecIrStatus eis4_wire_shape_validate(
        const SEis4Record *record, TZrUInt32 cursorOffset,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    static const TZrUInt32 valueOffsets[3] = {
        EIS4_VALUES_OFFSET, EIS4_VALUES_OFFSET + 24u,
        EIS4_VALUES_OFFSET + 48u
    };
    const SZrExecIrInstruction *add = &record->instructions[2];
    if (record->moduleId != 1u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_MODULE_ID_OFFSET);
    if (record->functionId != 1u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_FUNCTION_ID_OFFSET);
    if (record->entryBlockId != 1u || record->sealed > 1u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_FUNCTION_ENTRY_BLOCK_OFFSET);
    if (record->block.id != 1u ||
        record->block.flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_OFFSET);
    if (record->block.instructions.start != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_INSTRUCTION_START_OFFSET);
    if (record->block.instructions.count != 4u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_INSTRUCTION_COUNT_OFFSET);
    if (record->block.predecessors.start != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_PREDECESSOR_START_OFFSET);
    if (record->block.predecessors.count != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_PREDECESSOR_COUNT_OFFSET);
    if (record->block.successors.start != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_SUCCESSOR_START_OFFSET);
    if (record->block.successors.count != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_SUCCESSOR_COUNT_OFFSET);
    if (record->block.immediateDominator != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_DOMINATOR_OFFSET);
    if (record->block.terminatorInstructionId != 4u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_BLOCK_TERMINATOR_OFFSET);
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        if (record->constants[index].typeToken != ZR_VALUE_TYPE_INT64 ||
            record->constants[index].flags != 0u)
            return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                             EIS4_CONSTANTS_OFFSET + index * 16u);
        if (record->constants[index].bits != (index == 0u ? 20u : 22u))
            return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                             index == 0u ? EIS4_FIRST_CONSTANT_BITS_OFFSET
                                         : EIS4_SECOND_CONSTANT_BITS_OFFSET);
    }
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        if (record->values[index].id != index + 1u ||
            record->values[index].definition != index + 1u ||
            record->values[index].typeToken != ZR_VALUE_TYPE_INT64 ||
            record->values[index].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            record->values[index].nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            record->values[index].flags != 0u)
            return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                             valueOffsets[index]);
    }
    if (record->resultIds[0] != 1u || record->resultIds[1] != 2u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_RESULTS_OFFSET);
    if (record->resultIds[2] != 3u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_THIRD_RESULT_ID_OFFSET);
    if (record->operandIds[0] != 1u || record->operandIds[1] != 2u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_OPERANDS_OFFSET);
    if (record->operandIds[2] != 3u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_OPERANDS_OFFSET + 8u);
    if (add->opcode != ZR_EXEC_IR_OPCODE_ADD)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_ADD_OPCODE_OFFSET);
    if (!eis4_range_is(add->results, 2u, 1u))
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_ADD_RESULTS_OFFSET);
    if (!eis4_range_is(add->operands, 0u, 2u))
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_ADD_OPERANDS_OFFSET);
    if (cursorOffset != ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         cursorOffset);
    if (!eis4_record_shape_is(record))
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS4_INSTRUCTIONS_OFFSET);
    return ZR_ARTIFACT_EXEC_IR_OK;
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis4_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis4Cursor cursor;
    SEis4Record record;
    SZrExecIrModule temporary;
    SZrExecIrFunction *function;
    SZrExecIrBlock *block;
    SZrExecIrRange range;
    TZrExecIrFunctionId functionId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId valueId;
    TZrUInt16 version, reserved;
    TZrUInt32 declaredLength;
    EZrArtifactExecIrStatus status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
    if (bytes == ZR_NULL || outModule == ZR_NULL ||
        outModule->functionCount != 0u || outModule->functions != ZR_NULL ||
        outModule->constantCount != 0u || outModule->constants != ZR_NULL ||
        outModule->layoutCount != 0u || outModule->layouts != ZR_NULL ||
        outModule->sourceMapCount != 0u || outModule->sourceMaps != ZR_NULL)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (length != ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    cursor.bytes = (TZrByte *)bytes;
    cursor.offset = 0u;
    if (eis4_get32(&cursor) != ZR_ARTIFACT_EXEC_IR_ADD_MAGIC)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_BAD_MAGIC, 0u);
    version = eis4_get16(&cursor);
    reserved = eis4_get16(&cursor);
    declaredLength = eis4_get32(&cursor);
    if (version != ZR_ARTIFACT_EXEC_IR_ADD_VERSION)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
    if (reserved != 0u)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 6u);
    if (declaredLength != ZR_ARTIFACT_EXEC_IR_ADD_ENCODED_SIZE)
        return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 8u);
    memset(&record, 0, sizeof(record));
    eis4_read_record(&cursor, &record);
    status = eis4_counts_validate(record.counts, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    status = eis4_wire_shape_validate(&record, cursor.offset, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;

    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = record.moduleId;
    temporary.moduleToken = record.moduleToken;
    temporary.moduleHash = record.moduleHash;
    temporary.contract = record.moduleContract;
    if (!ZrCore_ExecIr_ModuleAppendConstant(&temporary, record.constants, 2u,
                                            &range) || range.start != 0u ||
        range.count != 2u ||
        !ZrCore_ExecIr_ModuleAddFunction(&temporary, record.functionToken,
                                         record.signatureHash, &functionId) ||
        functionId != 1u)
        goto reject;
    function = ZrCore_ExecIr_ModuleFunctionAt(&temporary, functionId);
    if (function == ZR_NULL) goto reject;
    function->contract = record.functionContract;
    if (ZrCore_ExecIr_FunctionAddBlock(
                function, record.block.flags) != 1u)
        goto reject;
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        valueId = ZrCore_ExecIr_FunctionAddValue(
                function, record.values[index].typeToken,
                record.values[index].ownership,
                record.values[index].nullability);
        if (valueId != index + 1u) goto reject;
    }
    if (!ZrCore_ExecIr_FunctionAppendResults(
                function, record.resultIds, 3u, &range) ||
        range.start != 0u || range.count != 3u ||
        !ZrCore_ExecIr_FunctionAppendOperands(
                function, record.operandIds, 3u, &range) ||
        range.start != 0u || range.count != 3u)
        goto reject;
    for (TZrUInt32 index = 0u; index < 4u; ++index) {
        if (!ZrCore_ExecIr_FunctionAppendInstruction(
                    function, &record.instructions[index], &instructionId) ||
            instructionId != index + 1u)
            goto reject;
    }
    block = ZrCore_ExecIr_FunctionBlockAt(function, 1u);
    if (block == ZR_NULL) goto reject;
    block->instructions = record.block.instructions;
    block->predecessors = record.block.predecessors;
    block->successors = record.block.successors;
    block->immediateDominator = record.block.immediateDominator;
    block->terminatorInstructionId = record.block.terminatorInstructionId;
    function->sealed = (TZrBool)record.sealed;
    status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
    if (!eis4_shape_is(&temporary)) goto reject;
    *outModule = temporary;
    return eis4_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
reject:
    ZrCore_ExecIr_FreeModule(&temporary);
    return eis4_fail(diagnostic, status == ZR_ARTIFACT_EXEC_IR_OK
                                     ? ZR_ARTIFACT_EXEC_IR_INVALID_SECTION
                                     : status, 0u);
}
