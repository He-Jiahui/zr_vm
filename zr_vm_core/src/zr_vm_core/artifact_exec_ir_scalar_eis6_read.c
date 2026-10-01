#include "artifact_exec_ir_scalar_eis6_internal.h"

#include <stdlib.h>
#include <string.h>

static TZrBool eis6_output_is_empty(const SZrExecIrModule *module) {
    return (TZrBool)(module != ZR_NULL && module->id == 0u &&
                     module->moduleToken == 0u && module->moduleHash == 0u &&
                     module->contract.schemaVersion == 0u &&
                     module->functionCount == 0u &&
                     module->functionCapacity == 0u &&
                     module->functions == ZR_NULL &&
                     module->constantCount == 0u &&
                     module->constantCapacity == 0u &&
                     module->constants == ZR_NULL &&
                     module->layoutCount == 0u &&
                     module->layoutCapacity == 0u &&
                     module->layouts == ZR_NULL &&
                     module->sourceMapCount == 0u &&
                     module->sourceMapCapacity == 0u &&
                     module->sourceMaps == ZR_NULL);
}

static EZrArtifactExecIrStatus eis6_read_pool(
        SEis5Cursor *cursor, SZrExecIrFunction *function,
        TZrUInt32 count, TZrBool results,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    for (TZrUInt32 index = 0u; index < count; ++index) {
        TZrExecIrValueId id = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
        if (results) {
            if (!ZrCore_ExecIr_FunctionAppendResults(function, &id, 1u,
                                                     ZR_NULL))
                return ZrCore_ArtifactExecIrScalarEis6_Fail(
                        diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, cursor->offset);
        } else if (!ZrCore_ExecIr_FunctionAppendOperands(function, &id, 1u,
                                                          ZR_NULL)) {
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, cursor->offset);
        }
    }
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

