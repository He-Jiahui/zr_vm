//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_FUNCTION_H
#define ZR_VM_CORE_FUNCTION_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
struct SZrState;
struct SZrTypeValueOnStack;

struct ZR_STRUCT_ALIGN SZrFunctionClosureVariable {
    TZrString *name;
    TBool inStack;
    TUInt32 index;
    EZrValueType valueType;
};

typedef struct SZrFunctionClosureVariable SZrFunctionClosureVariable;

struct ZR_STRUCT_ALIGN SZrFunctionLocalVariable {
    TZrString *name;
    TZrMemoryOffset offsetActivate;
    TZrMemoryOffset offsetDead;
};

typedef struct SZrFunctionLocalVariable SZrFunctionLocalVariable;

struct ZR_STRUCT_ALIGN SZrFunctionExecutionLocationInfo {
    TZrMemoryOffset currentInstructionOffset;
    TUInt32 lineInSource;
};

typedef struct SZrFunctionExecutionLocationInfo SZrFunctionExecutionLocationInfo;

struct ZR_STRUCT_ALIGN SZrFunction {
    SZrRawObject super;
    TUInt16 parameterCount;
    TBool hasVariableArguments;
    TUInt32 stackSize;
    // length
    TUInt32 instructionsLength;
    TUInt32 closureValueLength;
    TUInt32 constantValueLength;
    TUInt32 localVariableLength;
    TUInt32 childFunctionLength;
    TUInt32 executionLocationInfoLength;
    // debug
    TUInt32 lineInSourceStart;
    TUInt32 lineInSourceEnd;
    // instructions
    TZrInstruction *instructionsList;
    // variables
    SZrFunctionClosureVariable *closureValueList;
    SZrTypeValue *constantValueList;
    SZrFunctionLocalVariable *localVariableList;
    struct SZrFunction *childFunctionList;
    // function debug info
    SZrFunctionExecutionLocationInfo *executionLocationInfoList;
    TUInt32 *lineInSourceList;
    TZrString *sourceCodeList;
    SZrRawObject *gcList;
};

typedef struct SZrFunction SZrFunction;

// struct ZR_STRUCT_ALIGN SZrFunctionOverload {
//     TZrSize functionOverloadsLength;
//     SZrFunction **functionOverloads;
// };
//
// typedef struct SZrFunctionOverload SZrFunctionOverload;

ZR_CORE_API SZrFunction *ZrFunctionNew(struct SZrState *state);

ZR_CORE_API void ZrFunctionFree(struct SZrState *state, SZrFunction *function);

ZR_CORE_API TZrString *ZrFunctionGetLocalVariableName(SZrFunction *function, TUInt32 index, TUInt32 programCounter);

ZR_CORE_API TZrStackValuePointer ZrFunctionCheckStack(struct SZrState *state, TZrSize size,
                                                      TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrFunctionCheckNativeStack(struct SZrState *state);

ZR_CORE_API TZrStackValuePointer ZrFunctionCheckStackAndGc(struct SZrState *state, TZrSize size,
                                                           TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrFunctionCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount);

ZR_CORE_API void ZrFunctionCallWithoutYield(struct SZrState *state, TZrStackValuePointer stackPointer,
                                            TZrSize resultCount);

ZR_CORE_API struct SZrCallInfo *ZrFunctionPreCall(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                  TZrSize resultCount);

#endif // ZR_VM_CORE_FUNCTION_H
