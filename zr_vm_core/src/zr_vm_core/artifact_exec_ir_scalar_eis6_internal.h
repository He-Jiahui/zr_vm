#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS6_INTERNAL_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS6_INTERNAL_H

#include "artifact_exec_ir_scalar_eis5_internal.h"
#include "artifact_exec_ir_scalar_eis6.h"

#define ZR_ARTIFACT_EXEC_IR_EIS6_MEMORY_TOKEN_COUNT_OFFSET ((TZrUInt32)44u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_SCHEMA_OFFSET ((TZrUInt32)48u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_BINDING_ROW_COUNT_OFFSET ((TZrUInt32)52u)

#define ZR_ARTIFACT_EXEC_IR_EIS6_MODULE_ID_OFFSET ((TZrUInt32)56u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_MODULE_TOKEN_OFFSET ((TZrUInt32)60u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_MODULE_HASH_OFFSET ((TZrUInt32)64u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_MODULE_CONTRACT_OFFSET ((TZrUInt32)72u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_FUNCTION_ID_OFFSET ((TZrUInt32)136u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_FUNCTION_TOKEN_OFFSET ((TZrUInt32)140u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_SIGNATURE_HASH_OFFSET ((TZrUInt32)144u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_ENTRY_BLOCK_OFFSET ((TZrUInt32)152u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_SEALED_OFFSET ((TZrUInt32)156u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_FUNCTION_CONTRACT_OFFSET ((TZrUInt32)160u)

#define ZR_ARTIFACT_EXEC_IR_EIS6_MEMORY_TOKEN_SIZE ((TZrUInt32)4u)
#define ZR_ARTIFACT_EXEC_IR_EIS6_MAX_ROWS \
        ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES

typedef struct SEis6Layout {
    SEis5Counts counts;
    SEis5Layout graph;
    TZrUInt32 memoryTokenCount;
    TZrUInt32 bindingRowCount;
    TZrUInt32 constantsOffset;
    TZrUInt32 valuesOffset;
    TZrUInt32 blocksOffset;
    TZrUInt32 instructionsOffset;
    TZrUInt32 resultsOffset;
    TZrUInt32 operandsOffset;
    TZrUInt32 successorsOffset;
    TZrUInt32 predecessorsOffset;
    TZrUInt32 memoryTokensOffset;
    TZrUInt32 bindingRowsOffset;
    TZrUInt32 totalSize;
} SEis6Layout;

typedef struct SEis6Metadata {
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
} SEis6Metadata;

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_Fail(
        SZrArtifactExecIrDiagnostic *diagnostic,
        EZrArtifactExecIrStatus status, TZrUInt32 offset);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_ComputeLayout(
        const SEis5Counts *counts, TZrUInt32 memoryTokenCount,
        TZrUInt32 bindingRowCount, SEis6Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis6_ValidateModule(
        const SZrExecIrModule *module, SEis6Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic);
void ZrCore_ArtifactExecIrScalarEis6_GetMetadata(
        SEis5Cursor *cursor, SEis6Metadata *metadata);
void ZrCore_ArtifactExecIrScalarEis6_PutMetadata(
        SEis5Cursor *cursor, const SEis6Metadata *metadata);
void ZrCore_ArtifactExecIrScalarEis6_GetBindingRow(
        SEis5Cursor *cursor, SZrExecIrBindingRow *row);
void ZrCore_ArtifactExecIrScalarEis6_PutBindingRow(
        SEis5Cursor *cursor, const SZrExecIrBindingRow *row);

#endif
