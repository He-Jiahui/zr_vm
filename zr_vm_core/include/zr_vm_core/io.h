//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_VM_CORE_IO_H
#define ZR_VM_CORE_IO_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_ffi_contract.h"

struct SZrState;
struct SZrGlobalState;
struct SZrString;
struct SZrFunction;

/** @brief 向反序列化器借出下一段连续字节；返回缓冲区须至少在下一次读取回调前有效，空指针或零长度表示结束。 */
typedef TZrBytePtr (*FZrIoRead)(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size);

/** 二进制文件保存稳定的 helper 编号，加载时按此表恢复运行时函数指针；预留编号不能重新分配。 */
typedef enum EZrIoNativeHelperId {
    ZR_IO_NATIVE_HELPER_NONE = 0,
    ZR_IO_NATIVE_HELPER_MODULE_IMPORT = 1,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_UNIQUE = 2,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE = 3,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_DEGRADE = 4,
    ZR_IO_NATIVE_HELPER_RESERVED_LEGACY_OWNERSHIP_USING = 5,
    ZR_IO_NATIVE_HELPER_REFLECTION_TYPEOF = 6,
    ZR_IO_NATIVE_HELPER_RESERVED_LEGACY_RUNTIME_DECORATOR_APPLY = 7,
    ZR_IO_NATIVE_HELPER_RESERVED_LEGACY_RUNTIME_MEMBER_DECORATOR_APPLY = 8,
    ZR_IO_NATIVE_HELPER_MODULE_IMPORT_GUARD = 9,
    ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN = 10
} EZrIoNativeHelperId;

/** @brief 写入器回调类型；TODO: 当前仓库未找到使用点，需核查它是否仍属公开写入契约。 */
typedef EZrThreadStatus (*FZrIoWrite)(struct SZrState *state, TZrBytePtr buffer, TZrSize size, TZrPtr customData);

/** @brief 释放加载器私有资源；调用方在最后一次读取后显式执行，释放 SZrIo 本身不会调用它。 */
typedef void (*FZrIoClose)(struct SZrState *state, TZrPtr customData);

/** 读取游标借用回调提供的缓冲区；remained/pointer 只在该缓冲区有效期内可用，hasReadError 在一次读取会话内保持失败。 */
struct ZR_STRUCT_ALIGN SZrIo {
    struct SZrState *state;
    FZrIoRead read;
    TZrSize remained;
    TZrBytePtr pointer;
    TZrPtr customData;
    FZrIoClose close;
    TZrBool isBinary;
    TZrBool hasReadError;
    TZrUInt32 sourceVersionPatch;
};

typedef struct SZrIo SZrIo;

/** @brief 由宿主选择源码或二进制输入并初始化 io；成功时加载器随后负责调用 io->close。 */
typedef TZrBool (*FZrIoLoadSource)(struct SZrState *state, TZrNativeString sourcePath, TZrNativeString md5, SZrIo *io);


/** 模块依赖的序列化名称和摘要；字符串由 GC 管理，数组由 SZrIoSource 所有。 */
struct SZrIoImport {
    struct SZrString *name;
    struct SZrString *md5;
};

typedef struct SZrIoImport SZrIoImport;


/** 跨模块声明引用；索引的解释依赖对应模块导出表，不能单独作为运行时对象使用。 */
struct SZrIoReference {
    struct SZrString *referenceModuleName;
    struct SZrString *referenceModuleMd5;
    TZrSize referenceIndex;
};

typedef struct SZrIoReference SZrIoReference;

// 前向声明，用于SZrIoClass和SZrIoStruct
struct SZrIoFunction;

/** 编译产物中的局部变量调试与逃逸信息，加载后投影到运行时函数的局部变量表。 */
struct SZrIoFunctionLocalVariable {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt64 instructionStartIndex;
    TZrUInt64 instructionEndIndex;
    TZrUInt64 startLine; // debug
    TZrUInt64 endLine; // debug
    TZrUInt32 scopeDepth;
    TZrUInt32 escapeFlags;
};

typedef struct SZrIoFunctionLocalVariable SZrIoFunctionLocalVariable;

/** 捕获槽位及作用域信息；加载后成为运行时 closureValueList 的捕获元数据。 */
struct SZrIoFunctionClosureVariable {
    struct SZrString *name;
    TZrUInt8 inStack;
    TZrUInt32 index;
    TZrUInt32 valueType;
    TZrUInt32 scopeDepth;
    TZrUInt32 escapeFlags;
};

