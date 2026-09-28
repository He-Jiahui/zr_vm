#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_common/zr_type_conf.h"

#include <string.h>

typedef struct SScalarCursor {
    TZrByte *bytes;
    TZrUInt32 offset;
} SScalarCursor;

typedef struct SScalarRecord {
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
    SZrExecIrConstant constant;
    SZrExecIrValue value;
    TZrUInt32 blockId;
    TZrUInt32 blockFlags;
    SZrExecIrRange blockInstructions;
    TZrUInt32 terminatorId;
    SZrExecIrInstruction instructions[2];
    TZrUInt32 resultId;
    TZrUInt32 operandId;
} SScalarRecord;

typedef struct SBranchRecord {
    SScalarRecord common;
    SZrExecIrBlock blocks[2];
    SZrExecIrInstruction instructions[3];
    TZrUInt32 successorId;
    TZrUInt32 predecessorId;
} SBranchRecord;

static EZrArtifactExecIrStatus scalar_fail(
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

static void scalar_put16(SScalarCursor *cursor, TZrUInt16 value) {
    cursor->bytes[cursor->offset++] = (TZrByte)value;
    cursor->bytes[cursor->offset++] = (TZrByte)(value >> 8u);
}

static void scalar_put32(SScalarCursor *cursor, TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

static void scalar_put64(SScalarCursor *cursor, TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        cursor->bytes[cursor->offset++] = (TZrByte)(value >> (8u * index));
}

static TZrUInt16 scalar_get16(SScalarCursor *cursor) {
    const TZrByte *bytes = cursor->bytes + cursor->offset;
    cursor->offset += 2u;
    return (TZrUInt16)((TZrUInt16)bytes[0] | ((TZrUInt16)bytes[1] << 8u));
}

static TZrUInt32 scalar_get32(SScalarCursor *cursor) {
    TZrUInt32 value = 0u;
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        value |= (TZrUInt32)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

static TZrUInt64 scalar_get64(SScalarCursor *cursor) {
    TZrUInt64 value = 0u;
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        value |= (TZrUInt64)cursor->bytes[cursor->offset++] << (8u * index);
    return value;
}

static void scalar_put_range(SScalarCursor *cursor, SZrExecIrRange range) {
    scalar_put32(cursor, range.start);
    scalar_put32(cursor, range.count);
}

static SZrExecIrRange scalar_get_range(SScalarCursor *cursor) {
    SZrExecIrRange range;
    range.start = scalar_get32(cursor);
    range.count = scalar_get32(cursor);
    return range;
}

static void scalar_put_contract(SScalarCursor *cursor,
                                const SZrExecutionContract *contract) {
    scalar_put32(cursor, contract->schemaVersion);
    scalar_put32(cursor, contract->abiVersion);
    scalar_put32(cursor, contract->logicalVersion);
    scalar_put32(cursor, contract->reserved0);
    scalar_put64(cursor, contract->generation);
    scalar_put32(cursor, contract->targetToken);
    scalar_put32(cursor, contract->reserved1);
    scalar_put64(cursor, contract->signatureHash);
    scalar_put64(cursor, contract->layoutHash);
    scalar_put64(cursor, contract->moduleHash);
    scalar_put32(cursor, contract->requiredCapabilities);
    scalar_put32(cursor, contract->declaredEffects);
}

static void scalar_get_contract(SScalarCursor *cursor,
                                SZrExecutionContract *contract) {
    contract->schemaVersion = scalar_get32(cursor);
    contract->abiVersion = scalar_get32(cursor);
    contract->logicalVersion = scalar_get32(cursor);
    contract->reserved0 = scalar_get32(cursor);
    contract->generation = scalar_get64(cursor);
    contract->targetToken = scalar_get32(cursor);
    contract->reserved1 = scalar_get32(cursor);
    contract->signatureHash = scalar_get64(cursor);
    contract->layoutHash = scalar_get64(cursor);
    contract->moduleHash = scalar_get64(cursor);
    contract->requiredCapabilities = scalar_get32(cursor);
    contract->declaredEffects = scalar_get32(cursor);
}

static void scalar_put_instruction(SScalarCursor *cursor,
                                   const SZrExecIrInstruction *instruction) {
    scalar_put16(cursor, instruction->opcode);
    scalar_put16(cursor, instruction->flags);
    scalar_put_range(cursor, instruction->results);
    scalar_put_range(cursor, instruction->operands);
    scalar_put_range(cursor, instruction->phiRange);
    scalar_put_range(cursor, instruction->successorRange);
    scalar_put32(cursor, instruction->typeToken);
    scalar_put32(cursor, instruction->matchTypeToken);
    scalar_put32(cursor, instruction->layoutId);
    scalar_put_range(cursor, instruction->memoryIn);
    scalar_put_range(cursor, instruction->memoryOut);
    scalar_put32(cursor, instruction->effectIn);
    scalar_put32(cursor, instruction->effectOut);
    scalar_put32(cursor, instruction->sourceId);
    scalar_put32(cursor, instruction->deoptId);
    scalar_put32(cursor, instruction->bindingRow);
}

static void scalar_get_instruction(SScalarCursor *cursor,
                                   SZrExecIrInstruction *instruction) {
    instruction->opcode = scalar_get16(cursor);
    instruction->flags = scalar_get16(cursor);
    instruction->results = scalar_get_range(cursor);
    instruction->operands = scalar_get_range(cursor);
    instruction->phiRange = scalar_get_range(cursor);
    instruction->successorRange = scalar_get_range(cursor);
    instruction->typeToken = scalar_get32(cursor);
    instruction->matchTypeToken = scalar_get32(cursor);
    instruction->layoutId = scalar_get32(cursor);
    instruction->memoryIn = scalar_get_range(cursor);
    instruction->memoryOut = scalar_get_range(cursor);
    instruction->effectIn = scalar_get32(cursor);
    instruction->effectOut = scalar_get32(cursor);
    instruction->sourceId = scalar_get32(cursor);
    instruction->deoptId = scalar_get32(cursor);
    instruction->bindingRow = scalar_get32(cursor);
}

static TZrBool scalar_range_is(SZrExecIrRange range,
                              TZrUInt32 start, TZrUInt32 count) {
    return (TZrBool)(range.start == start && range.count == count);
}

static TZrBool scalar_contract_is(const SZrExecutionContract *contract,
                                  TZrMetadataToken target, TZrUInt64 signature,
                                  TZrUInt64 layout, TZrUInt64 moduleHash) {
    return (TZrBool)(contract->schemaVersion == ZR_EXECUTION_CONTRACT_SCHEMA_VERSION &&
                    contract->abiVersion == ZR_EXECUTION_CONTRACT_ABI_VERSION &&
                    contract->logicalVersion == ZR_EXECUTION_CONTRACT_LOGICAL_VERSION &&
                    contract->reserved0 == 0u && contract->reserved1 == 0u &&
                    contract->generation == 1u &&
                    contract->targetToken == target &&
                    contract->signatureHash == signature &&
                    contract->layoutHash == layout && layout != 0u &&
                    contract->moduleHash == moduleHash && moduleHash != 0u &&
                    contract->requiredCapabilities == 0u &&
                    contract->declaredEffects == 0u);
}

static TZrBool scalar_instruction_is(const SZrExecIrInstruction *instruction,
                                     EZrExecIrOpcode opcode,
                                     TZrUInt32 resultCount,
                                     TZrUInt32 operandCount,
                                     TZrUInt32 successorCount) {
    return (TZrBool)(instruction->opcode == opcode &&
                    instruction->flags == 0u &&
                    scalar_range_is(instruction->results, 0u, resultCount) &&
                    scalar_range_is(instruction->operands, 0u, operandCount) &&
                    scalar_range_is(instruction->phiRange, 0u, 0u) &&
                    scalar_range_is(instruction->successorRange, 0u,
                                    successorCount) &&
                    instruction->typeToken == 0u &&
                    instruction->matchTypeToken == 0u &&
                    instruction->layoutId == 0u &&
                    scalar_range_is(instruction->memoryIn, 0u, 0u) &&
                    scalar_range_is(instruction->memoryOut, 0u, 0u) &&
                    instruction->effectIn == 0u &&
                    instruction->effectOut == 0u &&
                    instruction->deoptId == 0u &&
                    instruction->bindingRow == 0u);
}

static TZrBool scalar_block_aux_is(const SZrExecIrBlock *block) {
    if (!scalar_range_is(block->phis, 0u, 0u) ||
        block->effectPhiResult != 0u ||
        !scalar_range_is(block->effectPhiIncomings, 0u, 0u) ||
        block->id == 0u) return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++index) {
        if (block->memoryPhiResults[index] != 0u ||
            !scalar_range_is(block->memoryPhiIncomings[index], 0u, 0u))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool scalar_common_shape_is(const SZrExecIrModule *module,
                                      TZrBool branched) {
    const SZrExecIrFunction *function;
    const SZrExecIrValue *value;
    if (module == ZR_NULL || module->id != 1u || module->moduleToken == 0u ||
        module->functionCount != 1u || module->functions == ZR_NULL ||
        module->constantCount != 1u || module->constants == ZR_NULL ||
        module->layoutCount != 0u || module->layouts != ZR_NULL ||
        module->sourceMapCount != 0u || module->sourceMaps != ZR_NULL ||
        module->constants[0].typeToken != ZR_VALUE_TYPE_INT64 ||
        module->constants[0].flags != 0u) return ZR_FALSE;
    function = &module->functions[0];
    if (function->id != 1u || function->functionToken == 0u ||
        function->signatureHash == 0u || function->entryBlockId != 1u ||
        function->valueCount != 1u || function->values == ZR_NULL ||
        function->instructionCount != (branched ? 3u : 2u) ||
        function->instructions == ZR_NULL ||
        function->blockCount != (branched ? 2u : 1u) ||
        function->blocks == ZR_NULL ||
        function->operandCount != 1u || function->operands == ZR_NULL ||
        function->resultCount != 1u || function->results == ZR_NULL ||
        function->memoryTokenCount != 0u || function->memoryTokenPool != ZR_NULL ||
        function->phiCount != 0u || function->phiPool != ZR_NULL ||
        function->phiIncomingCount != 0u || function->phiIncoming != ZR_NULL ||
        function->predecessorCount != (branched ? 1u : 0u) ||
        (branched ? function->predecessors == ZR_NULL
                  : function->predecessors != ZR_NULL) ||
        function->successorCount != (branched ? 1u : 0u) ||
        (branched ? function->successors == ZR_NULL
                  : function->successors != ZR_NULL) ||
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
    value = &function->values[0];
    if (value->id != 1u || value->definition != (branched ? 2u : 1u) ||
        value->typeToken != ZR_VALUE_TYPE_INT64 ||
        value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
        value->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
        value->flags != 0u || function->results[0] != 1u ||
        function->operands[0] != 1u)
        return ZR_FALSE;
    if (!scalar_contract_is(&module->contract, module->moduleToken,
                            function->signatureHash,
                            function->contract.layoutHash,
                            module->moduleHash) ||
        !scalar_contract_is(&function->contract, function->functionToken,
                            function->signatureHash,
                            module->contract.layoutHash,
                            module->moduleHash)) return ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool scalar_shape_is(const SZrExecIrModule *module) {
    const SZrExecIrFunction *function;
    const SZrExecIrBlock *block;
    if (!scalar_common_shape_is(module, ZR_FALSE)) return ZR_FALSE;
    function = &module->functions[0];
    block = &function->blocks[0];
    if (block->id != 1u || block->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        !scalar_range_is(block->instructions, 0u, 2u) ||
        !scalar_range_is(block->predecessors, 0u, 0u) ||
        !scalar_range_is(block->successors, 0u, 0u) ||
        block->immediateDominator != 0u ||
        block->terminatorInstructionId != 2u ||
        !scalar_block_aux_is(block) ||
        !scalar_instruction_is(&function->instructions[0],
                               ZR_EXEC_IR_OPCODE_CONSTANT, 1u, 0u, 0u) ||
        !scalar_instruction_is(&function->instructions[1],
                               ZR_EXEC_IR_OPCODE_RETURN, 0u, 1u, 0u))
        return ZR_FALSE;
    return ZrCore_ExecIr_VerifyModule(module, ZR_NULL);
}

static TZrBool scalar_branch_shape_is(const SZrExecIrModule *module) {
    const SZrExecIrFunction *function;
    const SZrExecIrBlock *entry, *ret;
    if (!scalar_common_shape_is(module, ZR_TRUE)) return ZR_FALSE;
    function = &module->functions[0];
    entry = &function->blocks[0];
    ret = &function->blocks[1];
    if (entry->id != 1u || entry->flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        !scalar_range_is(entry->instructions, 0u, 1u) ||
        !scalar_range_is(entry->predecessors, 0u, 0u) ||
        !scalar_range_is(entry->successors, 0u, 1u) ||
        entry->immediateDominator != 0u ||
        entry->terminatorInstructionId != 1u ||
        !scalar_block_aux_is(entry) ||
        ret->id != 2u || ret->flags != 0u ||
        !scalar_range_is(ret->instructions, 1u, 2u) ||
        !scalar_range_is(ret->predecessors, 0u, 1u) ||
        !scalar_range_is(ret->successors, 0u, 0u) ||
        ret->immediateDominator != 1u ||
        ret->terminatorInstructionId != 3u ||
        !scalar_block_aux_is(ret) ||
        function->successors[0] != 2u || function->predecessors[0] != 1u ||
        !scalar_instruction_is(&function->instructions[0],
                               ZR_EXEC_IR_OPCODE_BRANCH, 0u, 0u, 1u) ||
        !scalar_instruction_is(&function->instructions[1],
                               ZR_EXEC_IR_OPCODE_CONSTANT, 1u, 0u, 0u) ||
        !scalar_instruction_is(&function->instructions[2],
                               ZR_EXEC_IR_OPCODE_RETURN, 0u, 1u, 0u))
        return ZR_FALSE;
    return ZrCore_ExecIr_VerifyModule(module, ZR_NULL);
}

static void scalar_write_prefix(SScalarCursor *cursor,
                                const SZrExecIrModule *module,
                                TZrUInt32 magic, TZrUInt16 version,
                                TZrUInt16 length) {
    const SZrExecIrFunction *function = &module->functions[0];
    const SZrExecIrValue *value = &function->values[0];
    scalar_put32(cursor, magic);
    scalar_put16(cursor, version);
    scalar_put16(cursor, length);
    scalar_put32(cursor, module->id);
    scalar_put32(cursor, module->moduleToken);
    scalar_put64(cursor, module->moduleHash);
    scalar_put_contract(cursor, &module->contract);
    scalar_put32(cursor, function->id);
    scalar_put32(cursor, function->functionToken);
    scalar_put64(cursor, function->signatureHash);
    scalar_put32(cursor, function->entryBlockId);
    scalar_put32(cursor, function->sealed);
    scalar_put_contract(cursor, &function->contract);
    scalar_put32(cursor, module->constants[0].typeToken);
    scalar_put32(cursor, module->constants[0].flags);
    scalar_put64(cursor, module->constants[0].bits);
    scalar_put32(cursor, value->id);
    scalar_put32(cursor, value->definition);
    scalar_put32(cursor, value->typeToken);
    scalar_put32(cursor, value->ownership);
    scalar_put32(cursor, value->nullability);
    scalar_put32(cursor, value->flags);
}

static void scalar_write_record(SScalarCursor *cursor,
                                const SZrExecIrModule *module) {
    const SZrExecIrFunction *function = &module->functions[0];
    const SZrExecIrBlock *block = &function->blocks[0];
    scalar_write_prefix(cursor, module, ZR_ARTIFACT_EXEC_IR_SCALAR_MAGIC,
                        ZR_ARTIFACT_EXEC_IR_SCALAR_VERSION,
                        (TZrUInt16)ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE);
    scalar_put32(cursor, block->id);
    scalar_put32(cursor, block->flags);
    scalar_put_range(cursor, block->instructions);
    scalar_put32(cursor, block->terminatorInstructionId);
    scalar_put_instruction(cursor, &function->instructions[0]);
    scalar_put_instruction(cursor, &function->instructions[1]);
    scalar_put32(cursor, function->results[0]);
    scalar_put32(cursor, function->operands[0]);
}

static void scalar_read_prefix(SScalarCursor *cursor, SScalarRecord *record) {
    cursor->offset = 8u;
    record->moduleId = scalar_get32(cursor);
    record->moduleToken = scalar_get32(cursor);
    record->moduleHash = scalar_get64(cursor);
    scalar_get_contract(cursor, &record->moduleContract);
    record->functionId = scalar_get32(cursor);
    record->functionToken = scalar_get32(cursor);
    record->signatureHash = scalar_get64(cursor);
    record->entryBlockId = scalar_get32(cursor);
    record->sealed = scalar_get32(cursor);
    scalar_get_contract(cursor, &record->functionContract);
    record->constant.typeToken = scalar_get32(cursor);
    record->constant.flags = scalar_get32(cursor);
    record->constant.bits = scalar_get64(cursor);
    record->value.id = scalar_get32(cursor);
    record->value.definition = scalar_get32(cursor);
    record->value.typeToken = scalar_get32(cursor);
    record->value.ownership = (EZrExecIrOwnership)scalar_get32(cursor);
    record->value.nullability = (EZrExecIrNullability)scalar_get32(cursor);
    record->value.flags = scalar_get32(cursor);
}

static void scalar_read_record(SScalarCursor *cursor, SScalarRecord *record) {
    scalar_read_prefix(cursor, record);
    record->blockId = scalar_get32(cursor);
    record->blockFlags = scalar_get32(cursor);
    record->blockInstructions = scalar_get_range(cursor);
    record->terminatorId = scalar_get32(cursor);
    scalar_get_instruction(cursor, &record->instructions[0]);
    scalar_get_instruction(cursor, &record->instructions[1]);
    record->resultId = scalar_get32(cursor);
    record->operandId = scalar_get32(cursor);
}

static void scalar_write_branch_block(SScalarCursor *cursor,
                                      const SZrExecIrBlock *block) {
    scalar_put32(cursor, block->id);
    scalar_put32(cursor, block->flags);
    scalar_put_range(cursor, block->instructions);
    scalar_put_range(cursor, block->predecessors);
    scalar_put_range(cursor, block->successors);
    scalar_put32(cursor, block->immediateDominator);
    scalar_put32(cursor, block->terminatorInstructionId);
}

static void scalar_read_branch_block(SScalarCursor *cursor,
                                     SZrExecIrBlock *block) {
    block->id = scalar_get32(cursor);
    block->flags = scalar_get32(cursor);
    block->instructions = scalar_get_range(cursor);
    block->predecessors = scalar_get_range(cursor);
    block->successors = scalar_get_range(cursor);
    block->immediateDominator = scalar_get32(cursor);
    block->terminatorInstructionId = scalar_get32(cursor);
}

static void scalar_write_branch_record(SScalarCursor *cursor,
                                       const SZrExecIrModule *module) {
    const SZrExecIrFunction *function = &module->functions[0];
    scalar_write_prefix(cursor, module, ZR_ARTIFACT_EXEC_IR_BRANCH_MAGIC,
                        ZR_ARTIFACT_EXEC_IR_BRANCH_VERSION,
                        (TZrUInt16)ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        scalar_write_branch_block(cursor, &function->blocks[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        scalar_put_instruction(cursor, &function->instructions[index]);
    scalar_put32(cursor, function->results[0]);
    scalar_put32(cursor, function->operands[0]);
    scalar_put32(cursor, function->successors[0]);
    scalar_put32(cursor, function->predecessors[0]);
}

static void scalar_read_branch_record(SScalarCursor *cursor,
                                      SBranchRecord *record) {
    scalar_read_prefix(cursor, &record->common);
    for (TZrUInt32 index = 0u; index < 2u; ++index)
        scalar_read_branch_block(cursor, &record->blocks[index]);
    for (TZrUInt32 index = 0u; index < 3u; ++index)
        scalar_get_instruction(cursor, &record->instructions[index]);
    record->common.resultId = scalar_get32(cursor);
    record->common.operandId = scalar_get32(cursor);
    record->successorId = scalar_get32(cursor);
    record->predecessorId = scalar_get32(cursor);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrUInt32 size;
    if (module == ZR_NULL || outSize == ZR_NULL)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (scalar_shape_is(module))
        size = ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE;
    else if (scalar_branch_shape_is(module))
        size = ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE;
    else
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    *outSize = size;
    return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrByte temporary[ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE];
    SScalarCursor cursor = {temporary, 0u};
    TZrUInt32 size;
    EZrArtifactExecIrStatus status;
    if (bytes == ZR_NULL || module == ZR_NULL)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalar_GetEncodedSize(module, &size,
                                                        diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (capacity < size)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    if (size == ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE)
        scalar_write_record(&cursor, module);
    else
        scalar_write_branch_record(&cursor, module);
    if (cursor.offset != size)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                           cursor.offset);
    memcpy(bytes, temporary, size);
    return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static EZrArtifactExecIrStatus scalar_read_branch(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SScalarCursor cursor = {(TZrByte *)bytes, 4u};
    SBranchRecord record;
    SZrExecIrModule temporary;
    SZrExecIrFunction *function;
    SZrExecIrRange range;
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId valueId;
    TZrExecIrInstructionId instructionId;
    EZrArtifactExecIrStatus status = ZR_ARTIFACT_EXEC_IR_LIMIT;
    if (length != ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    if (scalar_get16(&cursor) != ZR_ARTIFACT_EXEC_IR_BRANCH_VERSION ||
        scalar_get16(&cursor) != ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
    memset(&record, 0, sizeof(record));
    scalar_read_branch_record(&cursor, &record);
    if (cursor.offset != ZR_ARTIFACT_EXEC_IR_BRANCH_ENCODED_SIZE)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                           cursor.offset);
    if (record.successorId != 2u)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                           ZR_ARTIFACT_EXEC_IR_BRANCH_SUCCESSOR_ID_OFFSET);
    if (record.predecessorId != 1u)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                           ZR_ARTIFACT_EXEC_IR_BRANCH_SUCCESSOR_ID_OFFSET + 4u);
    if (record.common.moduleId != 1u || record.common.functionId != 1u ||
        record.common.entryBlockId != 1u || record.common.sealed > 1u ||
        record.common.value.id != 1u || record.common.value.definition != 2u ||
        record.common.value.flags != 0u || record.common.resultId != 1u ||
        record.common.operandId != 1u || record.blocks[0].id != 1u ||
        record.blocks[1].id != 2u)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = record.common.moduleId;
    temporary.moduleToken = record.common.moduleToken;
    temporary.moduleHash = record.common.moduleHash;
    temporary.contract = record.common.moduleContract;
    if (!ZrCore_ExecIr_ModuleAppendConstant(&temporary,
                                            &record.common.constant, 1u,
                                            &range) ||
        !ZrCore_ExecIr_ModuleAddFunction(&temporary,
                                        record.common.functionToken,
                                        record.common.signatureHash,
                                        &functionId)) goto reject;
    function = ZrCore_ExecIr_ModuleFunctionAt(&temporary, functionId);
    function->contract = record.common.functionContract;
    if (ZrCore_ExecIr_FunctionAddBlock(function, record.blocks[0].flags) != 1u ||
        ZrCore_ExecIr_FunctionAddBlock(function, record.blocks[1].flags) != 2u)
        goto reject;
    valueId = ZrCore_ExecIr_FunctionAddValue(function,
                                            record.common.value.typeToken,
                                            record.common.value.ownership,
                                            record.common.value.nullability);
    if (valueId != 1u ||
        !ZrCore_ExecIr_FunctionAppendResults(function,
                                             &record.common.resultId, 1u,
                                             &range) ||
        !ZrCore_ExecIr_FunctionAppendOperands(function,
                                              &record.common.operandId, 1u,
                                              &range) ||
        !ZrCore_ExecIr_FunctionAppendSuccessors(function,
                                                &record.successorId, 1u,
                                                &range) ||
        !ZrCore_ExecIr_FunctionAppendPredecessors(function,
                                                  &record.predecessorId, 1u,
                                                  &range)) goto reject;
    for (TZrUInt32 index = 0u; index < 3u; ++index) {
        if (!ZrCore_ExecIr_FunctionAppendInstruction(
                     function, &record.instructions[index], &instructionId) ||
            instructionId != index + 1u) goto reject;
    }
    for (TZrUInt32 index = 0u; index < 2u; ++index) {
        SZrExecIrBlock *block = ZrCore_ExecIr_FunctionBlockAt(function, index + 1u);
        block->instructions = record.blocks[index].instructions;
        block->predecessors = record.blocks[index].predecessors;
        block->successors = record.blocks[index].successors;
        block->immediateDominator = record.blocks[index].immediateDominator;
        block->terminatorInstructionId =
                record.blocks[index].terminatorInstructionId;
    }
    function->sealed = (TZrBool)record.common.sealed;
    status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
    if (!scalar_branch_shape_is(&temporary)) goto reject;
    *outModule = temporary;
    return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
reject:
    ZrCore_ExecIr_FreeModule(&temporary);
    return scalar_fail(diagnostic, status, 0u);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalar_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SScalarCursor cursor;
    SScalarRecord record;
    SZrExecIrModule temporary;
    SZrExecIrFunction *function;
    SZrExecIrBlock *block;
    SZrExecIrRange range;
    TZrExecIrFunctionId functionId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrValueId valueId;
    TZrUInt32 magic;
    EZrArtifactExecIrStatus status;
    if (bytes == ZR_NULL || outModule == ZR_NULL ||
        outModule->functionCount != 0u || outModule->functions != ZR_NULL ||
        outModule->constantCount != 0u || outModule->constants != ZR_NULL ||
        outModule->layoutCount != 0u || outModule->layouts != ZR_NULL ||
        outModule->sourceMapCount != 0u || outModule->sourceMaps != ZR_NULL)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (length < 8u)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    cursor.bytes = (TZrByte *)bytes;
    cursor.offset = 0u;
    magic = scalar_get32(&cursor);
    if (magic == ZR_ARTIFACT_EXEC_IR_BRANCH_MAGIC)
        return scalar_read_branch(bytes, length, outModule, diagnostic);
    if (magic != ZR_ARTIFACT_EXEC_IR_SCALAR_MAGIC)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_BAD_MAGIC, 0u);
    if (length != ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    if (scalar_get16(&cursor) != ZR_ARTIFACT_EXEC_IR_SCALAR_VERSION ||
        scalar_get16(&cursor) != ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
    memset(&record, 0, sizeof(record));
    scalar_read_record(&cursor, &record);
    if (cursor.offset != ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE ||
        record.moduleId != 1u || record.functionId != 1u ||
        record.entryBlockId != 1u || record.sealed > 1u ||
        record.value.id != 1u ||
        record.value.definition != 1u || record.value.flags != 0u ||
        record.blockId != 1u ||
        record.blockFlags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        !scalar_range_is(record.blockInstructions, 0u, 2u) ||
        record.terminatorId != 2u || record.resultId != 1u ||
        record.operandId != 1u ||
        record.instructions[0].opcode != ZR_EXEC_IR_OPCODE_CONSTANT ||
        record.instructions[1].opcode != ZR_EXEC_IR_OPCODE_RETURN)
        return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                           ZR_ARTIFACT_EXEC_IR_SCALAR_CONSTANT_OPCODE_OFFSET);
    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = record.moduleId;
    temporary.moduleToken = record.moduleToken;
    temporary.moduleHash = record.moduleHash;
    temporary.contract = record.moduleContract;
    status = ZR_ARTIFACT_EXEC_IR_LIMIT;
    if (!ZrCore_ExecIr_ModuleAppendConstant(&temporary, &record.constant, 1u,
                                            &range) ||
        !ZrCore_ExecIr_ModuleAddFunction(&temporary, record.functionToken,
                                        record.signatureHash, &functionId))
        goto reject;
    function = ZrCore_ExecIr_ModuleFunctionAt(&temporary, functionId);
    function->contract = record.functionContract;
    if (ZrCore_ExecIr_FunctionAddBlock(function, record.blockFlags) != 1u)
        goto reject;
    valueId = ZrCore_ExecIr_FunctionAddValue(function, record.value.typeToken,
                                            record.value.ownership,
                                            record.value.nullability);
    if (valueId != 1u ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &valueId, 1u, &range) ||
        !ZrCore_ExecIr_FunctionAppendOperands(function, &valueId, 1u, &range) ||
        !ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                &record.instructions[0],
                                                &instructionId) || instructionId != 1u ||
        !ZrCore_ExecIr_FunctionAppendInstruction(function,
                                                &record.instructions[1],
                                                &instructionId) || instructionId != 2u)
        goto reject;
    block = ZrCore_ExecIr_FunctionBlockAt(function, 1u);
    block->instructions = record.blockInstructions;
    block->terminatorInstructionId = record.terminatorId;
    function->sealed = (TZrBool)record.sealed;
    status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
    if (!scalar_shape_is(&temporary)) goto reject;
    *outModule = temporary;
    return scalar_fail(diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
reject:
    ZrCore_ExecIr_FreeModule(&temporary);
    return scalar_fail(diagnostic, status, 0u);
}