static EZrArtifactExecIrStatus eis6_read_block_edges(
        SEis5Cursor *cursor, SZrExecIrFunction *function,
        TZrUInt32 count, TZrBool successors,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    for (TZrUInt32 index = 0u; index < count; ++index) {
        TZrExecIrBlockId id = ZrCore_ArtifactExecIrScalarEis5_Get32(cursor);
        TZrBool appended = successors
                ? ZrCore_ExecIr_FunctionAppendSuccessors(
                          function, &id, 1u, ZR_NULL)
                : ZrCore_ExecIr_FunctionAppendPredecessors(
                          function, &id, 1u, ZR_NULL);
        if (!appended)
            return ZrCore_ArtifactExecIrScalarEis6_Fail(
                    diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, cursor->offset);
    }
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_Read(
        const TZrByte *bytes, TZrUInt32 length, SZrExecIrModule *outModule,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis5Cursor cursor;
    SEis5Counts counts;
    SEis6Layout layout;
    SEis6Metadata metadata;
    SZrExecIrModule temporary;
    SZrExecIrFunction *function;
    SZrExecIrRange appendRange;
    SZrExecIrBindingRow *rows = ZR_NULL;
    TZrUInt32 memoryTokenCount, schemaVersion, rowCount;
    TZrUInt32 definitions[ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES];
    TZrUInt16 version, reserved;
    TZrUInt32 declaredLength, magic;
    TZrUInt32 functionId;
    EZrArtifactExecIrStatus status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
    SZrExecIrDiagnostic coreDiagnostic;

    if (bytes == ZR_NULL || !eis6_output_is_empty(outModule))
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    if (length < ZR_ARTIFACT_EXEC_IR_EIS6_HEADER_SIZE)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);

    cursor.bytes = (TZrByte *)bytes;
    cursor.offset = 0u;
    magic = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    version = ZrCore_ArtifactExecIrScalarEis5_Get16(&cursor);
    reserved = ZrCore_ArtifactExecIrScalarEis5_Get16(&cursor);
    declaredLength = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    if (magic != ZR_ARTIFACT_EXEC_IR_EIS6_MAGIC)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_BAD_MAGIC, 0u);
    if (version != ZR_ARTIFACT_EXEC_IR_EIS6_VERSION || reserved != 0u)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION, 4u);
    if (declaredLength > ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BYTES)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, 8u);
    if (declaredLength != length)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 8u);

    counts.constants = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.values = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.blocks = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.instructions = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.operands = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.results = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.successors = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    counts.predecessors = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    memoryTokenCount = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    schemaVersion = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    rowCount = ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
    if (schemaVersion != ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,
                ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_SCHEMA_OFFSET);
    status = ZrCore_ArtifactExecIrScalarEis6_ComputeLayout(
            &counts, memoryTokenCount, rowCount, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (layout.totalSize != length)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 8u);

    cursor.offset = ZR_ARTIFACT_EXEC_IR_EIS6_MODULE_ID_OFFSET;
    ZrCore_ArtifactExecIrScalarEis6_GetMetadata(&cursor, &metadata);
    if (metadata.moduleId != 1u || metadata.moduleToken == 0u ||
        metadata.moduleHash == 0u || metadata.functionId != 1u ||
        metadata.functionToken == 0u || metadata.signatureHash == 0u ||
        metadata.entryBlockId != 1u || metadata.sealed > 1u)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                ZR_ARTIFACT_EXEC_IR_EIS6_MODULE_ID_OFFSET);

    ZrCore_ExecIr_ModuleInit(&temporary);
    temporary.id = metadata.moduleId;
    temporary.moduleToken = metadata.moduleToken;
    temporary.moduleHash = metadata.moduleHash;
    temporary.contract = metadata.moduleContract;
    for (TZrUInt32 index = 0u; index < counts.constants; ++index) {
        SZrExecIrConstant constant;
        cursor.offset = layout.constantsOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_SIZE;
        memset(&constant, 0, sizeof(constant));
        ZrCore_ArtifactExecIrScalarEis5_GetConstant(&cursor, &constant);
        if (!ZrCore_ExecIr_ModuleAppendConstant(
                    &temporary, &constant, 1u, &appendRange)) {
            status = ZR_ARTIFACT_EXEC_IR_LIMIT;
            goto reject;
        }
    }
    if (!ZrCore_ExecIr_ModuleAddFunction(
                &temporary, metadata.functionToken, metadata.signatureHash,
                &functionId) || functionId != metadata.functionId) {
        status = ZR_ARTIFACT_EXEC_IR_LIMIT;
        goto reject;
    }
    function = ZrCore_ExecIr_ModuleFunctionAt(&temporary, functionId);
    if (function == ZR_NULL) {
        status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
        goto reject;
    }
    function->contract = metadata.functionContract;

    for (TZrUInt32 index = 0u; index < counts.blocks; ++index) {
        SZrExecIrBlock block;
        cursor.offset = layout.blocksOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_SIZE;
        memset(&block, 0, sizeof(block));
        ZrCore_ArtifactExecIrScalarEis5_GetBlock(&cursor, &block);
        if (block.id != index + 1u ||
            (block.flags != 0u &&
             block.flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY) ||
            ((index == 0u) !=
             (block.flags == ZR_EXEC_IR_BLOCK_FLAG_ENTRY))) {
            status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
            goto reject;
        }
        if (ZrCore_ExecIr_FunctionAddBlock(function, block.flags) != block.id) {
            status = ZR_ARTIFACT_EXEC_IR_LIMIT;
            goto reject;
        }
        function->blocks[index].instructions = block.instructions;
        function->blocks[index].predecessors = block.predecessors;
        function->blocks[index].successors = block.successors;
        function->blocks[index].immediateDominator =
                block.immediateDominator;
        function->blocks[index].terminatorInstructionId =
                block.terminatorInstructionId;
    }

    for (TZrUInt32 index = 0u; index < counts.values; ++index) {
        SZrExecIrValue value;
        TZrExecIrValueId valueId;
        cursor.offset = layout.valuesOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE;
        memset(&value, 0, sizeof(value));
        ZrCore_ArtifactExecIrScalarEis5_GetValue(&cursor, &value);
        if (value.id != index + 1u || value.typeToken == 0u ||
            value.ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value.nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            value.flags != 0u || value.definition == 0u ||
            value.definition > counts.instructions) {
            status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
            goto reject;
        }
        definitions[index] = value.definition;
        valueId = ZrCore_ExecIr_FunctionAddValue(
                function, value.typeToken, value.ownership, value.nullability);
        if (valueId != value.id) {
            status = ZR_ARTIFACT_EXEC_IR_LIMIT;
            goto reject;
        }
    }

    cursor.offset = layout.resultsOffset;
    status = eis6_read_pool(&cursor, function, counts.results, ZR_TRUE,
                            diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) goto reject;
    cursor.offset = layout.operandsOffset;
    status = eis6_read_pool(&cursor, function, counts.operands, ZR_FALSE,
                            diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) goto reject;
    cursor.offset = layout.successorsOffset;
    status = eis6_read_block_edges(&cursor, function, counts.successors,
                                   ZR_TRUE, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) goto reject;
    cursor.offset = layout.predecessorsOffset;
    status = eis6_read_block_edges(&cursor, function, counts.predecessors,
                                   ZR_FALSE, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) goto reject;
    cursor.offset = layout.memoryTokensOffset;
    for (TZrUInt32 index = 0u; index < memoryTokenCount; ++index) {
        TZrExecIrMemoryTokenId token =
                ZrCore_ArtifactExecIrScalarEis5_Get32(&cursor);
        if (!ZrCore_ExecIr_FunctionAppendMemoryTokens(
                    function, &token, 1u, ZR_NULL)) {
            status = ZR_ARTIFACT_EXEC_IR_LIMIT;
            goto reject;
        }
    }

    for (TZrUInt32 index = 0u; index < counts.instructions; ++index) {
        SZrExecIrInstruction instruction;
        TZrExecIrInstructionId instructionId;
        cursor.offset = layout.instructionsOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE;
        memset(&instruction, 0, sizeof(instruction));
        ZrCore_ArtifactExecIrScalarEis5_GetInstruction(&cursor,
                                                       &instruction);
        if (instruction.results.start > function->resultCount ||
            instruction.results.count >
                    function->resultCount - instruction.results.start) {
            status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
            goto reject;
        }
        for (TZrUInt32 resultIndex = instruction.results.start;
             resultIndex < instruction.results.start +
                                  instruction.results.count;
             ++resultIndex) {
            TZrExecIrValueId valueId = function->results[resultIndex];
            if (valueId == 0u || valueId > function->valueCount ||
                function->values[valueId - 1u].definition != 0u ||
                (function->values[valueId - 1u].flags &
                 ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u) {
                status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
                goto reject;
            }
        }
        if (!ZrCore_ExecIr_FunctionAppendInstruction(
                    function, &instruction, &instructionId) ||
            instructionId != index + 1u) {
            status = ZR_ARTIFACT_EXEC_IR_LIMIT;
            goto reject;
        }
    }
    for (TZrUInt32 index = 0u; index < counts.values; ++index) {
        if (function->values[index].definition != definitions[index]) {
            status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
            goto reject;
        }
    }

    if (rowCount != 0u) {
        rows = (SZrExecIrBindingRow *)malloc(
                (size_t)rowCount * sizeof(*rows));
        if (rows == ZR_NULL) {
            status = ZR_ARTIFACT_EXEC_IR_LIMIT;
            goto reject;
        }
    }
    cursor.offset = layout.bindingRowsOffset;
    for (TZrUInt32 index = 0u; index < rowCount; ++index) {
        cursor.offset = layout.bindingRowsOffset + index *
                ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_ROW_SIZE;
        ZrCore_ArtifactExecIrScalarEis6_GetBindingRow(&cursor, &rows[index]);
    }
    {
        SZrExecIrFunction rawAssociation = *function;
        rawAssociation.bindingRowsSchemaVersion = schemaVersion;
        rawAssociation.bindingRows = rows;
        rawAssociation.bindingRowCount = rowCount;
        rawAssociation.bindingRowCapacity = rowCount;
        if (!ZrCore_ExecIr_FunctionValidateBindingRows(
                    &rawAssociation, &coreDiagnostic)) {
            status = ZR_ARTIFACT_EXEC_IR_INVALID_SECTION;
            goto reject;
        }
    }
    if (!ZrCore_ExecIr_FunctionSetBindingRows(
                function, rows, rowCount, &coreDiagnostic)) {
        status = ZR_ARTIFACT_EXEC_IR_LIMIT;
        goto reject;
    }
    free(rows);
    rows = ZR_NULL;
    function->sealed = (TZrBool)metadata.sealed;

    status = ZrCore_ArtifactExecIrScalarEis6_ValidateModule(
            &temporary, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) goto reject;
    *outModule = temporary;
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);

reject:
    free(rows);
    ZrCore_ExecIr_FreeModule(&temporary);
    return ZrCore_ArtifactExecIrScalarEis6_Fail(diagnostic, status,
                                                cursor.offset);
}