typedef struct SZrIoFunctionClosureVariable SZrIoFunctionClosureVariable;

/** catch 类型和目标指令偏移；由异常调度按处理器记录中的范围索引使用。 */
typedef struct SZrIoFunctionCatchClause {
    struct SZrString *typeName;
    TZrUInt64 targetInstructionOffset;
} SZrIoFunctionCatchClause;

/** try/catch/finally 的序列化控制流边界；索引须与同一函数的 catch 表匹配。 */
typedef struct SZrIoFunctionExceptionHandler {
    TZrUInt64 protectedStartInstructionOffset;
    TZrUInt64 finallyTargetInstructionOffset;
    TZrUInt64 afterFinallyInstructionOffset;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 catchClauseCount;
    TZrUInt8 hasFinally;
} SZrIoFunctionExceptionHandler;

/** 常量值可包含嵌套函数；后者的原生树归此记录所有，加载时转成运行时函数值。 */
struct SZrIoFunctionConstantVariable {
    EZrValueType type;
    TZrPureValue value;
    TZrBool hasFunctionValue;
    struct SZrIoFunction *functionValue;
    TZrUInt64 startLine; // debug
    TZrUInt64 endLine; // debug
};

typedef struct SZrIoFunctionConstantVariable SZrIoFunctionConstantVariable;

/** 模块导出槽位与可调用子函数索引的二进制快照；加载后参与符号链接。 */
struct SZrIoFunctionExportedVariable {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    TZrUInt32 callableChildIndex;
};

typedef struct SZrIoFunctionExportedVariable SZrIoFunctionExportedVariable;

/** 持久化的类型引用；名称为 GC 字符串，值语义字段由运行时元数据复制。 */
typedef struct SZrIoFunctionTypedTypeRef {
    EZrValueType baseType;
    TZrBool isNullable;
    TZrUInt32 ownershipQualifier;
    TZrBool isArray;
    struct SZrString *typeName;
    EZrValueType elementBaseType;
    struct SZrString *elementTypeName;
    EZrStaticCType staticCType;
    TZrUInt32 staticCTypeId;
} SZrIoFunctionTypedTypeRef;

/** 泛型约束的二进制表示；constraintTypeNames 数组归所属导出符号所有。 */
typedef struct SZrIoFunctionTypedGenericParameter {
    struct SZrString *name;
    TZrUInt8 genericKind;
    TZrUInt8 variance;
    TZrUInt8 requiresClass;
    TZrUInt8 requiresStruct;
    TZrUInt8 requiresNew;
    TZrUInt8 requiresOwner;
    TZrUInt16 reserved0;
    TZrUInt32 requiredOwnershipQualifier;
    TZrUInt32 constraintTypeCount;
    struct SZrString **constraintTypeNames;
} SZrIoFunctionTypedGenericParameter;

/** 源级局部变量与类型、符号、位置 ID 的绑定，供加载后的分析与诊断使用。 */
typedef struct SZrIoFunctionTypedLocalBinding {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    SZrIoFunctionTypedTypeRef type;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 placeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
    TZrUInt32 roleFlags;
} SZrIoFunctionTypedLocalBinding;

/** 闭包捕获项的类型及声明身份，captureIndex 对应同一函数的捕获表。 */
typedef struct SZrIoFunctionTypedClosureBinding {
    TZrUInt32 captureIndex;
    SZrIoFunctionTypedTypeRef type;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
} SZrIoFunctionTypedClosureBinding;

/** 类型化导出签名及 token；子数组由源树所有，运行时投影须在源树释放前完成。 */
typedef struct SZrIoFunctionTypedExportSymbol {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt8 accessModifier;
    TZrUInt8 symbolKind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt16 reserved0;
    TZrUInt32 callableChildIndex;
    SZrIoFunctionTypedTypeRef valueType;
    TZrSize parameterCount;
    SZrIoFunctionTypedTypeRef *parameterTypes;
    TZrSize genericParameterCount;
    SZrIoFunctionTypedGenericParameter *genericParameters;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 columnInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrUInt32 columnInSourceEnd;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrUInt32 signatureBlobOffset;
    TZrUInt32 signatureBlobLength;
    TZrUInt64 signatureHash;
} SZrIoFunctionTypedExportSymbol;

