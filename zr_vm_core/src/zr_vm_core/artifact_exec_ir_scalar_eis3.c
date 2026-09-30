#include "artifact_exec_ir_scalar_eis3.h"
#include "zr_vm_common/zr_type_conf.h"

#include <string.h>

typedef struct SEis3Cursor {
    TZrByte *bytes;
    TZrUInt32 offset;
} SEis3Cursor;

typedef struct SEis3Record {
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
    SZrExecIrConstant constants[3];
    SZrExecIrValue values[3];
    SZrExecIrBlock blocks[3];
    SZrExecIrInstruction instructions[6];
    TZrUInt32 resultIds[3];
    TZrUInt32 operandIds[3];
    TZrUInt32 successorIds[2];
    TZrUInt32 predecessorIds[2];
} SEis3Record;

#define EIS3_MODULE_ID_OFFSET 12u
#define EIS3_COUNT_OFFSET 180u
#define EIS3_BLOCK_COUNT_OFFSET 188u
#define EIS3_ENTRY_SUCCESSOR_RANGE_OFFSET 356u
#define EIS3_ENTRY_SUCCESSOR_COUNT_OFFSET 360u
#define EIS3_FIRST_SUCCESSOR_ID_OFFSET 980u
#define EIS3_FIRST_PREDECESSOR_ID_OFFSET 988u

