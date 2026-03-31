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

struct ZR_STRUCT_ALIGN SZrFunctionStackAnchor {
    TZrMemoryOffset offset;
};

typedef struct SZrFunctionStackAnchor SZrFunctionStackAnchor;

struct ZR_STRUCT_ALIGN SZrFunctionClosureVariable {
    struct SZrString *name;
    TZrBool inStack;
    TZrUInt32 index;
    EZrValueType valueType;
};

typedef struct SZrFunctionClosureVariable SZrFunctionClosureVariable;

struct ZR_STRUCT_ALIGN SZrFunctionLocalVariable {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrMemoryOffset offsetActivate;
    TZrMemoryOffset offsetDead;
};

typedef struct SZrFunctionLocalVariable SZrFunctionLocalVariable;

struct ZR_STRUCT_ALIGN SZrFunctionExecutionLocationInfo {
    TZrMemoryOffset currentInstructionOffset;
    TZrUInt32 lineInSource;
};

typedef struct SZrFunctionExecutionLocationInfo SZrFunctionExecutionLocationInfo;

struct ZR_STRUCT_ALIGN SZrFunctionCatchClauseInfo {
    struct SZrString *typeName;
    TZrMemoryOffset targetInstructionOffset;
};

typedef struct SZrFunctionCatchClauseInfo SZrFunctionCatchClauseInfo;

struct ZR_STRUCT_ALIGN SZrFunctionExceptionHandlerInfo {
    TZrMemoryOffset protectedStartInstructionOffset;
    TZrMemoryOffset finallyTargetInstructionOffset;
    TZrMemoryOffset afterFinallyInstructionOffset;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 catchClauseCount;
    TZrBool hasFinally;
};

typedef struct SZrFunctionExceptionHandlerInfo SZrFunctionExceptionHandlerInfo;

typedef enum EZrFunctionTypedSymbolKind {
    ZR_FUNCTION_TYPED_SYMBOL_VARIABLE = 1,
    ZR_FUNCTION_TYPED_SYMBOL_FUNCTION = 2
} EZrFunctionTypedSymbolKind;

typedef struct SZrFunctionTypedTypeRef {
    EZrValueType baseType;
    TZrBool isNullable;
    TZrUInt32 ownershipQualifier;
    TZrBool isArray;
    struct SZrString *typeName;
    EZrValueType elementBaseType;
    struct SZrString *elementTypeName;
} SZrFunctionTypedTypeRef;

typedef struct SZrFunctionTypedLocalBinding {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    SZrFunctionTypedTypeRef type;
} SZrFunctionTypedLocalBinding;

typedef struct SZrFunctionTypedExportSymbol {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
    TZrUInt8 symbolKind;
    SZrFunctionTypedTypeRef valueType;
    TZrUInt32 parameterCount;
    SZrFunctionTypedTypeRef *parameterTypes;
} SZrFunctionTypedExportSymbol;

struct ZR_STRUCT_ALIGN SZrFunction {
    SZrRawObject super;
    TZrUInt16 parameterCount;
    TZrBool hasVariableArguments;
    TZrUInt32 stackSize;
    // function name (函数名由函数自身持有，匿名函数为 ZR_NULL)
    struct SZrString *functionName;
    // length
    TZrUInt32 instructionsLength;
    TZrUInt32 closureValueLength;
    TZrUInt32 constantValueLength;
    TZrUInt32 localVariableLength;
    TZrUInt32 childFunctionLength;
    TZrUInt32 executionLocationInfoLength;
    TZrUInt32 catchClauseCount;
    TZrUInt32 exceptionHandlerCount;
    // debug
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
    // instructions
    TZrInstruction *instructionsList;
    // variables
    SZrFunctionClosureVariable *closureValueList;
    SZrTypeValue *constantValueList;
    SZrFunctionLocalVariable *localVariableList;
    struct SZrFunction *childFunctionList;
    // function debug info
    SZrFunctionExecutionLocationInfo *executionLocationInfoList;
    SZrFunctionCatchClauseInfo *catchClauseList;
    SZrFunctionExceptionHandlerInfo *exceptionHandlerList;
    TZrUInt32 *lineInSourceList;
    struct SZrString *sourceCodeList;
    SZrRawObject *gcList;
    
