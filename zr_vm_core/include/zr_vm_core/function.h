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

typedef enum EZrFunctionMemberEntryKind {
    ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL = 0,
    ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR = 1
} EZrFunctionMemberEntryKind;

typedef struct SZrFunctionMemberEntry {
    struct SZrString *symbol;
    TZrUInt8 entryKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    TZrUInt32 prototypeIndex;
    TZrUInt32 descriptorIndex;
} SZrFunctionMemberEntry;

#define ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR ((TZrUInt8)0x01)

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

typedef struct SZrFunctionMetadataParameter {
    struct SZrString *name;
    SZrFunctionTypedTypeRef type;
} SZrFunctionMetadataParameter;

typedef struct SZrFunctionCompileTimeVariableInfo {
    struct SZrString *name;
    SZrFunctionTypedTypeRef type;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
} SZrFunctionCompileTimeVariableInfo;

typedef struct SZrFunctionCompileTimeFunctionInfo {
    struct SZrString *name;
    SZrFunctionTypedTypeRef returnType;
    TZrUInt32 parameterCount;
    SZrFunctionMetadataParameter *parameters;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
} SZrFunctionCompileTimeFunctionInfo;

typedef struct SZrFunctionTestInfo {
    struct SZrString *name;
    TZrUInt32 parameterCount;
    SZrFunctionMetadataParameter *parameters;
    TZrBool hasVariableArguments;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
} SZrFunctionTestInfo;

typedef enum EZrSemIrOpcode {
    ZR_SEMIR_OPCODE_NOP = 0,
    ZR_SEMIR_OPCODE_OWN_UNIQUE = 1,
    ZR_SEMIR_OPCODE_OWN_USING = 2,
    ZR_SEMIR_OPCODE_OWN_SHARE = 3,
    ZR_SEMIR_OPCODE_OWN_WEAK = 4,
    ZR_SEMIR_OPCODE_TYPEOF = 5,
    ZR_SEMIR_OPCODE_DYN_CALL = 6,
    ZR_SEMIR_OPCODE_DYN_TAIL_CALL = 7,
    ZR_SEMIR_OPCODE_META_CALL = 8,
    ZR_SEMIR_OPCODE_META_TAIL_CALL = 9,
    ZR_SEMIR_OPCODE_META_GET = 10,
    ZR_SEMIR_OPCODE_META_SET = 11,
    ZR_SEMIR_OPCODE_DYN_ITER_INIT = 12,
    ZR_SEMIR_OPCODE_DYN_ITER_MOVE_NEXT = 13,
    ZR_SEMIR_OPCODE_OWN_UPGRADE = 14,
    ZR_SEMIR_OPCODE_OWN_RELEASE = 15
} EZrSemIrOpcode;

typedef enum EZrSemIrEffectKind {
    ZR_SEMIR_EFFECT_KIND_NONE = 0,
    ZR_SEMIR_EFFECT_KIND_OWNERSHIP_TRANSITION = 1,
    ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME = 2
} EZrSemIrEffectKind;

typedef enum EZrSemIrOwnershipState {
    ZR_SEMIR_OWNERSHIP_STATE_PLAIN_GC = 0,
    ZR_SEMIR_OWNERSHIP_STATE_UNIQUE = 1,
    ZR_SEMIR_OWNERSHIP_STATE_SHARED = 2,
    ZR_SEMIR_OWNERSHIP_STATE_WEAK = 3,
    ZR_SEMIR_OWNERSHIP_STATE_BORROW_SHARED = 4,
    ZR_SEMIR_OWNERSHIP_STATE_BORROW_MUT = 5
} EZrSemIrOwnershipState;

typedef struct SZrSemIrOwnershipEntry {
    TZrUInt32 state;
} SZrSemIrOwnershipEntry;

typedef struct SZrSemIrEffectEntry {
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 ownershipInputIndex;
    TZrUInt32 ownershipOutputIndex;
} SZrSemIrEffectEntry;

typedef struct SZrSemIrBlockEntry {
    TZrUInt32 blockId;
    TZrUInt32 firstInstructionIndex;
    TZrUInt32 instructionCount;
} SZrSemIrBlockEntry;