static EZrArtifactExecIrStatus eis3_fail(
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

static void eis3_put16(SEis3Cursor *cursor, TZrUInt16 value) {
    cursor->bytes[cursor->offset++] = (TZrByte)value;
    cursor->bytes[cursor->offset++] = (TZrByte)(value >> 8u);
}

static void eis3_put32(SEis3Cursor *cursor, TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

static void eis3_put64(SEis3Cursor *cursor, TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

static TZrUInt16 eis3_get16(SEis3Cursor *cursor) {
    const TZrByte *bytes = cursor->bytes + cursor->offset;
    cursor->offset += 2u;
    return (TZrUInt16)((TZrUInt16)bytes[0] | ((TZrUInt16)bytes[1] << 8u));
}

static TZrUInt32 eis3_get32(SEis3Cursor *cursor) {
    TZrUInt32 value = 0u;
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        value |= (TZrUInt32)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

static TZrUInt64 eis3_get64(SEis3Cursor *cursor) {
    TZrUInt64 value = 0u;
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        value |= (TZrUInt64)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

static void eis3_put_range(SEis3Cursor *cursor, SZrExecIrRange range) {
    eis3_put32(cursor, range.start);
    eis3_put32(cursor, range.count);
}

static SZrExecIrRange eis3_get_range(SEis3Cursor *cursor) {
    SZrExecIrRange range;
    range.start = eis3_get32(cursor);
    range.count = eis3_get32(cursor);
    return range;
}

static void eis3_put_contract(SEis3Cursor *cursor,
                              const SZrExecutionContract *contract) {
    eis3_put32(cursor, contract->schemaVersion);
    eis3_put32(cursor, contract->abiVersion);
    eis3_put32(cursor, contract->logicalVersion);
    eis3_put32(cursor, contract->reserved0);
    eis3_put64(cursor, contract->generation);
    eis3_put32(cursor, contract->targetToken);
    eis3_put32(cursor, contract->reserved1);
    eis3_put64(cursor, contract->signatureHash);
    eis3_put64(cursor, contract->layoutHash);
    eis3_put64(cursor, contract->moduleHash);
    eis3_put32(cursor, contract->requiredCapabilities);
    eis3_put32(cursor, contract->declaredEffects);
}

static void eis3_get_contract(SEis3Cursor *cursor,
                              SZrExecutionContract *contract) {
    contract->schemaVersion = eis3_get32(cursor);
    contract->abiVersion = eis3_get32(cursor);
    contract->logicalVersion = eis3_get32(cursor);
    contract->reserved0 = eis3_get32(cursor);
    contract->generation = eis3_get64(cursor);
    contract->targetToken = eis3_get32(cursor);
    contract->reserved1 = eis3_get32(cursor);
    contract->signatureHash = eis3_get64(cursor);
    contract->layoutHash = eis3_get64(cursor);
    contract->moduleHash = eis3_get64(cursor);
    contract->requiredCapabilities = eis3_get32(cursor);
    contract->declaredEffects = eis3_get32(cursor);
}

static void eis3_put_constant(SEis3Cursor *cursor,
                              const SZrExecIrConstant *constant) {
    eis3_put32(cursor, constant->typeToken);
    eis3_put32(cursor, constant->flags);
    eis3_put64(cursor, constant->bits);
}

static void eis3_get_constant(SEis3Cursor *cursor,
                              SZrExecIrConstant *constant) {
    constant->typeToken = eis3_get32(cursor);
    constant->flags = eis3_get32(cursor);
    constant->bits = eis3_get64(cursor);
}

static void eis3_put_value(SEis3Cursor *cursor,
                           const SZrExecIrValue *value) {
    eis3_put32(cursor, value->id);
    eis3_put32(cursor, value->definition);
    eis3_put32(cursor, value->typeToken);
    eis3_put32(cursor, value->ownership);
    eis3_put32(cursor, value->nullability);
    eis3_put32(cursor, value->flags);
}

static void eis3_get_value(SEis3Cursor *cursor, SZrExecIrValue *value) {
    value->id = eis3_get32(cursor);
    value->definition = eis3_get32(cursor);
    value->typeToken = eis3_get32(cursor);
    value->ownership = (EZrExecIrOwnership)eis3_get32(cursor);
    value->nullability = (EZrExecIrNullability)eis3_get32(cursor);
    value->flags = eis3_get32(cursor);
}

static void eis3_put_block(SEis3Cursor *cursor,
                           const SZrExecIrBlock *block) {
    eis3_put32(cursor, block->id);
    eis3_put32(cursor, block->flags);
    eis3_put_range(cursor, block->instructions);
    eis3_put_range(cursor, block->predecessors);
    eis3_put_range(cursor, block->successors);
    eis3_put32(cursor, block->immediateDominator);
    eis3_put32(cursor, block->terminatorInstructionId);
}

static void eis3_get_block(SEis3Cursor *cursor, SZrExecIrBlock *block) {
    block->id = eis3_get32(cursor);
    block->flags = eis3_get32(cursor);
    block->instructions = eis3_get_range(cursor);
    block->predecessors = eis3_get_range(cursor);
    block->successors = eis3_get_range(cursor);
    block->immediateDominator = eis3_get32(cursor);
    block->terminatorInstructionId = eis3_get32(cursor);
}

static void eis3_put_instruction(
        SEis3Cursor *cursor, const SZrExecIrInstruction *instruction) {
    eis3_put16(cursor, instruction->opcode);
    eis3_put16(cursor, instruction->flags);
    eis3_put_range(cursor, instruction->results);
    eis3_put_range(cursor, instruction->operands);
    eis3_put_range(cursor, instruction->phiRange);
    eis3_put_range(cursor, instruction->successorRange);
    eis3_put32(cursor, instruction->typeToken);
    eis3_put32(cursor, instruction->matchTypeToken);
    eis3_put32(cursor, instruction->layoutId);
    eis3_put_range(cursor, instruction->memoryIn);
    eis3_put_range(cursor, instruction->memoryOut);
    eis3_put32(cursor, instruction->effectIn);
    eis3_put32(cursor, instruction->effectOut);
    eis3_put32(cursor, instruction->sourceId);
    eis3_put32(cursor, instruction->deoptId);
    eis3_put32(cursor, instruction->bindingRow);
}

static void eis3_get_instruction(
        SEis3Cursor *cursor, SZrExecIrInstruction *instruction) {
    instruction->opcode = eis3_get16(cursor);
    instruction->flags = eis3_get16(cursor);
    instruction->results = eis3_get_range(cursor);
    instruction->operands = eis3_get_range(cursor);
    instruction->phiRange = eis3_get_range(cursor);
    instruction->successorRange = eis3_get_range(cursor);
    instruction->typeToken = eis3_get32(cursor);
    instruction->matchTypeToken = eis3_get32(cursor);
    instruction->layoutId = eis3_get32(cursor);
    instruction->memoryIn = eis3_get_range(cursor);
    instruction->memoryOut = eis3_get_range(cursor);
    instruction->effectIn = eis3_get32(cursor);
    instruction->effectOut = eis3_get32(cursor);
    instruction->sourceId = eis3_get32(cursor);
    instruction->deoptId = eis3_get32(cursor);
    instruction->bindingRow = eis3_get32(cursor);
}

static TZrBool eis3_range_is(SZrExecIrRange range, TZrUInt32 start,
                             TZrUInt32 count) {
    return (TZrBool)(range.start == start && range.count == count);
}

static TZrBool eis3_contract_is(const SZrExecutionContract *contract,
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

static TZrBool eis3_block_aux_is(const SZrExecIrBlock *block) {
    if (!eis3_range_is(block->phis, 0u, 0u) ||
        block->effectPhiResult != 0u ||
        !eis3_range_is(block->effectPhiIncomings, 0u, 0u) || block->id == 0u)
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++index) {
        if (block->memoryPhiResults[index] != 0u ||
            !eis3_range_is(block->memoryPhiIncomings[index], 0u, 0u))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool eis3_instruction_is(
        const SZrExecIrInstruction *instruction, EZrExecIrOpcode opcode,
        SZrExecIrRange results, SZrExecIrRange operands,
        SZrExecIrRange successors, TZrUInt32 layoutId) {
    return (TZrBool)(instruction->opcode == opcode && instruction->flags == 0u &&
                    eis3_range_is(instruction->results, results.start,
                                  results.count) &&
                    eis3_range_is(instruction->operands, operands.start,
                                  operands.count) &&
                    eis3_range_is(instruction->phiRange, 0u, 0u) &&
                    eis3_range_is(instruction->successorRange, successors.start,
                                  successors.count) &&
                    instruction->typeToken == 0u &&
                    instruction->matchTypeToken == 0u &&
                    instruction->layoutId == layoutId &&
                    eis3_range_is(instruction->memoryIn, 0u, 0u) &&
                    eis3_range_is(instruction->memoryOut, 0u, 0u) &&
                    instruction->effectIn == 0u &&
                    instruction->effectOut == 0u &&
                    instruction->deoptId == 0u &&
                    instruction->bindingRow == 0u);
}

static TZrBool eis3_shape_is(const SZrExecIrModule *module) {
    static const TZrUInt32 valueDefinitions[3] = {1u, 3u, 5u};
    const SZrExecIrFunction *function;
    const SZrExecIrBlock *blocks;
    const SZrExecIrInstruction *instructions;
    const SZrExecIrRange empty = {.start = 0u, .count = 0u};
    if (module == ZR_NULL || module->id != 1u || module->moduleToken == 0u ||
        module->moduleHash == 0u || module->functionCount != 1u ||
        module->functions == ZR_NULL || module->constantCount != 3u ||
        module->constants == ZR_NULL || module->layoutCount != 0u ||
        module->layouts != ZR_NULL || module->sourceMapCount != 0u ||
        module->sourceMaps != ZR_NULL)
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        if (module->constants[index].typeToken != ZR_VALUE_TYPE_INT64 ||
            module->constants[index].flags != 0u)
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
        function->instructionCount != 6u || function->instructions == ZR_NULL ||
        function->blockCount != 3u || function->blocks == ZR_NULL ||
        function->operandCount != 3u || function->operands == ZR_NULL ||
        function->resultCount != 3u || function->results == ZR_NULL ||
        function->memoryTokenCount != 0u || function->memoryTokenPool != ZR_NULL ||
        function->phiCount != 0u || function->phiPool != ZR_NULL ||
        function->phiIncomingCount != 0u || function->phiIncoming != ZR_NULL ||
        function->predecessorCount != 2u || function->predecessors == ZR_NULL ||
        function->successorCount != 2u || function->successors == ZR_NULL ||
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
    if (!eis3_contract_is(&module->contract, module->moduleToken,
                          function->signatureHash,
                          function->contract.layoutHash, module->moduleHash) ||
        !eis3_contract_is(&function->contract, function->functionToken,
                          function->signatureHash,
                          module->contract.layoutHash, module->moduleHash))
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        if (value->id != index + 1u ||
            value->definition != valueDefinitions[index] ||
            value->typeToken != ZR_VALUE_TYPE_INT64 ||
            value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            value->flags != 0u || function->results[index] != index + 1u ||
            function->operands[index] != index + 1u)
            return ZR_FALSE;
    }
    if (function->successors[0] != 2u || function->successors[1] != 3u ||
        function->predecessors[0] != 1u || function->predecessors[1] != 1u)
        return ZR_FALSE;
    blocks = function->blocks;
    if (blocks[0].id != 1u || blocks[0].flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        !eis3_range_is(blocks[0].instructions, 0u, 2u) ||
        !eis3_range_is(blocks[0].predecessors, 0u, 0u) ||
        !eis3_range_is(blocks[0].successors, 0u, 2u) ||
        blocks[0].immediateDominator != 0u ||
        blocks[0].terminatorInstructionId != 2u ||
        !eis3_block_aux_is(&blocks[0]) ||
        blocks[1].id != 2u || blocks[1].flags != 0u ||
        !eis3_range_is(blocks[1].instructions, 2u, 2u) ||
        !eis3_range_is(blocks[1].predecessors, 0u, 1u) ||
        !eis3_range_is(blocks[1].successors, 0u, 0u) ||
        blocks[1].immediateDominator != 1u ||
        blocks[1].terminatorInstructionId != 4u ||
        !eis3_block_aux_is(&blocks[1]) ||
        blocks[2].id != 3u || blocks[2].flags != 0u ||
        !eis3_range_is(blocks[2].instructions, 4u, 2u) ||
        !eis3_range_is(blocks[2].predecessors, 1u, 1u) ||
        !eis3_range_is(blocks[2].successors, 0u, 0u) ||
        blocks[2].immediateDominator != 1u ||
        blocks[2].terminatorInstructionId != 6u ||
        !eis3_block_aux_is(&blocks[2]))
        return ZR_FALSE;
    instructions = function->instructions;
    if (!eis3_instruction_is(&instructions[0], ZR_EXEC_IR_OPCODE_CONSTANT,
                              (SZrExecIrRange){.start = 0u, .count = 1u},
                              empty, empty, 0u) ||
        !eis3_instruction_is(&instructions[1],
                              ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH, empty,
                              (SZrExecIrRange){.start = 0u, .count = 1u},
                              (SZrExecIrRange){.start = 0u, .count = 2u}, 0u) ||
        !eis3_instruction_is(&instructions[2], ZR_EXEC_IR_OPCODE_CONSTANT,
                              (SZrExecIrRange){.start = 1u, .count = 1u},
                              empty, empty, 1u) ||
        !eis3_instruction_is(&instructions[3], ZR_EXEC_IR_OPCODE_RETURN, empty,
                              (SZrExecIrRange){.start = 1u, .count = 1u},
                              empty, 0u) ||
        !eis3_instruction_is(&instructions[4], ZR_EXEC_IR_OPCODE_CONSTANT,
                              (SZrExecIrRange){.start = 2u, .count = 1u},
                              empty, empty, 2u) ||
        !eis3_instruction_is(&instructions[5], ZR_EXEC_IR_OPCODE_RETURN, empty,
                              (SZrExecIrRange){.start = 2u, .count = 1u},
                              empty, 0u))
        return ZR_FALSE;
    return ZrCore_ExecIr_VerifyModule(module, ZR_NULL);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    if (module == ZR_NULL || outSize == ZR_NULL)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (!eis3_shape_is(module))
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    *outSize = ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE;
    return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static void eis3_write_record(SEis3Cursor *cursor,
                              const SZrExecIrModule *module) {
    const SZrExecIrFunction *function = &module->functions[0];
    eis3_put32(cursor, ZR_ARTIFACT_EXEC_IR_CFG_MAGIC);
    eis3_put16(cursor, ZR_ARTIFACT_EXEC_IR_CFG_VERSION);
    eis3_put16(cursor, 0u);
    eis3_put32(cursor, ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE);
    eis3_put32(cursor, module->id);
    eis3_put32(cursor, module->moduleToken);
    eis3_put64(cursor, module->moduleHash);
    eis3_put_contract(cursor, &module->contract);
    eis3_put32(cursor, function->id);
    eis3_put32(cursor, function->functionToken);
    eis3_put64(cursor, function->signatureHash);
    eis3_put32(cursor, function->entryBlockId);
    eis3_put32(cursor, function->sealed);
    eis3_put_contract(cursor, &function->contract);
    eis3_put32(cursor, module->constantCount);
    eis3_put32(cursor, function->valueCount);
    eis3_put32(cursor, function->blockCount);
    eis3_put32(cursor, function->instructionCount);
    eis3_put32(cursor, function->resultCount);
    eis3_put32(cursor, function->operandCount);
    eis3_put32(cursor, function->successorCount);
    eis3_put32(cursor, function->predecessorCount);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_put_constant(cursor, &module->constants[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_put_value(cursor, &function->values[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_put_block(cursor, &function->blocks[index]);
    for (TZrUInt32 index = 0u; index < 6u; ++index)
        eis3_put_instruction(cursor, &function->instructions[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_put32(cursor, function->results[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_put32(cursor, function->operands[index]);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        eis3_put32(cursor, function->successors[index]);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        eis3_put32(cursor, function->predecessors[index]);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis3_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrByte temporary[ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE];
    SEis3Cursor cursor = {temporary, 0u};
    TZrUInt32 size;
    EZrArtifactExecIrStatus status;
    if (module == ZR_NULL || bytes == ZR_NULL)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize(
            module, &size, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (capacity < size)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    eis3_write_record(&cursor, module);
    if (cursor.offset != ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         cursor.offset);
    memcpy(bytes, temporary, cursor.offset);
    return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static void eis3_read_record(SEis3Cursor *cursor, SEis3Record *record) {
    record->moduleId = eis3_get32(cursor);
    record->moduleToken = eis3_get32(cursor);
    record->moduleHash = eis3_get64(cursor);
    eis3_get_contract(cursor, &record->moduleContract);
    record->functionId = eis3_get32(cursor);
    record->functionToken = eis3_get32(cursor);
    record->signatureHash = eis3_get64(cursor);
    record->entryBlockId = eis3_get32(cursor);
    record->sealed = eis3_get32(cursor);
    eis3_get_contract(cursor, &record->functionContract);
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        record->counts[index] = eis3_get32(cursor);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_get_constant(cursor, &record->constants[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_get_value(cursor, &record->values[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        eis3_get_block(cursor, &record->blocks[index]);
    for (TZrUInt32 index = 0u; index < 6u; ++index)
        eis3_get_instruction(cursor, &record->instructions[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        record->resultIds[index] = eis3_get32(cursor);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        record->operandIds[index] = eis3_get32(cursor);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        record->successorIds[index] = eis3_get32(cursor);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        record->predecessorIds[index] = eis3_get32(cursor);
}

static EZrArtifactExecIrStatus eis3_counts_validate(
        const TZrUInt32 counts[8], SZrArtifactExecIrDiagnostic *diagnostic) {
    static const TZrUInt32 expected[8] = {3u, 3u, 3u, 6u, 3u, 3u, 2u, 2u};
    for (TZrUInt32 index = 0u; index < 8u; ++index) {
        TZrUInt32 offset = EIS3_COUNT_OFFSET + index * 4u;
        if (counts[index] > expected[index])
            return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, offset);
        if (counts[index] != expected[index])
            return eis3_fail(diagnostic,
                             ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, offset);
    }
    return ZR_ARTIFACT_EXEC_IR_OK;
}

static EZrArtifactExecIrStatus eis3_wire_shape_validate(
        const SEis3Record *record, SEis3Cursor *cursor,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    if (record->moduleId != 1u || record->functionId != 1u ||
        record->entryBlockId != 1u || record->sealed > 1u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_MODULE_ID_OFFSET);
    if (record->blocks[0].successors.start != 0u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_ENTRY_SUCCESSOR_RANGE_OFFSET);
    if (record->blocks[0].successors.count != 2u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_ENTRY_SUCCESSOR_COUNT_OFFSET);
    if (record->successorIds[0] != 2u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_FIRST_SUCCESSOR_ID_OFFSET);
    if (record->successorIds[1] != 3u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_FIRST_SUCCESSOR_ID_OFFSET + 4u);
    if (record->predecessorIds[0] != 1u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_FIRST_PREDECESSOR_ID_OFFSET);
    if (record->predecessorIds[1] != 1u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         EIS3_FIRST_PREDECESSOR_ID_OFFSET + 4u);
    if (cursor->offset != ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                         cursor->offset);
    return ZR_ARTIFACT_EXEC_IR_OK;
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis3_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis3Cursor cursor = {(TZrByte *)bytes, 0u};
    SEis3Record record;
    SZrExecIrModule temporary;
    SZrExecIrFunction *function;
    SZrExecIrRange range;
    TZrExecIrFunctionId functionId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId valueId;
    TZrUInt16 version, reserved;
    TZrUInt32 declaredLength;
    EZrArtifactExecIrStatus status = ZR_ARTIFACT_EXEC_IR_LIMIT;
    if (bytes == ZR_NULL || outModule == ZR_NULL)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (length != ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    if (eis3_get32(&cursor) != ZR_ARTIFACT_EXEC_IR_CFG_MAGIC)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_BAD_MAGIC, 0u);
    version = eis3_get16(&cursor);
    reserved = eis3_get16(&cursor);
    declaredLength = eis3_get32(&cursor);
    if (version != ZR_ARTIFACT_EXEC_IR_CFG_VERSION)
        return eis3_fail(diagnostic,
                         ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
    if (reserved != 0u)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 6u);
    if (declaredLength != ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE)
        return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 8u);
    memset(&record, 0, sizeof(record));
    eis3_read_record(&cursor, &record);
    status = eis3_counts_validate(record.counts, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    status = eis3_wire_shape_validate(&record, &cursor, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;

    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = record.moduleId;
    temporary.moduleToken = record.moduleToken;
    temporary.moduleHash = record.moduleHash;
    temporary.contract = record.moduleContract;
    if (!ZrCore_ExecIr_ModuleAppendConstant(&temporary, record.constants, 3u,
                                            &range) || range.start != 0u ||
        range.count != 3u ||
        !ZrCore_ExecIr_ModuleAddFunction(&temporary, record.functionToken,
                                         record.signatureHash, &functionId) ||
        functionId != 1u)
        goto reject;
    function = ZrCore_ExecIr_ModuleFunctionAt(&temporary, functionId);
    if (function == ZR_NULL) goto reject;
    function->contract = record.functionContract;
    if (ZrCore_ExecIr_FunctionAddBlock(
                function, record.blocks[0].flags) != 1u ||
        ZrCore_ExecIr_FunctionAddBlock(
                function, record.blocks[1].flags) != 2u ||
        ZrCore_ExecIr_FunctionAddBlock(
                function, record.blocks[2].flags) != 3u)
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
        range.start != 0u || range.count != 3u ||
        !ZrCore_ExecIr_FunctionAppendSuccessors(
                function, record.successorIds, 2u, &range) ||
        range.start != 0u || range.count != 2u ||
        !ZrCore_ExecIr_FunctionAppendPredecessors(
                function, record.predecessorIds, 2u, &range) ||
        range.start != 0u || range.count != 2u)
        goto reject;
    for (TZrUInt32 index = 0u; index < 6u; ++index) {
        if (!ZrCore_ExecIr_FunctionAppendInstruction(
                    function, &record.instructions[index], &instructionId) ||
            instructionId != index + 1u)
            goto reject;
    }
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        SZrExecIrBlock *block = ZrCore_ExecIr_FunctionBlockAt(
                function, index + 1u);
        if (block == ZR_NULL) goto reject;
        block->instructions = record.blocks[index].instructions;
        block->predecessors = record.blocks[index].predecessors;
        block->successors = record.blocks[index].successors;
        block->immediateDominator = record.blocks[index].immediateDominator;
        block->terminatorInstructionId =
                record.blocks[index].terminatorInstructionId;
    }
    function->sealed = (TZrBool)record.sealed;
    status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
    if (!eis3_shape_is(&temporary)) goto reject;
    *outModule = temporary;
    return eis3_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
reject:
    ZrCore_ExecIr_FreeModule(&temporary);
    return eis3_fail(diagnostic, status, 0u);
}
