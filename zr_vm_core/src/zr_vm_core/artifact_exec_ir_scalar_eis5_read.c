#include "artifact_exec_ir_scalar_eis5_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct SEis5WireData {
    TZrUInt32 moduleId;
    TZrMetadataToken moduleToken;
    TZrUInt64 moduleHash;
    SZrExecutionContract moduleContract;
    TZrUInt32 functionId;
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    TZrExecIrBlockId entryBlockId;
    TZrUInt32 sealed;
    SZrExecutionContract functionContract;
    SZrExecIrConstant *constants;
    SZrExecIrValue *values;
    SZrExecIrBlock *blocks;
    SZrExecIrInstruction *instructions;
    TZrExecIrValueId *results;
    TZrExecIrValueId *operands;
    TZrExecIrBlockId *successors;
    TZrExecIrBlockId *predecessors;
} SEis5WireData;

static TZrBool eis5_allocate_array(void **storage, TZrUInt32 count,
                                   size_t elementSize) {
    if (count == 0u) {
        *storage = ZR_NULL;
        return ZR_TRUE;
    }
    *storage = calloc((size_t)count, elementSize);
    return (TZrBool)(*storage != ZR_NULL);
}

static void eis5_wire_free(SEis5WireData *wire) {
    if (wire == ZR_NULL) return;
    free(wire->constants);
    free(wire->values);
    free(wire->blocks);
    free(wire->instructions);
    free(wire->results);
    free(wire->operands);
    free(wire->successors);
    free(wire->predecessors);
    memset(wire, 0, sizeof(*wire));
}

static TZrBool eis5_range_fits(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)((TZrUInt64)range.start + range.count <= count);
}

