#ifndef ZR_VM_PARSER_EXEC_IR_EXECBC_H
#define ZR_VM_PARSER_EXEC_IR_EXECBC_H

#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_projections.h"

typedef struct SZrExecBcExecutionInput {
    const SZrExecIrOracleValue *initialValues;
    TZrUInt32 initialValueCount;
    const SZrExecIrOracleValue *constants;
    TZrUInt32 constantCount;
    TZrUInt32 maxSteps;
} SZrExecBcExecutionInput;

typedef struct SZrExecBcExecutionResult {
    SZrExecIrOracleValue *slots;
    TZrUInt32 slotCount;
    TZrUInt32 executedInstructionCount;
    TZrExecIrBlockId currentBlock;
    TZrExecIrInstructionId returnInstructionId;
    TZrExecIrSourceId returnSourceId;
    SZrExecIrOracleValue returnValue;
    TZrBool returned;
    TZrUInt32 ownershipTag;
} SZrExecBcExecutionResult;

#define ZR_EXEC_BC_EXECUTION_RESULT_TAG ((TZrUInt32)0x45425831u)

ZR_PARSER_API void ZrParser_ExecBcExecutionResult_Init(
        SZrExecBcExecutionResult *result);
ZR_PARSER_API void ZrParser_ExecBcExecutionResult_Free(
        SZrExecBcExecutionResult *result);
ZR_PARSER_API TZrBool ZrParser_ExecBcProjection_Run(
        const SZrExecBcProjection *projection,
        const SZrExecBcExecutionInput *input,
        SZrExecBcExecutionResult *result,
        SZrExecIrDiagnostic *diagnostic);

#endif
