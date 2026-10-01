#include "artifact_exec_ir_scalar_eis6_internal.h"
#include "zr_vm_common/zr_type_conf.h"

#include <stdint.h>
#include <string.h>

static TZrBool eis6_checked_add(TZrUInt64 left, TZrUInt64 right,
                                TZrUInt64 *out) {
    if (out == ZR_NULL || right > UINT64_MAX - left) return ZR_FALSE;
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool eis6_checked_mul(TZrUInt64 left, TZrUInt64 right,
                                TZrUInt64 *out) {
    if (out == ZR_NULL || (left != 0u && right > UINT64_MAX / left))
        return ZR_FALSE;
    *out = left * right;
    return ZR_TRUE;
}

static TZrBool eis6_range_is(SZrExecIrRange range,
                             TZrUInt32 start, TZrUInt32 count) {
    return (TZrBool)(range.start == start && range.count == count);
}

static TZrBool eis6_range_fits(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)((TZrUInt64)range.start + range.count <= count);
}

static TZrBool eis6_contract_is(const SZrExecutionContract *contract,
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

static TZrBool eis6_block_aux_is_empty(const SZrExecIrBlock *block) {
    if (!eis6_range_is(block->phis, 0u, 0u) ||
        block->effectPhiResult != 0u ||
        !eis6_range_is(block->effectPhiIncomings, 0u, 0u))
        return ZR_FALSE;
    for (TZrUInt32 index = 0u; index < ZR_EXEC_IR_MEMORY_CLASS_COUNT;
         ++index) {
        if (block->memoryPhiResults[index] != 0u ||
            !eis6_range_is(block->memoryPhiIncomings[index], 0u, 0u))
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool eis6_side_tables_are_supported(
        const SZrExecIrFunction *function) {
    return (TZrBool)(function->phiCount == 0u && function->phiPool == ZR_NULL &&
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

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_Fail(
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

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_ComputeLayout(
        const SEis5Counts *counts, TZrUInt32 memoryTokenCount,
        TZrUInt32 bindingRowCount, SEis6Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis5Layout graph;
    TZrUInt64 poolItems, next, total, bytes;
    EZrArtifactExecIrStatus status;
    if (counts == ZR_NULL || layout == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    memset(layout, 0, sizeof(*layout));
    status = ZrCore_ArtifactExecIrScalarEis5_ComputeLayout(
            counts, &graph, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (bindingRowCount > counts->instructions)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_ROW_COUNT_OFFSET);

    poolItems = counts->operands;
    if (!eis6_checked_add(poolItems, counts->results, &next) ||
        !eis6_checked_add(next, counts->successors, &poolItems) ||
        !eis6_checked_add(poolItems, counts->predecessors, &next) ||
        !eis6_checked_add(next, memoryTokenCount, &poolItems) ||
        poolItems > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_POOL_ITEMS)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT,
                ZR_ARTIFACT_EXEC_IR_EIS6_MEMORY_TOKEN_COUNT_OFFSET);

    layout->counts = *counts;
    layout->graph = graph;
    layout->memoryTokenCount = memoryTokenCount;
    layout->bindingRowCount = bindingRowCount;
    layout->constantsOffset = graph.constantsOffset + 12u;
    layout->valuesOffset = graph.valuesOffset + 12u;
    layout->blocksOffset = graph.blocksOffset + 12u;
    layout->instructionsOffset = graph.instructionsOffset + 12u;
    layout->resultsOffset = graph.resultsOffset + 12u;
    layout->operandsOffset = graph.operandsOffset + 12u;
    layout->successorsOffset = graph.successorsOffset + 12u;
    layout->predecessorsOffset = graph.predecessorsOffset + 12u;
    total = (TZrUInt64)graph.totalSize + 12u;
    if (total > UINT32_MAX) goto size_limit;
    layout->memoryTokensOffset = (TZrUInt32)total;
    if (!eis6_checked_mul(memoryTokenCount,
                          ZR_ARTIFACT_EXEC_IR_EIS6_MEMORY_TOKEN_SIZE,
                          &bytes) ||
        !eis6_checked_add(total, bytes, &total) || total > UINT32_MAX)
        goto size_limit;
    layout->bindingRowsOffset = (TZrUInt32)total;
    if (!eis6_checked_mul(bindingRowCount,
                          ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_ROW_SIZE,
                          &bytes) ||
        !eis6_checked_add(total, bytes, &total) ||
        total > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BYTES || total > UINT32_MAX)
        goto size_limit;
    layout->totalSize = (TZrUInt32)total;
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);

size_limit:
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT,
            ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET);
}

void ZrCore_ArtifactExecIrScalarEis6_GetMetadata(
        SEis5Cursor *cursor, SEis6Metadata *metadata) {
    memset(metadata, 0, sizeof(*metadata));
    metadata->moduleId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    metadata->moduleToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    metadata->moduleHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    ZrCore_ArtifactExecIrScalarEis5_GetContract(cursor,
                                                &metadata->moduleContract);
    metadata->functionId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    metadata->functionToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    metadata->signatureHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    metadata->entryBlockId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    metadata->sealed = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    ZrCore_ArtifactExecIrScalarEis5_GetContract(cursor,
                                                &metadata->functionContract);
}

void ZrCore_ArtifactExecIrScalarEis6_PutMetadata(
        SEis5Cursor *cursor, const SEis6Metadata *metadata) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, metadata->moduleId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, metadata->moduleToken);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, metadata->moduleHash);
    ZrCore_ArtifactExecIrScalarEis5_PutContract(cursor,
                                                &metadata->moduleContract);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, metadata->functionId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, metadata->functionToken);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, metadata->signatureHash);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, metadata->entryBlockId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, metadata->sealed);
    ZrCore_ArtifactExecIrScalarEis5_PutContract(cursor,
                                                &metadata->functionContract);
}