static EZrArtifactExecIrStatus eis5_validate_wire(
        const SEis5WireData *wire, const SEis5Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    TZrUInt32 definitions[ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES];

    for (TZrUInt32 index = 0u; index < layout->counts.constants; ++index) {
        const SZrExecIrConstant *constant = &wire->constants[index];
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
    for (TZrUInt32 index = 0u; index < layout->counts.values; ++index) {
        const SZrExecIrValue *value = &wire->values[index];
        if (value->id != index + 1u ||
            (value->typeToken != ZR_VALUE_TYPE_INT64 &&
             value->typeToken != ZR_VALUE_TYPE_BOOL) ||
            value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            value->flags != 0u || value->definition == 0u ||
            value->definition > layout->counts.instructions)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->valuesOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
    }
    for (TZrUInt32 index = 0u; index < layout->counts.blocks; ++index) {
        const SZrExecIrBlock *block = &wire->blocks[index];
        TZrUInt32 expectedFlags = index == 0u
                                          ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY
                                          : 0u;
        if (block->id != index + 1u || block->flags != expectedFlags ||
            !eis5_range_fits(block->instructions,
                             layout->counts.instructions) ||
            !eis5_range_fits(block->predecessors,
                             layout->counts.predecessors) ||
            !eis5_range_fits(block->successors,
                             layout->counts.successors) ||
            block->immediateDominator > layout->counts.blocks)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->blocksOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_SIZE);
    }

    memset(definitions, 0, sizeof(definitions));
    for (TZrUInt32 index = 0u; index < layout->counts.instructions; ++index) {
        const SZrExecIrInstruction *instruction = &wire->instructions[index];
        TZrUInt32 expectedResults, expectedOperands, expectedSuccessors;
        TZrUInt32 recordOffset = layout->instructionsOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE;
        switch ((EZrExecIrOpcode)instruction->opcode) {
            case ZR_EXEC_IR_OPCODE_CONSTANT:
                expectedResults = 1u;
                expectedOperands = 0u;
                expectedSuccessors = 0u;
                if (instruction->layoutId >= layout->counts.constants)
                    return ZrCore_ArtifactExecIrScalarEis5_Fail(
                            diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                            recordOffset + 44u);
                break;
            case ZR_EXEC_IR_OPCODE_ADD:
                expectedResults = 1u;
                expectedOperands = 2u;
                expectedSuccessors = 0u;
                break;
            case ZR_EXEC_IR_OPCODE_COMPARE:
                expectedResults = 1u;
                expectedOperands = 2u;
                expectedSuccessors = 0u;
                if (instruction->typeToken >
                    ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_NE)
                    return ZrCore_ArtifactExecIrScalarEis5_Fail(
                            diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                            recordOffset +
                                    ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_TYPE_TOKEN_OFFSET);
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
                        recordOffset);
        }
        if (instruction->flags != 0u ||
            instruction->results.count != expectedResults ||
            instruction->operands.count != expectedOperands ||
            instruction->successorRange.count != expectedSuccessors ||
            !eis5_range_fits(instruction->results,
                             layout->counts.results) ||
            !eis5_range_fits(instruction->operands,
                             layout->counts.operands) ||
            !eis5_range_fits(instruction->phiRange, 0u) ||
            !eis5_range_fits(instruction->successorRange,
                             layout->counts.successors) ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_COMPARE &&
             instruction->typeToken != 0u) ||
            instruction->matchTypeToken != 0u ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT &&
             instruction->layoutId != 0u) ||
            instruction->memoryIn.start != 0u ||
            instruction->memoryIn.count != 0u ||
            instruction->memoryOut.start != 0u ||
            instruction->memoryOut.count != 0u ||
            instruction->effectIn != 0u || instruction->effectOut != 0u ||
            instruction->deoptId != 0u || instruction->bindingRow != 0u)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    recordOffset);
        for (TZrUInt32 item = instruction->results.start;
             item < instruction->results.start + instruction->results.count;
             ++item) {
            TZrExecIrValueId valueId = wire->results[item];
            TZrExecIrTypeToken expectedType;
            if (valueId == 0u || valueId > layout->counts.values ||
                definitions[valueId - 1u] != 0u)
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->resultsOffset + item *
                                ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
            if (instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT)
                expectedType = wire->constants[instruction->layoutId]
                                       .typeToken;
            else if (instruction->opcode == ZR_EXEC_IR_OPCODE_COMPARE)
                expectedType = (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL;
            else
                expectedType = (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64;
            if (wire->values[valueId - 1u].typeToken != expectedType)
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->valuesOffset + (valueId - 1u) *
                                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
            definitions[valueId - 1u] = index + 1u;
        }
        for (TZrUInt32 item = instruction->operands.start;
             item < instruction->operands.start + instruction->operands.count;
             ++item) {
            TZrExecIrValueId valueId = wire->operands[item];
            TZrExecIrTypeToken operandType;
            if (valueId == 0u || valueId > layout->counts.values)
                return ZrCore_ArtifactExecIrScalarEis5_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->operandsOffset + item *
                                ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
            operandType = wire->values[valueId - 1u].typeToken;
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
    if (layout->counts.results != layout->counts.values)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_RESULT_COUNT_OFFSET);
    for (TZrUInt32 index = 0u; index < layout->counts.values; ++index) {
        if (definitions[index] == 0u ||
            wire->values[index].definition != definitions[index])
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->valuesOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE + 4u);
    }
    for (TZrUInt32 index = 0u; index < layout->counts.successors; ++index) {
        TZrExecIrBlockId blockId = wire->successors[index];
        if (blockId == 0u || blockId > layout->counts.blocks)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->successorsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
    }
    for (TZrUInt32 index = 0u; index < layout->counts.predecessors; ++index) {
        TZrExecIrBlockId blockId = wire->predecessors[index];
        if (blockId == 0u || blockId > layout->counts.blocks)
            return ZrCore_ArtifactExecIrScalarEis5_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->predecessorsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
    }
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static EZrArtifactExecIrStatus eis5_decode_records(
        SEis5Cursor *cursor, const SEis5Layout *layout,
        SEis5WireData *wire, SZrArtifactExecIrDiagnostic *diagnostic) {
    for (TZrUInt32 index = 0u; index < layout->counts.constants; ++index)
        ZrCore_ArtifactExecIrScalarEis5_GetConstant(cursor,
                                                    &wire->constants[index]);
    for (TZrUInt32 index = 0u; index < layout->counts.values; ++index)
        ZrCore_ArtifactExecIrScalarEis5_GetValue(cursor,
                                                  &wire->values[index]);
    for (TZrUInt32 index = 0u; index < layout->counts.blocks; ++index)
        ZrCore_ArtifactExecIrScalarEis5_GetBlock(cursor,
                                                  &wire->blocks[index]);
    for (TZrUInt32 index = 0u; index < layout->counts.instructions; ++index)
        ZrCore_ArtifactExecIrScalarEis5_GetInstruction(
                cursor, &wire->instructions[index]);
    for (TZrUInt32 index = 0u; index < layout->counts.results; ++index)
        wire->results[index] = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    for (TZrUInt32 index = 0u; index < layout->counts.operands; ++index)
        wire->operands[index] = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    for (TZrUInt32 index = 0u; index < layout->counts.successors; ++index)
        wire->successors[index] =
                ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    for (TZrUInt32 index = 0u; index < layout->counts.predecessors; ++index)
        wire->predecessors[index] =
                ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    if (cursor->offset != layout->totalSize)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                cursor->offset);
    return eis5_validate_wire(wire, layout, diagnostic);
}

