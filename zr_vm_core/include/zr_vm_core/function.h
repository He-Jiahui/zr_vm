//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_FUNCTION_H
#define ZR_VM_CORE_FUNCTION_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
struct SZrState;
struct SZrTypeValueOnStack;
struct SZrString;
struct SZrObjectPrototype;

struct ZR_STRUCT_ALIGN SZrFunctionClosureVariable {
    struct SZrString *name;
    TBool inStack;
    TUInt32 index;
    EZrValueType valueType;
};

typedef struct SZrFunctionClosureVariable SZrFunctionClosureVariable;

struct ZR_STRUCT_ALIGN SZrFunctionLocalVariable {
    struct SZrString *name;
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
    // function name (函数名由函数自身持有，匿名函数为 ZR_NULL)
    struct SZrString *functionName;
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
    struct SZrString *sourceCodeList;
    SZrRawObject *gcList;
    
    // module export info (for script-level functions only)
    // 导出变量信息（用于模块导出）
    struct SZrFunctionExportedVariable {
        struct SZrString *name;                    // 变量名
        TUInt32 stackSlot;                          // 栈槽位
        TUInt8 accessModifier;                      // 可见性修饰符 (0=PRIVATE, 1=PUBLIC, 2=PROTECTED)
    } *exportedVariables;                           // 导出变量数组
    TUInt32 exportedVariableLength;                 // 导出变量数量
    
    // prototype数据存储（从常量池迁移）
    TByte *prototypeData;                           // prototype 二进制数据（序列化后的 SZrCompiledPrototypeInfo 数组）
    TUInt32 prototypeDataLength;                    // prototype 数据长度（字节数）
    TUInt32 prototypeCount;                         // prototype 数量
    struct SZrObjectPrototype **prototypeInstances; // 运行时实例化的prototype对象指针数组
    TUInt32 prototypeInstancesLength;               // prototype实例数组长度
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

ZR_CORE_API struct SZrString *ZrFunctionGetLocalVariableName(SZrFunction *function, TUInt32 index,
                                                             TUInt32 programCounter);

ZR_CORE_API TZrStackValuePointer ZrFunctionCheckStack(struct SZrState *state, TZrSize size,
                                                      TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrFunctionCheckNativeStack(struct SZrState *state);

ZR_CORE_API TZrStackValuePointer ZrFunctionCheckStackAndGc(struct SZrState *state, TZrSize size,
                                                           TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrFunctionCall(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount);

ZR_CORE_API void ZrFunctionCallWithoutYield(struct SZrState *state, TZrStackValuePointer stackPointer,
                                            TZrSize resultCount);

ZR_CORE_API struct SZrCallInfo *ZrFunctionPreCall(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                  TZrSize resultCount, TZrStackValuePointer returnDestination);

ZR_CORE_API void ZrFunctionPostCall(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount);
#endif // ZR_VM_CORE_FUNCTION_H
