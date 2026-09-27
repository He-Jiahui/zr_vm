#ifndef ZR_VM_PARSER_EXEC_IR_EXECBC_H
#define ZR_VM_PARSER_EXEC_IR_EXECBC_H

#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/exec_ir_projections.h"

typedef TZrBool (*FZrExecBcMemory)(
        void *userData, const SZrExecBcInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

typedef TZrBool (*FZrExecBcPlace)(
        void *userData, const SZrExecBcInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

typedef TZrBool (*FZrExecBcCall)(
        void *userData, const SZrExecBcInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

typedef TZrBool (*FZrExecBcInvoke)(
        void *userData, const SZrExecBcInstruction *instruction,
        const SZrExecIrOracleValue *operands, TZrUInt32 operandCount,
        SZrExecIrOracleValue *result, TZrBool *threw);

typedef TZrBool (*FZrExecBcExceptionPayload)(
        void *userData, const SZrExecBcInstruction *instruction,
        SZrExecIrOracleValue *result);

typedef struct SZrExecBcExecutionInput {
    const SZrExecIrOracleValue *initialValues;
    TZrUInt32 initialValueCount;
    const SZrExecIrOracleValue *constants;
    TZrUInt32 constantCount;
    TZrUInt32 maxSteps;
    FZrExecBcMemory memory;
    void *memoryUserData;
    FZrExecBcCall call;
    void *callUserData;
    FZrExecBcInvoke invoke;
    void *invokeUserData;
    FZrExecBcExceptionPayload exceptionPayload;
    void *exceptionPayloadUserData;
    FZrExecBcPlace place;
    void *placeUserData;
} SZrExecBcExecutionInput;

typedef struct SZrExecBcExecutionResult {
    SZrExecIrOracleValue *slots;
    TZrUInt32 slotCount;
    TZrUInt32 *ownerStates; /* indexed by physical slot, including phi temp */
    TZrUInt32 executedInstructionCount;
    TZrExecIrBlockId currentBlock;
    TZrExecIrInstructionId returnInstructionId;
    TZrExecIrSourceId returnSourceId;
    SZrExecIrOracleEvent *events;
    TZrUInt32 eventCount;
    TZrUInt32 eventCapacity;
    SZrExecIrOracleValue returnValue;
    TZrBool returned;
    TZrBool terminatedByThrow;
    TZrBool suspended;
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