void ZrCore_ArtifactExecIrScalarEis6_GetBindingRow(
        SEis5Cursor *cursor, SZrExecIrBindingRow *row) {
    memset(row, 0, sizeof(*row));
    row->rowIndex = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->instructionId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->segmentIndex = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.bindingKind = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.targetMetadataToken =
            ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.signatureToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.ownerTypeToken = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.signatureHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    row->contract.moduleSignatureHash =
            ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    row->contract.layoutVersion = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.dispatchSlot = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.layoutHash = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    row->contract.operation = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.reserved0 = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->contract.reserved1 = ZrCore_ArtifactExecIrScalarEis5_Get64(cursor);
    row->location.kind = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->location.targetIndex = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->location.ownerDepth = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->location.flags = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
    row->sourceId = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
}

void ZrCore_ArtifactExecIrScalarEis6_PutBindingRow(
        SEis5Cursor *cursor, const SZrExecIrBindingRow *row) {
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->rowIndex);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->instructionId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->segmentIndex);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.bindingKind);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            cursor, row->contract.targetMetadataToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.signatureToken);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.ownerTypeToken);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, row->contract.signatureHash);
    ZrCore_ArtifactExecIrScalarEis5_Put64(
            cursor, row->contract.moduleSignatureHash);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.layoutVersion);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.dispatchSlot);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, row->contract.layoutHash);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.operation);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->contract.reserved0);
    ZrCore_ArtifactExecIrScalarEis5_Put64(cursor, row->contract.reserved1);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->location.kind);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->location.targetIndex);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->location.ownerDepth);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->location.flags);
    ZrCore_ArtifactExecIrScalarEis5_Put32(cursor, row->sourceId);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_ValidateModule(
        const SZrExecIrModule *module, SEis6Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    const SZrExecIrFunction *function;
    SEis5Counts counts;
    TZrUInt32 definitions[ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES];
    EZrArtifactExecIrStatus status;
    if (module == ZR_NULL || layout == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (module->functionCount != 1u || module->functions == ZR_NULL ||
        module->layoutCount != 0u || module->layouts != ZR_NULL ||
        module->sourceMapCount != 0u || module->sourceMaps != ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
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
    status = ZrCore_ArtifactExecIrScalarEis6_ComputeLayout(
            &counts, function->memoryTokenCount, function->bindingRowCount,
            layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (module->constantCount > module->constantCapacity ||
        module->functionCount > module->functionCapacity ||
        module->constants == ZR_NULL || function->valueCapacity < counts.values ||
        function->blockCapacity < counts.blocks ||
        function->instructionCapacity < counts.instructions ||
        function->operandCapacity < counts.operands ||
        function->resultCapacity < counts.results ||
        function->successorCapacity < counts.successors ||
        function->predecessorCapacity < counts.predecessors ||
        function->memoryTokenCount > function->memoryTokenCapacity ||
        (counts.values != 0u && function->values == ZR_NULL) ||
        (counts.blocks != 0u && function->blocks == ZR_NULL) ||
        (counts.instructions != 0u && function->instructions == ZR_NULL) ||
        (counts.operands != 0u && function->operands == ZR_NULL) ||
        (counts.results != 0u && function->results == ZR_NULL) ||
        (counts.successors != 0u && function->successors == ZR_NULL) ||
        (counts.predecessors != 0u && function->predecessors == ZR_NULL) ||
        (function->memoryTokenCount != 0u &&
         function->memoryTokenPool == ZR_NULL) ||
        function->bindingRowsSchemaVersion !=
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED ||
        function->bindingRowCount > function->bindingRowCapacity ||
        (function->bindingRowCount != 0u && function->bindingRows == ZR_NULL) ||
        !eis6_side_tables_are_supported(function) ||
        (function->sealed != ZR_FALSE && function->sealed != ZR_TRUE))
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    if (module->id != 1u || module->moduleToken == 0u || module->moduleHash == 0u ||
        function->id != 1u || function->functionToken == 0u ||
        function->signatureHash == 0u || function->entryBlockId != 1u ||
        !eis6_contract_is(&module->contract, module->moduleToken,
                          function->signatureHash,
                          function->contract.layoutHash, module->moduleHash) ||
        !eis6_contract_is(&function->contract, function->functionToken,
                          function->signatureHash,
                          module->contract.layoutHash, module->moduleHash))
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    if (function->resultCount != function->valueCount)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);

    for (TZrUInt32 index = 0u; index < counts.constants; ++index) {
        const SZrExecIrConstant *constant = &module->constants[index];
        if ((constant->typeToken != ZR_VALUE_TYPE_INT64 &&
             constant->typeToken != ZR_VALUE_TYPE_BOOL) ||
            constant->flags != 0u ||
            (constant->typeToken == ZR_VALUE_TYPE_BOOL && constant->bits > 1u))
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->constantsOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_SIZE);
    }
    memset(definitions, 0, sizeof(definitions));
    for (TZrUInt32 index = 0u; index < counts.values; ++index) {
        const SZrExecIrValue *value = &function->values[index];
        if (value->id != index + 1u ||
            (value->typeToken != ZR_VALUE_TYPE_INT64 &&
             value->typeToken != ZR_VALUE_TYPE_BOOL) ||
            value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            value->flags != 0u || value->definition == 0u ||
            value->definition > counts.instructions)
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
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
            !eis6_range_fits(block->instructions, counts.instructions) ||
            !eis6_range_fits(block->predecessors, counts.predecessors) ||
            !eis6_range_fits(block->successors, counts.successors) ||
            block->immediateDominator > counts.blocks ||
            !eis6_block_aux_is_empty(block))
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->blocksOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_SIZE);
    }

    for (TZrUInt32 index = 0u; index < counts.instructions; ++index) {
        const SZrExecIrInstruction *instruction =
                &function->instructions[index];
        TZrUInt32 expectedResults, expectedOperands, expectedSuccessors;
        TZrUInt32 recordOffset = layout->instructionsOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE;
        TZrUInt16 expectedFlags = 0u;
        TZrBool isCall = ZR_FALSE;
        switch ((EZrExecIrOpcode)instruction->opcode) {
            case ZR_EXEC_IR_OPCODE_CONSTANT:
                expectedResults = 1u;
                expectedOperands = 0u;
                expectedSuccessors = 0u;
                if (instruction->layoutId >= counts.constants)
                    return ZrCore_ArtifactExecIrScalarEis6_Fail(
                            diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                            recordOffset + 44u);
                break;
            case ZR_EXEC_IR_OPCODE_ADD:
            case ZR_EXEC_IR_OPCODE_SUB:
            case ZR_EXEC_IR_OPCODE_MUL:
                expectedResults = 1u;
                expectedOperands = 2u;
                expectedSuccessors = 0u;
                break;
            case ZR_EXEC_IR_OPCODE_DIV:
                expectedResults = 1u;
                expectedOperands = 2u;
                expectedSuccessors = 0u;
                expectedFlags = (TZrUInt16)ZR_EXEC_IR_FLAG_MAY_THROW;
                break;
            case ZR_EXEC_IR_OPCODE_COMPARE:
                expectedResults = 1u;
                expectedOperands = 2u;
                expectedSuccessors = 0u;
                if (instruction->typeToken >
                    ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_NE)
                    return ZrCore_ArtifactExecIrScalarEis6_Fail(
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
            case ZR_EXEC_IR_OPCODE_CALL:
                expectedResults = 1u;
                expectedOperands = instruction->operands.count;
                expectedSuccessors = 0u;
                expectedFlags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                            ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
                isCall = ZR_TRUE;
                break;
            default:
                return ZrCore_ArtifactExecIrScalarEis6_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        recordOffset);
        }
        if (instruction->flags != expectedFlags ||
            (isCall && (instruction->effectIn == 0u ||
                        instruction->effectOut <= instruction->effectIn)) ||
            (instruction->opcode == ZR_EXEC_IR_OPCODE_DIV &&
             (instruction->effectIn == 0u ||
              instruction->effectOut <= instruction->effectIn)) ||
            ((instruction->opcode != ZR_EXEC_IR_OPCODE_DIV && !isCall) &&
             (instruction->effectIn != 0u || instruction->effectOut != 0u)))
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    recordOffset +
                            ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_FLAGS_OFFSET);
        if (instruction->results.count != expectedResults ||
            (!isCall && instruction->operands.count != expectedOperands) ||
            instruction->successorRange.count != expectedSuccessors ||
            !eis6_range_fits(instruction->results, counts.results) ||
            !eis6_range_fits(instruction->operands, counts.operands) ||
            !eis6_range_is(instruction->phiRange, 0u, 0u) ||
            !eis6_range_fits(instruction->successorRange, counts.successors) ||
            !eis6_range_fits(instruction->memoryIn,
                             function->memoryTokenCount) ||
            !eis6_range_fits(instruction->memoryOut,
                             function->memoryTokenCount) ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_COMPARE &&
             instruction->typeToken != 0u) ||
            instruction->matchTypeToken != 0u ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT &&
             instruction->layoutId != 0u) ||
            (!isCall &&
             (!eis6_range_is(instruction->memoryIn, 0u, 0u) ||
              !eis6_range_is(instruction->memoryOut, 0u, 0u))) ||
            instruction->deoptId != 0u ||
            (!isCall && instruction->bindingRow != 0u))
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    recordOffset);

        for (TZrUInt32 item = instruction->results.start;
             item < instruction->results.start + instruction->results.count;
             ++item) {
            TZrExecIrValueId valueId = function->results[item];
            TZrExecIrTypeToken expectedType;
            if (valueId == 0u || valueId > counts.values ||
                definitions[valueId - 1u] != 0u)
                return ZrCore_ArtifactExecIrScalarEis6_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->resultsOffset + item *
                                ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
            if (instruction->opcode == ZR_EXEC_IR_OPCODE_CONSTANT)
                expectedType = module->constants[instruction->layoutId]
                                       .typeToken;
            else if (instruction->opcode == ZR_EXEC_IR_OPCODE_COMPARE)
                expectedType = (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL;
            else if (isCall)
                expectedType = function->values[valueId - 1u].typeToken;
            else
                expectedType = (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64;
            if (function->values[valueId - 1u].typeToken != expectedType)
                return ZrCore_ArtifactExecIrScalarEis6_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->valuesOffset + (valueId - 1u) *
                                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
            definitions[valueId - 1u] = index + 1u;
        }
        for (TZrUInt32 item = instruction->operands.start;
             item < instruction->operands.start + instruction->operands.count;
             ++item) {
            TZrExecIrValueId valueId = function->operands[item];
            TZrExecIrTypeToken typeToken;
            if (valueId == 0u || valueId > counts.values)
                return ZrCore_ArtifactExecIrScalarEis6_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->operandsOffset + item *
                                ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE);
            typeToken = function->values[valueId - 1u].typeToken;
            if (isCall) {
                if (typeToken != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64 &&
                    typeToken != (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL)
                    return ZrCore_ArtifactExecIrScalarEis6_Fail(
                            diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                            layout->valuesOffset + (valueId - 1u) *
                                    ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
            } else if ((instruction->opcode ==
                                ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                        typeToken != (TZrExecIrTypeToken)ZR_VALUE_TYPE_BOOL &&
                        typeToken != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64) ||
                       (instruction->opcode !=
                                ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                        typeToken != (TZrExecIrTypeToken)ZR_VALUE_TYPE_INT64)) {
                return ZrCore_ArtifactExecIrScalarEis6_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                        layout->valuesOffset + (valueId - 1u) *
                                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE);
            }
        }
    }
    for (TZrUInt32 index = 0u; index < counts.values; ++index) {
        if (definitions[index] == 0u ||
            function->values[index].definition != definitions[index])
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                    layout->valuesOffset + index *
                            ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE + 4u);
    }
    if (!ZrCore_ExecIr_FunctionValidateBindingRows(function, ZR_NULL) ||
        !ZrCore_ExecIr_VerifyModule(module, ZR_NULL))
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION, 0u);
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_GetEncodedSize(
        const SZrExecIrModule *module, TZrUInt32 *outSize,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis6Layout layout;
    EZrArtifactExecIrStatus status;
    if (module == ZR_NULL || outSize == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalarEis6_ValidateModule(
            module, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    *outSize = layout.totalSize;
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}