static TZrBool eis5_wire_allocate(const SEis5Layout *layout,
                                  SEis5WireData *wire) {
    return (TZrBool)(eis5_allocate_array((void **)&wire->constants,
                                         layout->counts.constants,
                                         sizeof(*wire->constants)) &&
                     eis5_allocate_array((void **)&wire->values,
                                         layout->counts.values,
                                         sizeof(*wire->values)) &&
                     eis5_allocate_array((void **)&wire->blocks,
                                         layout->counts.blocks,
                                         sizeof(*wire->blocks)) &&
                     eis5_allocate_array((void **)&wire->instructions,
                                         layout->counts.instructions,
                                         sizeof(*wire->instructions)) &&
                     eis5_allocate_array((void **)&wire->results,
                                         layout->counts.results,
                                         sizeof(*wire->results)) &&
                     eis5_allocate_array((void **)&wire->operands,
                                         layout->counts.operands,
                                         sizeof(*wire->operands)) &&
                     eis5_allocate_array((void **)&wire->successors,
                                         layout->counts.successors,
                                         sizeof(*wire->successors)) &&
                     eis5_allocate_array((void **)&wire->predecessors,
                                         layout->counts.predecessors,
                                         sizeof(*wire->predecessors)));
}

static EZrArtifactExecIrStatus eis5_build_module(
        const SEis5Layout *layout, const SEis5WireData *wire,
        SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SZrExecIrModule temporary;
    SZrExecIrFunction *function;
    SZrExecIrRange range;
    TZrExecIrFunctionId functionId = 0u;
    TZrExecIrInstructionId instructionId = 0u;
    EZrArtifactExecIrStatus status = ZR_ARTIFACT_EXEC_IR_LIMIT;

    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = wire->moduleId;
    temporary.moduleToken = wire->moduleToken;
    temporary.moduleHash = wire->moduleHash;
    temporary.contract = wire->moduleContract;
    if (!ZrCore_ExecIr_ModuleAppendConstant(
                &temporary, wire->constants, layout->counts.constants,
                &range) ||
        range.start != 0u || range.count != layout->counts.constants ||
        !ZrCore_ExecIr_ModuleAddFunction(
                &temporary, wire->functionToken, wire->signatureHash,
                &functionId) ||
        functionId != 1u)
        goto reject;
    function = ZrCore_ExecIr_ModuleFunctionAt(&temporary, functionId);
    if (function == ZR_NULL) goto reject;
    function->contract = wire->functionContract;
    for (TZrUInt32 index = 0u; index < layout->counts.blocks; ++index) {
        if (ZrCore_ExecIr_FunctionAddBlock(
                    function, wire->blocks[index].flags) != index + 1u)
            goto reject;
    }
    if (function->entryBlockId != wire->entryBlockId) goto reject;
    for (TZrUInt32 index = 0u; index < layout->counts.values; ++index) {
        TZrExecIrValueId valueId = ZrCore_ExecIr_FunctionAddValue(
                function, wire->values[index].typeToken,
                wire->values[index].ownership,
                wire->values[index].nullability);
        if (valueId != index + 1u) goto reject;
        function->values[index].flags = wire->values[index].flags;
    }
    if (!ZrCore_ExecIr_FunctionAppendResults(
                function, wire->results, layout->counts.results, &range) ||
        range.start != 0u || range.count != layout->counts.results ||
        !ZrCore_ExecIr_FunctionAppendOperands(
                function, wire->operands, layout->counts.operands, &range) ||
        range.start != 0u || range.count != layout->counts.operands ||
        !ZrCore_ExecIr_FunctionAppendSuccessors(
                function, wire->successors, layout->counts.successors,
                &range) ||
        range.start != 0u || range.count != layout->counts.successors ||
        !ZrCore_ExecIr_FunctionAppendPredecessors(
                function, wire->predecessors, layout->counts.predecessors,
                &range) ||
        range.start != 0u || range.count != layout->counts.predecessors)
        goto reject;
    for (TZrUInt32 index = 0u; index < layout->counts.instructions; ++index) {
        if (!ZrCore_ExecIr_FunctionAppendInstruction(
                    function, &wire->instructions[index], &instructionId) ||
            instructionId != index + 1u)
            goto reject;
    }
    for (TZrUInt32 index = 0u; index < layout->counts.values; ++index) {
        if (function->values[index].definition !=
            wire->values[index].definition) {
            status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
            goto reject;
        }
    }
    for (TZrUInt32 index = 0u; index < layout->counts.blocks; ++index) {
        SZrExecIrBlock *block = &function->blocks[index];
        block->instructions = wire->blocks[index].instructions;
        block->predecessors = wire->blocks[index].predecessors;
        block->successors = wire->blocks[index].successors;
        block->immediateDominator = wire->blocks[index].immediateDominator;
        block->terminatorInstructionId =
                wire->blocks[index].terminatorInstructionId;
    }
    if (wire->sealed > 1u) {
        status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
        goto reject;
    }
    function->sealed = (TZrBool)wire->sealed;

    status = ZrCore_ArtifactExecIrScalarEis5_ValidateModule(
            &temporary, &(SEis5Layout){0}, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) {
        ZrCore_ExecIr_FreeModule(&temporary);
        return status;
    }
    *outModule = temporary;
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);

