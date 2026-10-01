#include "artifact_exec_ir_scalar_eis6_internal.h"

#include <stdlib.h>
#include <string.h>

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis6Layout layout;
    SEis6Metadata metadata;
    SEis5Cursor cursor;
    TZrByte *temporary;
    EZrArtifactExecIrStatus status;
    const SZrExecIrFunction *function;
    if (module == ZR_NULL || bytes == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalarEis6_ValidateModule(
            module, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (capacity < layout.totalSize)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);
    temporary = (TZrByte *)malloc((size_t)layout.totalSize);
    if (temporary == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_LIMIT, 0u);

    function = &module->functions[0];
    memset(&metadata, 0, sizeof(metadata));
    metadata.moduleId = module->id;
    metadata.moduleToken = module->moduleToken;
    metadata.moduleHash = module->moduleHash;
    metadata.moduleContract = module->contract;
    metadata.functionId = function->id;
    metadata.functionToken = function->functionToken;
    metadata.signatureHash = function->signatureHash;
    metadata.entryBlockId = function->entryBlockId;
    metadata.sealed = (TZrUInt32)function->sealed;
    metadata.functionContract = function->contract;

    cursor.bytes = temporary;
    cursor.offset = 0u;
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            &cursor, ZR_ARTIFACT_EXEC_IR_EIS6_MAGIC);
    ZrCore_ArtifactExecIrScalarEis5_Put16(
            &cursor, ZR_ARTIFACT_EXEC_IR_EIS6_VERSION);
    ZrCore_ArtifactExecIrScalarEis5_Put16(&cursor, 0u);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, layout.totalSize);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, layout.counts.constants);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, layout.counts.values);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, layout.counts.blocks);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                          layout.counts.instructions);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, layout.counts.operands);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, layout.counts.results);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                          layout.counts.successors);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                          layout.counts.predecessors);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                          layout.memoryTokenCount);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            &cursor, ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                          layout.bindingRowCount);
    ZrCore_ArtifactExecIrScalarEis6_PutMetadata(
            &cursor, &metadata);

    cursor.offset = layout.constantsOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.constants; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutConstant(
                &cursor, &module->constants[index]);
    cursor.offset = layout.valuesOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.values; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutValue(
                &cursor, &function->values[index]);
    cursor.offset = layout.blocksOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.blocks; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutBlock(
                &cursor, &function->blocks[index]);
    cursor.offset = layout.instructionsOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.instructions; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutInstruction(
                &cursor, &function->instructions[index]);
    cursor.offset = layout.resultsOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.results; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                              function->results[index]);
    cursor.offset = layout.operandsOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.operands; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                              function->operands[index]);
    cursor.offset = layout.successorsOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.successors; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                              function->successors[index]);
    cursor.offset = layout.predecessorsOffset;
    for (TZrUInt32 index = 0u; index < layout.counts.predecessors; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                              function->predecessors[index]);
    cursor.offset = layout.memoryTokensOffset;
    for (TZrUInt32 index = 0u; index < layout.memoryTokenCount; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(
                &cursor, function->memoryTokenPool[index]);
    cursor.offset = layout.bindingRowsOffset;
    for (TZrUInt32 index = 0u; index < layout.bindingRowCount; ++index)
        ZrCore_ArtifactExecIrScalarEis6_PutBindingRow(
                &cursor, &function->bindingRows[index]);

    if (cursor.offset != layout.totalSize) {
        free(temporary);
        return ZrCore_ArtifactExecIrScalarEis6_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                cursor.offset);
    }
    memcpy(bytes, temporary, layout.totalSize);
    free(temporary);
    return ZrCore_ArtifactExecIrScalarEis6_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}