    // module export info (for script-level functions only)
    // 导出变量信息（用于模块导出）
    struct SZrFunctionExportedVariable {
        struct SZrString *name;                    // 变量名
        TZrUInt32 stackSlot;                          // 栈槽位
        TZrUInt8 accessModifier;                      // 可见性修饰符 (0=PRIVATE, 1=PUBLIC, 2=PROTECTED)
    } *exportedVariables;                           // 导出变量数组
    TZrUInt32 exportedVariableLength;                 // 导出变量数量

    SZrFunctionTypedLocalBinding *typedLocalBindings;
    TZrUInt32 typedLocalBindingLength;
    SZrFunctionTypedExportSymbol *typedExportedSymbols;
    TZrUInt32 typedExportedSymbolLength;
    
    // prototype数据存储（从常量池迁移）
    TZrByte *prototypeData;                           // prototype 二进制数据（序列化后的 SZrCompiledPrototypeInfo 数组）
    TZrUInt32 prototypeDataLength;                    // prototype 数据长度（字节数）
    TZrUInt32 prototypeCount;                         // prototype 数量
    struct SZrObjectPrototype **prototypeInstances; // 运行时实例化的prototype对象指针数组
    TZrUInt32 prototypeInstancesLength;               // prototype实例数组长度
};

typedef struct SZrFunction SZrFunction;

// struct ZR_STRUCT_ALIGN SZrFunctionOverload {
//     TZrSize functionOverloadsLength;
//     SZrFunction **functionOverloads;
// };
//
// typedef struct SZrFunctionOverload SZrFunctionOverload;

ZR_CORE_API SZrFunction *ZrCore_Function_New(struct SZrState *state);

ZR_CORE_API void ZrCore_Function_Free(struct SZrState *state, SZrFunction *function);

ZR_CORE_API struct SZrString *ZrCore_Function_GetLocalVariableName(SZrFunction *function, TZrUInt32 index,
                                                             TZrUInt32 programCounter);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CheckStack(struct SZrState *state, TZrSize size,
                                                      TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Function_CheckNativeStack(struct SZrState *state);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CheckStackAndGc(struct SZrState *state, TZrSize size,
                                                           TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Function_Call(struct SZrState *state, TZrStackValuePointer stackPointer, TZrSize resultCount);

ZR_CORE_API void ZrCore_Function_CallWithoutYield(struct SZrState *state, TZrStackValuePointer stackPointer,
                                            TZrSize resultCount);

ZR_CORE_API void ZrCore_Function_StackAnchorInit(struct SZrState *state,
                                           TZrStackValuePointer stackPointer,
                                           SZrFunctionStackAnchor *anchor);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_StackAnchorRestore(struct SZrState *state,
                                                              const SZrFunctionStackAnchor *anchor);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CheckStackAndAnchor(struct SZrState *state,
                                                               TZrSize size,
                                                               TZrStackValuePointer checkPointer,
                                                               TZrStackValuePointer stackPointer,
                                                               SZrFunctionStackAnchor *anchor);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallAndRestore(struct SZrState *state,
                                                          TZrStackValuePointer stackPointer,
                                                          TZrSize resultCount);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestore(struct SZrState *state,
                                                                      TZrStackValuePointer stackPointer,
                                                                      TZrSize resultCount);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallAndRestoreAnchor(struct SZrState *state,
                                                                const SZrFunctionStackAnchor *anchor,
                                                                TZrSize resultCount);

ZR_CORE_API TZrStackValuePointer ZrCore_Function_CallWithoutYieldAndRestoreAnchor(struct SZrState *state,
                                                                              const SZrFunctionStackAnchor *anchor,
                                                                              TZrSize resultCount);

ZR_CORE_API struct SZrCallInfo *ZrCore_Function_PreCall(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                  TZrSize resultCount, TZrStackValuePointer returnDestination);

ZR_CORE_API void ZrCore_Function_PostCall(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount);
#endif // ZR_VM_CORE_FUNCTION_H