reject:
    ZrCore_ExecIr_FreeModule(&temporary);
    return ZrCore_ArtifactExecIrScalarEis5_Fail(diagnostic, status, 0u);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis5Cursor cursor;
    SEis5Counts counts;
    SEis5Layout layout;
    SEis5WireData wire;
    TZrUInt32 magic, declaredLength;
    TZrUInt16 version, reserved;
    EZrArtifactExecIrStatus status;

    memset(&wire, 0, sizeof(wire));
    if (bytes == ZR_NULL || outModule == ZR_NULL ||
        outModule->functionCount != 0u || outModule->functions != ZR_NULL ||
        outModule->constantCount != 0u || outModule->constants != ZR_NULL ||
        outModule->layoutCount != 0u || outModule->layouts != ZR_NULL ||
        outModule->sourceMapCount != 0u || outModule->sourceMaps != ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (length < ZR_ARTIFACT_EXEC_IR_EIS5_HEADER_SIZE)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);

    cursor.bytes = (TZrByte *)bytes;
    cursor.offset = 0u;
    magic = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    if (magic != ZR_ARTIFACT_EXEC_IR_EIS5_MAGIC)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_BAD_MAGIC, 0u);
    version = ZrCore_ArtifactExecIrScalarEis5_Get16(&cursor);
    reserved = ZrCore_ArtifactExecIrScalarEis5_Get16(&cursor);
    declaredLength = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.constants = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.values = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.blocks = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.instructions = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.operands = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.results = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.successors = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.predecessors = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    if (version != ZR_ARTIFACT_EXEC_IR_EIS5_VERSION)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,
                ZR_ARTIFACT_EXEC_IR_EIS5_VERSION_OFFSET);
    if (reserved != 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_RESERVED_OFFSET);

    status = ZrCore_ArtifactExecIrScalarEis5_ComputeLayout(
            &counts, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (declaredLength > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BYTES ||
        length > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BYTES)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT,
                ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (declaredLength != layout.totalSize)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    if (length < declaredLength)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    if (length > declaredLength)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);

    wire.moduleId = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    wire.moduleToken = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    wire.moduleHash = ZrCore_ArtifactExecIrScalarEis5_Get64(&cursor);
    ZrCore_ArtifactExecIrScalarEis5_GetContract(&cursor,
                                                &wire.moduleContract);
    wire.functionId = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    wire.functionToken = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    wire.signatureHash = ZrCore_ArtifactExecIrScalarEis5_Get64(&cursor);
    wire.entryBlockId = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    wire.sealed = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    ZrCore_ArtifactExecIrScalarEis5_GetContract(
            &cursor, &wire.functionContract);
    if (cursor.offset != layout.constantsOffset)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                cursor.offset);
    if (wire.moduleId != 1u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_ID_OFFSET);
    if (wire.moduleToken == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_TOKEN_OFFSET);
    if (wire.moduleHash == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_HASH_OFFSET);
    if (wire.functionId != 1u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_FUNCTION_ID_OFFSET);
    if (wire.functionToken == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_FUNCTION_TOKEN_OFFSET);
    if (wire.signatureHash == 0u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_SIGNATURE_HASH_OFFSET);
    if (wire.entryBlockId != 1u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_ENTRY_BLOCK_OFFSET);
    if (wire.sealed > 1u)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS5_SEALED_OFFSET);
    if (!eis5_wire_allocate(&layout, &wire)) {
        eis5_wire_free(&wire);
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT,
                ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
    }
    status = eis5_decode_records(&cursor, &layout, &wire, diagnostic);
    if (status == ZR_ARTIFACT_EXEC_IR_OK)
        status = eis5_build_module(&layout, &wire, outModule, diagnostic);
    eis5_wire_free(&wire);
    return status;
}
