//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_FUNCTION_H
#define ZR_VM_CORE_FUNCTION_H

#include "zr_vm_core/value.h"
#include "zr_vm_core/conf.h"

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
    TUInt32 instructionLength;
    TUInt32 closureVariableLength;
    TUInt32 constantVariableLength;
    TUInt32 localVariableLength;
    TUInt32 childFunctionLength;
    TUInt32 executionLocationInfoLength;
    // debug
    TUInt32 lineInSourceStart;
    TUInt32 lineInSourceEnd;
    // instructions
    TZrInstruction *instructionList;
    // variables
    SZrFunctionClosureVariable *closureVariableList;
    SZrTypeValue *constantVariableList;
    SZrFunctionLocalVariable *localVariableList;
    struct SZrFunction *childFunctionList;
    // function debug info
    SZrFunctionExecutionLocationInfo *executionLocationInfoList;
    TUInt32 *lineInSourceList;
    TZrString *sourceCodeList;
    SZrRawObject *gcList;
};

typedef struct SZrFunction SZrFunction;
#endif //ZR_VM_CORE_FUNCTION_H
