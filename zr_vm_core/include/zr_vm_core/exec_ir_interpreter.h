#ifndef ZR_VM_CORE_EXEC_IR_INTERPRETER_H
#define ZR_VM_CORE_EXEC_IR_INTERPRETER_H

#include "zr_vm_core/exec_ir.h"

/*
 * A deliberately small, pointer-free value model for the reference
 * interpreter.  Runtime SZrTypeValue is not required here: the oracle is
 * also used by parser-only and cross-process differential tests.
 */
typedef enum EZrExecIrOracleValueKind {
    ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED = 0,
    ZR_EXEC_IR_ORACLE_VALUE_BOOL,
    ZR_EXEC_IR_ORACLE_VALUE_SIGNED,
    ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED,
    ZR_EXEC_IR_ORACLE_VALUE_FLOAT,
    ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT
} EZrExecIrOracleValueKind;

typedef struct SZrExecIrOracleValue {
    EZrExecIrOracleValueKind kind;
    union {
        TZrBool boolean;
        TZrInt64 signedInteger;
        TZrUInt64 unsignedInteger;
        TZrFloat64 floating;
    } as;
} SZrExecIrOracleValue;

/* Memory is intentionally supplied by the caller: the oracle must not
 * reinterpret an ExecIR value as a host pointer or invent a process-global
 * heap.  A LOAD receives one address operand and writes result; a STORE
 * receives address/value operands and must leave result untouched (NULL). */
typedef enum EZrExecIrOracleMemoryOperation {
    ZR_EXEC_IR_ORACLE_MEMORY_LOAD = 0,
    ZR_EXEC_IR_ORACLE_MEMORY_STORE,
    ZR_EXEC_IR_ORACLE_MEMORY_OPERATION_COUNT
} EZrExecIrOracleMemoryOperation;

typedef enum EZrExecIrOracleEventKind {
    ZR_EXEC_IR_ORACLE_EVENT_CALL = 0,
    ZR_EXEC_IR_ORACLE_EVENT_STORE,
    ZR_EXEC_IR_ORACLE_EVENT_THROW,
    ZR_EXEC_IR_ORACLE_EVENT_DROP,
    ZR_EXEC_IR_ORACLE_EVENT_BARRIER,
    ZR_EXEC_IR_ORACLE_EVENT_SUSPEND,
    ZR_EXEC_IR_ORACLE_EVENT_LOAD,
    ZR_EXEC_IR_ORACLE_EVENT_ALLOCATE,
    ZR_EXEC_IR_ORACLE_EVENT_ITERATOR,
    ZR_EXEC_IR_ORACLE_EVENT_KIND_COUNT
} EZrExecIrOracleEventKind;

#define ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT ((TZrUInt32)4u)

typedef struct SZrExecIrOracleEvent {
    EZrExecIrOracleEventKind kind;
    TZrExecIrInstructionId instructionId;
    TZrExecIrSourceId sourceId;
    TZrUInt32 operandCount;
    SZrExecIrOracleValue operands[ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT];
} SZrExecIrOracleEvent;

struct SZrExecIrOracleInput;
typedef TZrBool (*FZrExecIrOracleCall)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

/* INVOKE has two ordered successors: normal first and exceptional second.
 * The callback supplies a pointer-free result and explicitly selects the
 * exceptional continuation without exposing a runtime exception object. */
typedef TZrBool (*FZrExecIrOracleInvoke)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result,
        TZrBool *threw);

/* Iterator operations have the same ordered normal/exception edge shape as
 * INVOKE, but remain a distinct event/provider family for differential tests. */
typedef TZrBool (*FZrExecIrOracleIterator)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result,
        TZrBool *threw);

/* Place construction remains pointer-free: the provider maps the stable
 * operand/descriptor tokens to a caller-owned address token. */
typedef TZrBool (*FZrExecIrOraclePlace)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

typedef TZrBool (*FZrExecIrOracleMemory)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        EZrExecIrOracleMemoryOperation operation,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

/* Allocation is intentionally caller-owned and pointer-free.  The callback
 * receives the allocation's bounded constructor operands and returns a
 * scalar/token value that can be carried by ExecIR; it must not encode a host
 * address or publish an allocation event on behalf of the oracle. */
typedef TZrBool (*FZrExecIrOracleAllocate)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount,
        SZrExecIrOracleValue *result);

/* Runtime subtype identity stays outside the pointer-free oracle value.  The
 * caller supplies the canonical membership operation and returns only the
 * boolean result; a rejected query is reported distinctly from a false
 * membership result. */
typedef TZrBool (*FZrExecIrOracleTypeTest)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        const SZrExecIrOracleValue *value,
        TZrExecIrTypeToken matchTypeToken,
        TZrBool *result);

/* The active exception payload is supplied by the caller's handler context;
 * the pointer-free Oracle never reaches into a VM exception object. */
typedef TZrBool (*FZrExecIrOracleExceptionPayload)(
        void *userData,
        const SZrExecIrInstruction *instruction,
        SZrExecIrOracleValue *result);

typedef struct SZrExecIrOracleInput {
    const SZrExecIrFunction *function;
    /* Initial values are indexed by valueId - 1. */
    const SZrExecIrOracleValue *initialValues;
    TZrUInt32 initialValueCount;
    /* CONSTANT.layoutId is an index into this optional pool. */
    const SZrExecIrOracleValue *constants;
    TZrUInt32 constantCount;
    TZrUInt32 maxSteps;
    FZrExecIrOracleCall call;
    void *userData;
    FZrExecIrOracleMemory memory;
    void *memoryUserData;
    FZrExecIrOracleAllocate allocate;
    void *allocateUserData;
    FZrExecIrOracleTypeTest typeTest;
    void *typeTestUserData;
    FZrExecIrOracleExceptionPayload exceptionPayload;
    void *exceptionPayloadUserData;
    FZrExecIrOracleInvoke invoke;
    void *invokeUserData;
    FZrExecIrOracleIterator iterator;
    void *iteratorUserData;
    FZrExecIrOraclePlace place;
    void *placeUserData;
} SZrExecIrOracleInput;

typedef struct SZrExecIrOracleExecutionResult {
    TZrUInt32 instructionCount;
    TZrUInt32 executedInstructionCount;
    TZrUInt32 supportedInstructionCount;
    TZrUInt32 unsupportedInstructionId;
    TZrExecIrBlockId currentBlock;
    SZrExecIrOracleValue returnValue;
    TZrBool returned;
    TZrBool terminatedByThrow;
    TZrBool suspended;
    SZrExecIrOracleValue *values; /* valueCount entries, valueId - 1 indexed */
    TZrUInt32 valueCount;
    TZrUInt32 valueCapacity;
    SZrExecIrOracleEvent *events;
    TZrUInt32 eventCount;
    TZrUInt32 eventCapacity;
    /* Set by Init/RunOracleEx so Free and transactional replacement can
     * distinguish API-owned storage from a merely zeroed record. */
    TZrUInt32 ownershipTag;
} SZrExecIrOracleExecutionResult;

#define ZR_EXEC_IR_ORACLE_RESULT_TAG ((TZrUInt32)0x4f52434cu)

ZR_CORE_API void ZrCore_ExecIr_OracleResultInit(
        SZrExecIrOracleExecutionResult *result);
ZR_CORE_API void ZrCore_ExecIr_OracleResultFree(
        SZrExecIrOracleExecutionResult *result);
ZR_CORE_API TZrBool ZrCore_ExecIr_RunOracleEx(
        const SZrExecIrOracleInput *input,
        SZrExecIrOracleExecutionResult *result,
        SZrExecIrDiagnostic *diagnostic);

#endif