/** 模块作用及目标签名约束，供加载后的能力检查和可调用摘要使用。 */
typedef struct SZrIoFunctionModuleEffect {
    TZrUInt8 kind;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
    struct SZrString *moduleName;
    struct SZrString *assemblyName;
    struct SZrString *symbolName;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 columnInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrUInt32 columnInSourceEnd;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken targetSignatureToken;
    TZrUInt64 targetSignatureHash;
    TZrUInt64 targetModuleSignatureHash;
    struct SZrString *requestedModuleVersion;
    struct SZrString *minModuleVersionInclusive;
    struct SZrString *maxModuleVersionExclusive;
} SZrIoFunctionModuleEffect;

/** 一个导出可调用项的模块作用集合；effects 数组由所属函数源树管理。 */
typedef struct SZrIoFunctionCallableSummary {
    struct SZrString *name;
    TZrUInt32 callableChildIndex;
    TZrSize effectCount;
    SZrIoFunctionModuleEffect *effects;
} SZrIoFunctionCallableSummary;

/** 顶层绑定名与子函数索引的关系，加载后用于恢复可调用导出。 */
typedef struct SZrIoFunctionTopLevelCallableBinding {
    struct SZrString *name;
    TZrUInt32 stackSlot;
    TZrUInt32 callableChildIndex;
    TZrUInt8 accessModifier;
    TZrUInt8 exportKind;
    TZrUInt8 readiness;
    TZrUInt8 reserved0;
} SZrIoFunctionTopLevelCallableBinding;

/** 参数类型、默认值和装饰器元数据；嵌套函数常量的原生存储须递归释放。 */
typedef struct SZrIoFunctionMetadataParameter {
    struct SZrString *name;
    SZrIoFunctionTypedTypeRef type;
    TZrBool hasDefaultValue;
    SZrIoFunctionConstantVariable defaultValue;
    TZrUInt8 hasDecoratorMetadata;
    SZrIoFunctionConstantVariable decoratorMetadataValue;
    TZrSize decoratorNamesLength;
    struct SZrString **decoratorNames;
} SZrIoFunctionMetadataParameter;

/** 编译期可见路径到目标符号的关联，由所属变量元数据管理。 */
typedef struct SZrIoFunctionCompileTimePathBinding {
    struct SZrString *path;
    TZrUInt8 targetKind;
    struct SZrString *targetName;
} SZrIoFunctionCompileTimePathBinding;

/** 编译期变量快照；路径数组只在源树生命周期内有效。 */
typedef struct SZrIoFunctionCompileTimeVariableInfo {
    struct SZrString *name;
    SZrIoFunctionTypedTypeRef type;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
    TZrSize pathBindingsLength;
    SZrIoFunctionCompileTimePathBinding *pathBindings;
} SZrIoFunctionCompileTimeVariableInfo;

/** 编译期函数签名快照；参数数组及其装饰器数据由源树递归释放。 */
typedef struct SZrIoFunctionCompileTimeFunctionInfo {
    struct SZrString *name;
    SZrIoFunctionTypedTypeRef returnType;
    TZrSize parameterCount;
    SZrIoFunctionMetadataParameter *parameters;
    TZrUInt32 lineInSourceStart;
    TZrUInt32 lineInSourceEnd;
} SZrIoFunctionCompileTimeFunctionInfo;

