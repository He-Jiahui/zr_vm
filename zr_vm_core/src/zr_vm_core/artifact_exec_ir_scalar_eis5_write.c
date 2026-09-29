#include "artifact_exec_ir_scalar_eis5_internal.h"

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_Write(
        const SZrExecIrModule *module, TZrByte *bytes, TZrUInt32 capacity,
        SZrArtifactExecIrDiagnostic *diagnostic) {
    SEis5Cursor cursor;
    SEis5Layout layout;
    EZrArtifactExecIrStatus status;

    if (module == ZR_NULL || bytes == ZR_NULL)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT, 0u);
    status = ZrCore_ArtifactExecIrScalarEis5_ValidateModule(
            module, &layout, diagnostic);
    if (status != ZR_ARTIFACT_EXEC_IR_OK) return status;
    if (capacity < layout.totalSize)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_TRUNCATED, 0u);

    cursor.bytes = bytes;
    cursor.offset = 0u;
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            &cursor, ZR_ARTIFACT_EXEC_IR_EIS5_MAGIC);
    ZrCore_ArtifactExecIrScalarEis5_Put16(
            &cursor, ZR_ARTIFACT_EXEC_IR_EIS5_VERSION);
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

    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, module->id);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor, module->moduleToken);
    ZrCore_ArtifactExecIrScalarEis5_Put64(&cursor, module->moduleHash);
    ZrCore_ArtifactExecIrScalarEis5_PutContract(&cursor, &module->contract);
    ZrCore_ArtifactExecIrScalarEis5_Put32(&cursor,
                                           module->functions[0].id);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            &cursor, module->functions[0].functionToken);
    ZrCore_ArtifactExecIrScalarEis5_Put64(
            &cursor, module->functions[0].signatureHash);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            &cursor, module->functions[0].entryBlockId);
    ZrCore_ArtifactExecIrScalarEis5_Put32(
            &cursor, module->functions[0].sealed ? 1u : 0u);
    ZrCore_ArtifactExecIrScalarEis5_PutContract(
            &cursor, &module->functions[0].contract);

    for (TZrUInt32 index = 0u; index < layout.counts.constants; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutConstant(
                &cursor, &module->constants[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.values; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutValue(
                &cursor, &module->functions[0].values[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.blocks; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutBlock(
                &cursor, &module->functions[0].blocks[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.instructions; ++index)
        ZrCore_ArtifactExecIrScalarEis5_PutInstruction(
                &cursor, &module->functions[0].instructions[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.results; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(
                &cursor, module->functions[0].results[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.operands; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(
                &cursor, module->functions[0].operands[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.successors; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(
                &cursor, module->functions[0].successors[index]);
    for (TZrUInt32 index = 0u; index < layout.counts.predecessors; ++index)
        ZrCore_ArtifactExecIrScalarEis5_Put32(
                &cursor, module->functions[0].predecessors[index]);

    if (cursor.offset != layout.totalSize)
        return ZrCore_ArtifactExecIrScalarEis5_Fail(
                diagnostic, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                cursor.offset);
    return ZrCore_ArtifactExecIrScalarEis5_Fail(
            diagnostic, ZR_ARTIFACT_EXEC_IR_OK, 0u);
}
