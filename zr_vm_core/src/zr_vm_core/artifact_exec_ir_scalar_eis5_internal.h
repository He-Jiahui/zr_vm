#ifndef ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS5_INTERNAL_H
#define ZR_VM_CORE_ARTIFACT_EXEC_IR_SCALAR_EIS5_INTERNAL_H

#include "artifact_exec_ir_scalar_eis5.h"

#include <stdint.h>

#define ZR_ARTIFACT_EXEC_IR_EIS5_FIXED_PREFIX_SIZE ((TZrUInt32)168u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BLOCKS ((TZrUInt32)256u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MAX_SCALAR_NODES ((TZrUInt32)4096u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MAX_POOL_ITEMS ((TZrUInt32)16384u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MAX_BYTES \
        ZR_ARTIFACT_EXEC_IR_EIS5_MAX_ENCODED_SIZE

#define ZR_ARTIFACT_EXEC_IR_EIS5_VERSION_OFFSET ((TZrUInt32)4u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_RESERVED_OFFSET ((TZrUInt32)6u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_LENGTH_OFFSET ((TZrUInt32)8u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_COUNT_OFFSET ((TZrUInt32)12u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_COUNT_OFFSET ((TZrUInt32)16u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_COUNT_OFFSET ((TZrUInt32)20u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_COUNT_OFFSET ((TZrUInt32)24u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_OPERAND_COUNT_OFFSET ((TZrUInt32)28u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_RESULT_COUNT_OFFSET ((TZrUInt32)32u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_SUCCESSOR_COUNT_OFFSET ((TZrUInt32)36u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_PREDECESSOR_COUNT_OFFSET ((TZrUInt32)40u)

#define ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_ID_OFFSET ((TZrUInt32)44u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_TOKEN_OFFSET ((TZrUInt32)48u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_HASH_OFFSET ((TZrUInt32)52u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_MODULE_CONTRACT_OFFSET ((TZrUInt32)60u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_FUNCTION_ID_OFFSET ((TZrUInt32)124u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_FUNCTION_TOKEN_OFFSET ((TZrUInt32)128u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_SIGNATURE_HASH_OFFSET ((TZrUInt32)132u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_ENTRY_BLOCK_OFFSET ((TZrUInt32)140u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_SEALED_OFFSET ((TZrUInt32)144u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_FUNCTION_CONTRACT_OFFSET ((TZrUInt32)148u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_TYPE_TOKEN_OFFSET ((TZrUInt32)36u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_FLAGS_OFFSET ((TZrUInt32)2u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_EFFECT_IN_OFFSET ((TZrUInt32)64u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_EFFECT_OUT_OFFSET ((TZrUInt32)68u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_EQ ((TZrUInt32)0u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_LT ((TZrUInt32)1u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_LE ((TZrUInt32)2u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_GT ((TZrUInt32)3u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_GE ((TZrUInt32)4u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_COMPARE_MODE_NE ((TZrUInt32)5u)

#define ZR_ARTIFACT_EXEC_IR_EIS5_CONSTANT_SIZE ((TZrUInt32)16u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_VALUE_SIZE ((TZrUInt32)24u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_BLOCK_SIZE ((TZrUInt32)40u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_INSTRUCTION_SIZE ((TZrUInt32)84u)
#define ZR_ARTIFACT_EXEC_IR_EIS5_POOL_ID_SIZE ((TZrUInt32)4u)

typedef struct SEis5Counts {
    TZrUInt32 constants;
    TZrUInt32 values;
    TZrUInt32 blocks;
    TZrUInt32 instructions;
    TZrUInt32 operands;
    TZrUInt32 results;
    TZrUInt32 successors;
    TZrUInt32 predecessors;
} SEis5Counts;

typedef struct SEis5Layout {
    SEis5Counts counts;
    TZrUInt32 constantsOffset;
    TZrUInt32 valuesOffset;
    TZrUInt32 blocksOffset;
    TZrUInt32 instructionsOffset;
    TZrUInt32 resultsOffset;
    TZrUInt32 operandsOffset;
    TZrUInt32 successorsOffset;
    TZrUInt32 predecessorsOffset;
    TZrUInt32 totalSize;
} SEis5Layout;

typedef struct SEis5Cursor {
    TZrByte *bytes;
    TZrUInt32 offset;
} SEis5Cursor;

EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_Fail(
        SZrArtifactExecIrDiagnostic *diagnostic,
        EZrArtifactExecIrStatus status, TZrUInt32 offset);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_ComputeLayout(
        const SEis5Counts *counts, SEis5Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic);
EZrArtifactExecIrStatus ZrCore_ArtifactExecIrScalarEis5_ValidateModule(
        const SZrExecIrModule *module, SEis5Layout *layout,
        SZrArtifactExecIrDiagnostic *diagnostic);

TZrUInt16 ZrCore_ArtifactExecIrScalarEis5_Get16(SEis5Cursor *cursor);
TZrUInt32 ZrCore_ArtifactExecIrScalarEis5_Get32(SEis5Cursor *cursor);
TZrUInt64 ZrCore_ArtifactExecIrScalarEis5_Get64(SEis5Cursor *cursor);
void ZrCore_ArtifactExecIrScalarEis5_Put16(SEis5Cursor *cursor,
                                            TZrUInt16 value);
void ZrCore_ArtifactExecIrScalarEis5_Put32(SEis5Cursor *cursor,
                                            TZrUInt32 value);
void ZrCore_ArtifactExecIrScalarEis5_Put64(SEis5Cursor *cursor,
                                            TZrUInt64 value);
void ZrCore_ArtifactExecIrScalarEis5_GetContract(
        SEis5Cursor *cursor, SZrExecutionContract *contract);
void ZrCore_ArtifactExecIrScalarEis5_PutContract(
        SEis5Cursor *cursor, const SZrExecutionContract *contract);
SZrExecIrRange ZrCore_ArtifactExecIrScalarEis5_GetRange(
        SEis5Cursor *cursor);
void ZrCore_ArtifactExecIrScalarEis5_PutRange(
        SEis5Cursor *cursor, SZrExecIrRange range);
void ZrCore_ArtifactExecIrScalarEis5_GetConstant(
        SEis5Cursor *cursor, SZrExecIrConstant *constant);
void ZrCore_ArtifactExecIrScalarEis5_PutConstant(
        SEis5Cursor *cursor, const SZrExecIrConstant *constant);
void ZrCore_ArtifactExecIrScalarEis5_GetValue(
        SEis5Cursor *cursor, SZrExecIrValue *value);
void ZrCore_ArtifactExecIrScalarEis5_PutValue(
        SEis5Cursor *cursor, const SZrExecIrValue *value);
void ZrCore_ArtifactExecIrScalarEis5_GetBlock(
        SEis5Cursor *cursor, SZrExecIrBlock *block);
void ZrCore_ArtifactExecIrScalarEis5_PutBlock(
        SEis5Cursor *cursor, const SZrExecIrBlock *block);
void ZrCore_ArtifactExecIrScalarEis5_GetInstruction(
        SEis5Cursor *cursor, SZrExecIrInstruction *instruction);
void ZrCore_ArtifactExecIrScalarEis5_PutInstruction(
        SEis5Cursor *cursor, const SZrExecIrInstruction *instruction);

#endif