/** 逃逸分析持久化绑定；slotOrIndex 依赖 bindingKind 才能解释。 */
typedef struct SZrIoFunctionEscapeBinding {
    struct SZrString *name;
    TZrUInt32 slotOrIndex;
    TZrUInt32 scopeDepth;
    TZrUInt32 escapeFlags;
    TZrUInt8 bindingKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrIoFunctionEscapeBinding;

/** 成员符号到 prototype/descriptor 的关联，索引仅在同一函数元数据内有意义。 */
typedef struct SZrIoFunctionMemberEntry {
    struct SZrString *symbol;
    TZrUInt8 entryKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
    TZrUInt32 prototypeIndex;
    TZrUInt32 descriptorIndex;
} SZrIoFunctionMemberEntry;

/** 原生帧槽布局；加载时验证大小、对齐、别名标志后才能供执行器直接访问。 */
typedef struct SZrIoFunctionFrameSlotLayout {
    TZrUInt32 stackSlot;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 typeLayoutId;
    TZrUInt8 slotKind;
    TZrUInt8 isParameter;
    TZrUInt16 reserved0;
} SZrIoFunctionFrameSlotLayout;

/** SemIR 所有权状态的持久化行，由运行时投影复制到函数侧表。 */
typedef struct SZrIoSemIrOwnershipEntry {
    TZrUInt32 state;
} SZrIoSemIrOwnershipEntry;

/** SemIR 副作用记录；输入输出索引均相对于同一函数的所有权表。 */
typedef struct SZrIoSemIrEffectEntry {
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 ownershipInputIndex;
    TZrUInt32 ownershipOutputIndex;
} SZrIoSemIrEffectEntry;

/** SemIR 块到指令区间的映射，加载后供执行与调试侧表使用。 */
typedef struct SZrIoSemIrBlockEntry {
    TZrUInt32 blockId;
    TZrUInt32 firstInstructionIndex;
    TZrUInt32 instructionCount;
} SZrIoSemIrBlockEntry;

/** SemIR 指令快照；表索引须在同一函数的类型、作用及退优化表内解释。 */
typedef struct SZrIoSemIrInstruction {
    TZrUInt32 opcode;
    TZrUInt32 execInstructionIndex;
    TZrUInt32 typeTableIndex;
    TZrUInt32 effectTableIndex;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
    TZrUInt32 deoptId;
} SZrIoSemIrInstruction;

/** SemIR 退优化 ID 与执行指令位置的对应关系。 */
typedef struct SZrIoSemIrDeoptEntry {
    TZrUInt32 deoptId;
    TZrUInt32 execInstructionIndex;
} SZrIoSemIrDeoptEntry;

/** 调用点缓存与绑定契约的持久化记录；加载后保留供调用链接和优化路径使用。 */
typedef struct SZrIoFunctionCallSiteCacheEntry {
    TZrUInt32 kind;
    TZrUInt32 instructionIndex;
    TZrUInt32 memberEntryIndex;
    TZrUInt32 deoptId;
    TZrUInt32 argumentCount;
    SZrCallBindingContract bindingContract;
    SZrCallBindingLocation bindingLocation;
} SZrIoFunctionCallSiteCacheEntry;

struct SZrIoFunction;

/** 子函数原生树的所有权边；释放源树时递归释放 subFunction。 */
struct SZrIoFunctionClosure {
    struct SZrIoFunction *subFunction;
};

typedef struct SZrIoFunctionClosure SZrIoFunctionClosure;

/** 指令对应的源代码区间，供运行时诊断定位。 */
typedef struct SZrIoInstructionSourceRange {
    TZrUInt32 startLine;
    TZrUInt32 startColumn;
    TZrUInt32 endLine;
    TZrUInt32 endColumn;
} SZrIoInstructionSourceRange;

/** 调试映射；两种逐指令数组均由同一个 instructionsLength 定界并由源树释放。 */
struct SZrIoFunctionDebugInfo {
    struct SZrString *sourceFile;
    struct SZrString *sourceHash;
    TZrSize instructionsLength;
    TZrUInt64 *instructionsLine;
    SZrIoInstructionSourceRange *instructionRanges;
};

typedef struct SZrIoFunctionDebugInfo SZrIoFunctionDebugInfo;

// 前向声明，用于SZrIoClass和SZrIoStruct
struct SZrIoMemberDeclare;
typedef struct SZrIoMemberDeclare SZrIoMemberDeclare;

/** 类原型的二进制声明；父类型引用和成员数组属于源树，运行时加载前不得释放。 */
struct SZrIoClass {
    struct SZrString *name;
    TZrSize superClassLength;
    SZrIoReference *superClasses;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    struct SZrIoMemberDeclare *declares;
};

typedef struct SZrIoClass SZrIoClass;

/** 结构体原型的二进制声明；与类原型共用成员声明的读取和释放规则。 */
struct SZrIoStruct {
    struct SZrString *name;
    TZrSize superStructLength;
    SZrIoReference *superStructs;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    struct SZrIoMemberDeclare *declares;
};

typedef struct SZrIoStruct SZrIoStruct;

/** 一个函数的完整二进制中间表示；各 Length/Count 与对应原生数组共同构成源树所有权契约。 */
struct SZrIoFunction {
    struct SZrString *name;
    TZrUInt64 startLine;
    TZrUInt64 endLine;
    TZrSize parametersLength;
    TZrUInt64 hasVarArgs;
    TZrUInt32 stackSize;
    TZrUInt32 vmEntryClearStackSizePlusOne;
    TZrUInt32 sourceVersionPatch;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    TZrSize frameSlotLayoutsLength;
    SZrIoFunctionFrameSlotLayout *frameSlotLayouts;
    TZrSize instructionsLength;
    TZrInstruction *instructions;
    TZrSize localVariablesLength;
    SZrIoFunctionLocalVariable *localVariables;
    TZrSize closureVariablesLength;
    SZrIoFunctionClosureVariable *closureVariables;
    TZrSize catchClauseCount;
    SZrIoFunctionCatchClause *catchClauses;
    TZrSize exceptionHandlerCount;
    SZrIoFunctionExceptionHandler *exceptionHandlers;
    TZrSize constantVariablesLength;
    SZrIoFunctionConstantVariable *constantVariables;
    TZrSize exportedVariablesLength;
    SZrIoFunctionExportedVariable *exportedVariables;
    TZrSize typedLocalBindingsLength;
    SZrIoFunctionTypedLocalBinding *typedLocalBindings;
    TZrSize typedClosureBindingsLength;
    SZrIoFunctionTypedClosureBinding *typedClosureBindings;
    TZrSize typedExportedSymbolsLength;
    SZrIoFunctionTypedExportSymbol *typedExportedSymbols;
    TZrSize metadataTokenRecordLength;
    SZrMetadataTokenRecord *metadataTokenRecords;
    TZrSize moduleMetadataTokenRecordLength;
    SZrMetadataTokenRecord *moduleMetadataTokenRecords;
    TZrSize signatureBlobHeapLength;
    TZrByte *signatureBlobHeap;
    TZrSize metadataStringHeapLength;
    SZrMetadataStringHeapEntry *metadataStringHeap;
    TZrUInt64 moduleSignatureHash;
    struct SZrString *moduleVersion;
    TZrSize moduleMetadataBindingLength;
    SZrMetadataTokenBinding *moduleMetadataBindings;
    TZrSize staticImportsLength;
    struct SZrString **staticImports;
    TZrSize moduleEntryEffectsLength;
    SZrIoFunctionModuleEffect *moduleEntryEffects;
    TZrSize exportedCallableSummariesLength;
    SZrIoFunctionCallableSummary *exportedCallableSummaries;
    TZrSize topLevelCallableBindingsLength;
    SZrIoFunctionTopLevelCallableBinding *topLevelCallableBindings;
    TZrSize parameterMetadataLength;
    SZrIoFunctionMetadataParameter *parameterMetadata;
    TZrUInt8 hasCallableReturnType;
    SZrIoFunctionTypedTypeRef callableReturnType;
    TZrSize compileTimeVariableInfosLength;
    SZrIoFunctionCompileTimeVariableInfo *compileTimeVariableInfos;
    TZrSize compileTimeFunctionInfosLength;
    SZrIoFunctionCompileTimeFunctionInfo *compileTimeFunctionInfos;
    TZrSize escapeBindingLength;
    SZrIoFunctionEscapeBinding *escapeBindings;
    TZrSize returnEscapeSlotCount;
    TZrUInt32 *returnEscapeSlots;
    TZrUInt8 hasDecoratorMetadata;
    SZrIoFunctionConstantVariable decoratorMetadataValue;
    TZrSize decoratorNamesLength;
    struct SZrString **decoratorNames;
    TZrSize memberEntriesLength;
    SZrIoFunctionMemberEntry *memberEntries;
    TZrSize semIrTypeTableLength;
    SZrIoFunctionTypedTypeRef *semIrTypeTable;
    TZrSize semIrOwnershipTableLength;
    SZrIoSemIrOwnershipEntry *semIrOwnershipTable;
    TZrSize semIrEffectTableLength;
    SZrIoSemIrEffectEntry *semIrEffectTable;
    TZrSize semIrBlockTableLength;
    SZrIoSemIrBlockEntry *semIrBlockTable;
    TZrSize semIrInstructionLength;
    SZrIoSemIrInstruction *semIrInstructions;
    TZrSize semIrDeoptTableLength;
    SZrIoSemIrDeoptEntry *semIrDeoptTable;
    TZrSize callSiteCacheLength;
    SZrIoFunctionCallSiteCacheEntry *callSiteCaches;
    TZrSize nativeImportContractLength;
    SZrNativeImportContract *nativeImportContracts;
    TZrSize testManifestDataLength;
    TZrByte *testManifestData;
    TZrSize prototypesLength;                // prototype 数量
    TZrSize classesLength;
    SZrIoClass *classes;                      // class prototype 数组（如果 type 是 CLASS）
    TZrSize structsLength;
    SZrIoStruct *structs;                     // struct prototype 数组（如果 type 是 STRUCT）
    TZrSize prototypeDataLength;              // 完整 prototypeData blob（含头部 count）
    TZrByte *prototypeData;
    TZrSize closuresLength;
    SZrIoFunctionClosure *closures;
    TZrSize debugInfosLength;
    SZrIoFunctionDebugInfo *debugInfos;
};

typedef struct SZrIoFunction SZrIoFunction;

/** 按元类型分组的函数重载集合，由所属成员声明管理。 */
struct SZrIoMeta {
    EZrMetaType metaType;
    TZrSize functionsLength;
    SZrIoFunction *functions;
};

typedef struct SZrIoMeta SZrIoMeta;

/** 方法名与重载函数表，运行时投影前保持源树所有权。 */
struct SZrIoMethod {
    struct SZrString *name;
    TZrSize functionsLength;
    SZrIoFunction *functions;
};

typedef struct SZrIoMethod SZrIoMethod;

/** 属性及两个访问器函数；读取器分别建立 getter/setter 原生函数树。 */
struct SZrIoProperty {
    struct SZrString *name;
    /* TODO: propertyType 的跨版本含义未在读取器验证；需核对写入器与运行时消费方。 */
    TZrUInt32 propertyType;
    SZrIoFunction *getter;
    SZrIoFunction *setter;
};

typedef struct SZrIoProperty SZrIoProperty;

/** 字段声明的持久化名称；其值及运行时布局由后续模块链接决定。 */
struct SZrIoField {
    struct SZrString *name;
};

typedef struct SZrIoField SZrIoField;

/** 枚举字段及底层值；value 按所属枚举的 valueType 解释。 */
struct SZrIoEnumField {
    struct SZrString *name;
    TZrPureValue value;
};

typedef struct SZrIoEnumField SZrIoEnumField;

/** 成员类型判别式联合体；仅访问 type 指定的分支，并按该分支递归释放。 */
struct SZrIoMemberDeclare {
    EZrIoMemberDeclareType type;
    EZrIoMemberDeclareStatus status;
    union {
        SZrIoField *field;
        SZrIoMethod *method;
        SZrIoProperty *property;
        SZrIoMeta *meta;
        SZrIoEnumField *enumField;
    };
};

typedef struct SZrIoMemberDeclare SZrIoMemberDeclare;

/** 接口声明的继承与成员列表，引用索引需在模块上下文中解析。 */
struct SZrIoInterface {
    struct SZrString *name;
    TZrSize superInterfaceLength;
    SZrIoReference *superInterfaces;
    TZrSize genericParametersLength;
    TZrSize declaresLength;
    SZrIoMemberDeclare *declares;
};

typedef struct SZrIoInterface SZrIoInterface;

/** 枚举及字段数组；valueType 决定每个字段值的读取协议。 */
struct SZrIoEnum {
    struct SZrString *name;
    EZrValueType valueType;
    TZrSize fieldsLength;
    SZrIoEnumField *fields;
};

typedef struct SZrIoEnum SZrIoEnum;

/** 模块级声明的判别式联合体；type 决定哪个原生子树需要释放。 */
struct SZrIoModuleDeclare {
    EZrIoModuleDeclareType type;

    union {
        SZrIoClass *class_;
        SZrIoStruct *struct_;
        SZrIoInterface *interface_;
        SZrIoFunction *function;
        SZrIoEnum *enum_;
        SZrIoField *field;
    };
};

typedef struct SZrIoModuleDeclare SZrIoModuleDeclare;

/** 单个二进制模块的依赖、声明与入口函数；所有原生数组由 SZrIoSource 统一回收。 */
struct SZrIoModule {
    struct SZrString *name;
    struct SZrString *md5;
    TZrSize importsLength;
    SZrIoImport *imports;
    TZrSize declaresLength;
    SZrIoModuleDeclare *declares;
    SZrIoFunction *entryFunction;
};

typedef struct SZrIoModule SZrIoModule;

/** 文件头和模块森林；原生存储由 ReadSourceFree 释放，内含 GC 字符串继续由 GC 管理。 */
struct SZrIoSource {
    TZrChar signature[4];
    TZrUInt32 versionMajor;
    TZrUInt32 versionMinor;
    TZrUInt32 versionPatch;
    TZrUInt64 format;
    TZrUInt8 nativeIntSize;
    TZrUInt8 typeSizeSize;
    TZrUInt8 typeInstructionSize;
    TZrBool isBigEndian;
    TZrBool isDebug;
    TZrChar optional[3];
    TZrSize modulesLength;
    SZrIoModule *modules;
};

typedef struct SZrIoSource SZrIoSource;

/** @brief 创建可复用的读取游标；成功后用 Init 绑定输入，最后由 Free 释放。 */
/** @pre global 及其 allocator 有效。 */
ZR_CORE_API SZrIo *ZrCore_Io_New(struct SZrGlobalState *global);

/** @brief 仅释放游标，不关闭输入源，也不释放 ReadSourceNew 返回的源树。 */
/** @pre io 非空时，global 及其 allocator 须有效。 */
ZR_CORE_API void ZrCore_Io_Free(struct SZrGlobalState *global, SZrIo *io);

/** @brief 将已有游标绑定到一次读取会话；state 和 customData 须覆盖整个读取及可选 close 调用。 */
/** @pre state、io 和 read 有效；read 须提供 FZrIoRead 契约的缓冲区，若提供 close，调用方在会话结束时显式调用。 */
ZR_CORE_API void ZrCore_Io_Init(struct SZrState *state, SZrIo *io, FZrIoRead read, FZrIoClose close, TZrPtr customData);

/** @brief 从借用缓冲区读出至多 size 字节；短读返回实际字节数并保持 hasReadError。 */
ZR_CORE_API TZrSize ZrCore_Io_Read(SZrIo *io, TZrBytePtr buffer, TZrSize size);


/** @brief 解析二进制源树供模块加载或 AOT 转换；成功返回值由调用方用 ReadSourceFree 回收。 */
/** @pre io 已初始化，且 io->state、state->global 和 io->read 有效。 */
/** @note BUG: 当前失败路径不释放已分配的半成品源树；不可信输入还可触发长度溢出，见实现注释。 */
ZR_CORE_API SZrIoSource *ZrCore_Io_ReadSourceNew(SZrIo *io);

/** @brief 递归回收源树的原生数组；源树中的 GC 字符串和值不由此接口释放。 */
ZR_CORE_API void ZrCore_Io_ReadSourceFree(struct SZrGlobalState *global, SZrIoSource *source);

/** @brief 通过全局 sourceLoader 获取并解析二进制源；成功时调用方持有返回的源树。 */
/** @pre state->global->sourceLoader 已安装；源加载器成功时必须初始化 io。 */
ZR_CORE_API SZrIoSource *ZrCore_Io_LoadSource(struct SZrState *state, TZrNativeString sourceName, TZrNativeString md5);

/** @brief 将首个模块的入口函数复制到运行时并链接调用绑定；源树可随后释放。 */
/** @return 成功返回由运行时管理的函数，验证或分配失败返回空指针。 */
ZR_CORE_API struct SZrFunction *ZrCore_Io_LoadEntryFunctionToRuntime(struct SZrState *state,
                                                                     const SZrIoSource *source);
/** @brief 按持久化编号恢复内建原生 helper；未知或预留编号返回空指针。 */
ZR_CORE_API FZrNativeFunction ZrCore_Io_GetSerializableNativeHelperFunction(TZrUInt64 helperId);
/** @brief 恢复函数调用绑定侧表；仅在相关格式版本存在该表时由读取器调用。 */
ZR_CORE_API TZrBool ZrCore_Io_ReadCallBindings(SZrIo *io, SZrIoFunction *function);
#endif // ZR_VM_CORE_IO_H