typedef struct SZrSemIrInstruction {
    TZrUInt32 opcode;
    TZrUInt32 execInstructionIndex;
    TZrUInt32 typeTableIndex;
    TZrUInt32 effectTableIndex;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
    TZrUInt32 deoptId;
} SZrSemIrInstruction;

typedef struct SZrSemIrDeoptEntry {
    TZrUInt32 deoptId;
    TZrUInt32 execInstructionIndex;
} SZrSemIrDeoptEntry;

#define ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY ((TZrUInt32)2)

typedef enum EZrFunctionCallSiteCacheKind {
    ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE = 0,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET = 1,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET = 2,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC = 3,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC = 4,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL = 5,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL = 6,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL = 7,
    ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL = 8
} EZrFunctionCallSiteCacheKind;

typedef struct SZrFunctionCallSitePicSlot {
    struct SZrObjectPrototype *cachedReceiverPrototype;
    struct SZrObjectPrototype *cachedOwnerPrototype;
    struct SZrFunction *cachedFunction;
    TZrUInt32 cachedReceiverVersion;
    TZrUInt32 cachedOwnerVersion;
    TZrUInt32 cachedDescriptorIndex;
    TZrUInt8 cachedIsStatic;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrFunctionCallSitePicSlot;

typedef struct SZrFunctionCallSiteCacheEntry {
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 memberEntryIndex;
    TZrUInt32 deoptId;
    TZrUInt32 argumentCount;
    TZrUInt32 picSlotCount;
    TZrUInt32 picNextInsertIndex;
    TZrUInt32 reserved2;
    SZrFunctionCallSitePicSlot picSlots[ZR_FUNCTION_CALLSITE_CACHE_PIC_CAPACITY];
    TZrUInt32 runtimeHitCount;
    TZrUInt32 runtimeMissCount;
} SZrFunctionCallSiteCacheEntry;

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
    SZrFunctionCompileTimeVariableInfo *compileTimeVariableInfos;
    TZrUInt32 compileTimeVariableInfoLength;
    SZrFunctionCompileTimeFunctionInfo *compileTimeFunctionInfos;
    TZrUInt32 compileTimeFunctionInfoLength;
    SZrFunctionTestInfo *testInfos;
    TZrUInt32 testInfoLength;
    SZrFunctionMemberEntry *memberEntries;
    TZrUInt32 memberEntryLength;
    
    // prototype数据存储（从常量池迁移）
    TZrByte *prototypeData;                           // prototype 二进制数据（序列化后的 SZrCompiledPrototypeInfo 数组）
    TZrUInt32 prototypeDataLength;                    // prototype 数据长度（字节数）
    TZrUInt32 prototypeCount;                         // prototype 数量
    struct SZrObjectPrototype **prototypeInstances; // 运行时实例化的prototype对象指针数组
    TZrUInt32 prototypeInstancesLength;               // prototype实例数组长度

    SZrFunctionTypedTypeRef *semIrTypeTable;
    TZrUInt32 semIrTypeTableLength;
    SZrSemIrOwnershipEntry *semIrOwnershipTable;
    TZrUInt32 semIrOwnershipTableLength;
    SZrSemIrEffectEntry *semIrEffectTable;
    TZrUInt32 semIrEffectTableLength;
    SZrSemIrBlockEntry *semIrBlockTable;
    TZrUInt32 semIrBlockTableLength;
    SZrSemIrInstruction *semIrInstructions;
    TZrUInt32 semIrInstructionLength;
    SZrSemIrDeoptEntry *semIrDeoptTable;
    TZrUInt32 semIrDeoptTableLength;
    SZrFunctionCallSiteCacheEntry *callSiteCaches;
    TZrUInt32 callSiteCacheLength;
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

ZR_CORE_API TZrBool ZrCore_Function_TryReuseTailVmCall(struct SZrState *state,
                                                 struct SZrCallInfo *callInfo,
                                                 TZrStackValuePointer stackPointer);

ZR_CORE_API void ZrCore_Function_PostCall(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount);
#endif // ZR_VM_CORE_FUNCTION_H
