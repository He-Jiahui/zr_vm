//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_COMPILER_H
#define ZR_VM_PARSER_COMPILER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/comptime_contract.h"
#include "zr_vm_parser/test_contract.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_array_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_meta_conf.h"

typedef struct SZrCompilerStackSlotTypeHint {
    TZrUInt32 stackSlot;
    SZrInferredType type;
    TZrUInt32 aliasParentStackSlot;
    TZrUInt32 aliasMemberEntryIndex;
    TZrBool isFieldAlias;
    TZrBool isArrayElementAlias;
    TZrBool isInlineReceiverArgument;
    TZrBool isReadonlyAggregateArgument;
    TZrBool isReadonlyAggregateCallWindowSlot;
    TZrBool isReadonlyAggregateCallWindowCallable;
    TZrBool isReadonlyAggregateCallWindowActive;
    TZrUInt32 readonlyAggregateCallWindowArgumentCount;
} SZrCompilerStackSlotTypeHint;

// 编译器状态结构
typedef enum EZrCompilerInitializationPhase {
    ZR_COMPILER_INITIALIZATION_NONE = 0,
    ZR_COMPILER_INITIALIZATION_CONSTRUCTOR,
    ZR_COMPILER_INITIALIZATION_PROPERTY_INIT
} EZrCompilerInitializationPhase;

typedef enum EZrCompileToolBindingKind {
    ZR_COMPILE_TOOL_BINDING_SHADOW = 0,
    ZR_COMPILE_TOOL_BINDING_PROVIDER = 1
} EZrCompileToolBindingKind;

typedef struct SZrCompileToolBinding {
    SZrString *name;
    const SZrParserCompileToolModuleDescriptor *provider;
    const TZrChar *providerContentHash; // borrowed; provider storage outlives compiler state
    const SZrParserCompileToolResolvedArtifact *resolvedArtifact; // borrowed; close after compiler state
    EZrCompileToolBindingKind kind;
} SZrCompileToolBinding;

#define ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT 32U

typedef struct SZrComptimeCacheEntry {
    TZrByte digest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT];
    SZrTypeValue value;
} SZrComptimeCacheEntry;

typedef struct SZrCompilerAttributeFieldBinding {
    SZrString *name;
    EZrParserAttributeValueKind valueKind;
    TZrBool nullable;
} SZrCompilerAttributeFieldBinding;

typedef struct SZrCompilerAttributeSchemaBinding {
    SZrString *name;
    TZrUInt32 attributeId;
    TZrTypeId typeId;
    SZrParserAttributeUsage usage;
    SZrArray fields; // SZrCompilerAttributeFieldBinding
    SZrFileRange sourceRange;
} SZrCompilerAttributeSchemaBinding;

typedef struct SZrCompilerState {
    SZrState *state;                    // VM 状态
    SZrFunction *currentFunction;       // 当前编译的函数
    SZrAstNode *currentAst;             // 当前编译的 AST 节点
    SZrSemanticContext *semanticContext; // 统一语义记录上下文
    SZrHirModule *hirModule;            // 当前脚本的 HIR 模块句柄
    SZrSemanticIrFunction preSemanticIr; // ExecBC lowering 前的语义函数
    SZrArray preSemanticIrSlots;         // 栈槽到 PlaceId/ValueId 的编译期桥接
    SZrArray preSemanticIrReceiverLoanIds; // compiler-generated receiver loans
    TZrBool preSemanticIrInitialized;
    TZrBool preSemanticIrValidated;
    
    // 常量池管理
    SZrArray constants;                 // 常量值数组（SZrTypeValue）
    TZrSize constantCount;               // 常量数量
    TZrUInt32 cachedNullConstantIndex;   // 复用的 null 常量索引
    TZrBool hasCachedNullConstantIndex;  // 是否已经缓存 null 常量
    
    // 局部变量管理
    SZrArray localVars;                 // 局部变量数组（SZrFunctionLocalVariable）
    TZrSize localVarCount;              // 局部变量数量
    TZrSize stackSlotCount;             // 当前栈槽数量
    TZrSize maxStackSlotCount;          // 当前函数编译过程中的栈槽峰值
    TZrUInt32 lastExpressionSlot;       // 最近一次表达式编译产生的结果槽
    SZrArray stackSlotTypeHints;         // 临时栈槽类型提示（SZrCompilerStackSlotTypeHint）
    TZrSize stackSlotTypeHintScopeStart; // 当前函数的类型提示起始索引
    
    // 闭包管理
    SZrArray closureVars;               // 闭包变量数组（SZrFunctionClosureVariable）
    TZrSize closureVarCount;             // 闭包变量数量
    const struct SZrParserSubmissionContext *submissionContext; // borrowed incremental submission input
    SZrFunction *submissionEntryFunction; // root function allowed to publish submission bindings
    SZrArray submissionDeclaredCaptureIndices; // TZrUInt32 capture indexes published on success
    
    // 指令生成
    SZrArray instructions;              // 指令数组（TZrInstruction）
    TZrSize instructionCount;            // 指令数量
    
    // 作用域管理
    SZrArray scopeStack;                // 作用域栈（用于变量查找）
    
    // 跳转标签管理（用于控制流）
    SZrArray labels;                    // 标签数组
    SZrArray pendingJumps;              // 待解析的跳转
    SZrArray pendingAbsolutePatches;    // 待解析的绝对目标偏移（SZrPendingAbsolutePatch）
    
    // 循环标签栈（用于 break/continue）
    SZrArray loopLabelStack;            // 循环标签栈（SZrLoopLabel）
    SZrArray tryContextStack;           // try/finally 上下文栈（SZrCompilerTryContext）

    // 调试与异常元数据
    SZrArray executionLocations;        // 指令到源码行映射（SZrFunctionExecutionLocationInfo）
    SZrArray catchClauseInfos;          // catch 元数据（SZrCompilerCatchClauseInfo）
    SZrArray exceptionHandlerInfos;     // handler 元数据（SZrCompilerExceptionHandlerInfo）
    
    // 嵌套函数
    SZrArray childFunctions;            // 子函数数组（SZrFunction*）
    TZrBool emitTestManifest;
    SZrArray testManifestEntries;       // SZrParserTestEntry
    
    // 函数名到子函数索引的映射（仅用于编译时查找，运行时不需要）
    SZrArray childFunctionNameMap;      // 函数名映射数组（SZrChildFunctionNameMap）
    
    // 顶层函数声明（如果脚本只有一个顶层函数声明，保存它以便返回）
    SZrFunction *topLevelFunction;      // 顶层函数对象（如果存在）
    
    // 错误处理
    TZrBool hasError;
    TZrBool hadRecoverableError;            // 本轮编译是否出现过可恢复但不可忽略的错误
    const TZrChar *errorMessage;
    TZrChar *errorMessageStorage;
    TZrSize errorMessageStorageCapacity;
    SZrFileRange errorLocation;
    TZrBool hasStructuredError;
    SZrStructuredDiagnostic structuredError;
    TZrBool hasFatalError;                  // 是否有致命错误（阻止编译完成）
    TZrBool hasCompileTimeError;            // 是否发生过编译期错误（不能在后续语句中被吞掉）
    TZrBool suppressErrorOutput;            // 是否抑制 stderr 错误输出（LSP/分析器路径）
    
    // 尾调用优化上下文
    TZrBool isInTailCallContext;           // 是否处于尾调用上下文（return语句中的表达式）
    
    // 外部变量分析（用于闭包捕获）
    SZrArray referencedExternalVars;     // 引用的外部变量名数组（SZrString*），用于lambda编译时
    
    // 类型环境
    SZrTypeEnvironment *typeEnv;         // 当前类型环境
    SZrArray typeEnvStack;               // 类型环境栈（用于作用域管理）
    
    // 模块导出跟踪（仅用于脚本级变量）
    SZrArray pubVariables;               // pub 变量列表（SZrExportedVariable）
    SZrArray proVariables;                // pro 变量列表（SZrExportedVariable，包含所有 pub）
    SZrArray exportedTypes;              // TODO: 导出的类型列表（暂时作为占位）
    TZrBool isScriptLevel;                  // 是否在脚本级别（用于区分脚本级变量和函数内变量）
    
    // 脚本 AST 引用（用于类型查找）
    SZrAstNode *scriptAst;                // 当前编译的脚本 AST 节点（用于查找类型定义）
    SZrString *currentModuleKey;          // 当前脚本的 canonical 模块键
    
    // 类型 Prototype 信息（用于运行时创建）
    SZrArray typePrototypes;              // 待创建的 prototype 信息数组（SZrTypePrototypeInfo）
    SZrArray signatureCompiledInterfaceNodes; // Signature phase 已绑定的 interface AST 节点（SZrAstNode*）
    struct SZrTypePrototypeInfo *currentTypePrototypeInfo; // 当前正在构建的类型原型
    TZrBool externBindingsPredeclared;    // 是否已预注册 source-level extern 编译期绑定
    
    // 编译期环境管理
    SZrTypeEnvironment *compileTimeTypeEnv;   // 编译期类型环境
    SZrArray compileTimeVariables;            // 编译期变量表（SZrCompileTimeVariable*）
    SZrArray compileTimeFunctions;            // 编译期函数表（SZrCompileTimeFunction*）
    SZrArray importedCompileTimeModules;      // 跨文件导入模块的 compile-time 元数据（SZrImportedCompileTimeModule*）
    SZrArray importedCompileTimeModuleAliases; // 模块别名表（SZrImportedCompileTimeModuleAlias）
    struct SZrImportedCompileTimeModule *activeImportedCompileTimeModule; // 当前 compiler-only provider 执行域
    SZrArray typeValueAliases;                // 类型值别名表（SZrTypeBinding）
    SZrArray compileToolBindings;             // phase-tagged lexical CompileTool bindings
    SZrArray attributeSchemas;                // bound readonly-struct attribute schemas
    EZrParserCompilePhase compilePhase;
    SZrParserComptimeBudget comptimeBudget;
    EZrParserComptimeContext comptimeContext;
    TZrUInt64 comptimeCacheHitCount;
    TZrUInt64 comptimeCacheMissCount;
    TZrUInt32 comptimeCallDepth;
    SZrArray comptimeCache;                    // SZrComptimeCacheEntry
    TZrByte comptimeSourceDigest[ZR_PARSER_COMPTIME_CACHE_DIGEST_BYTE_COUNT];
    TZrBool hasComptimeSourceDigest;
    TZrBool isInCompileTimeContext;             // 是否在编译期上下文中
    TZrBool isCompilingCompileTimeRuntimeSupport; // 是否正在为 binary import 生成 compile-time runtime support
    
    // 构造函数上下文
    TZrBool isInConstructor;                     // 是否在构造函数中编译
    EZrCompilerInitializationPhase initializationPhase; // structured constructor/init-accessor phase
    SZrAstNode *currentFunctionNode;          // 当前编译的函数 AST 节点（用于访问参数信息）
    EZrCanonicalReceiverEffect currentFunctionReceiverEffect; // 当前 callable 的结构化 receiver effect
    TZrBool preservePropertyReferenceResult; // `ref property` keeps the managed reference identity
    SZrString *currentTypeName;               // 当前编译的类型名称（用于成员字段 const 检查）
    SZrAstNode *currentTypeNode;              // 当前编译的类型声明节点（用于 const 成员初始化检查）
    
    // const 变量跟踪（用于编译时检查）
    SZrArray constLocalVars;                   // const 局部变量名数组（SZrString*）
    SZrArray constParameters;                  // const 参数名数组（SZrString*）
    SZrArray constructorInitializedConstFields; // 构造函数中已初始化的 const 成员名数组（SZrString*）
    struct SZrCompilerState *compileToolProviderParent; // imported-provider compilation ancestry
    SZrArray ownedCompileToolProviders;         // SZrCompileToolProjectProvider*; compiler-only artifact owners
} SZrCompilerState;

// 编译期变量信息
typedef struct SZrCompileTimeVariable {
    SZrString *name;                       // 变量名
    SZrInferredType type;                  // 变量类型
    SZrAstNode *value;                     // 变量值（AST节点，用于编译期求值）
    SZrTypeValue evaluatedValue;           // 已求值的编译期结果
    TZrBool hasEvaluatedValue;               // 是否已经求值完成
    TZrBool isEvaluating;                    // 是否正在求值（用于循环依赖检测）
    SZrFileRange location;                  // 声明位置
} SZrCompileTimeVariable;

// 编译期函数信息
typedef struct SZrCompileTimeFunction {
    SZrString *name;                       // 函数名
    SZrAstNode *declaration;               // 函数声明 AST 节点
    SZrInferredType returnType;            // 返回类型
    SZrArray paramTypes;                   // 参数类型数组（SZrInferredType）
    SZrArray paramNames;                   // 参数名称数组（SZrString*）
    SZrArray paramHasDefaultValues;        // 参数默认值存在标记（TZrBool）
    SZrArray paramDefaultValues;           // 参数默认值（SZrTypeValue）
    SZrString *runtimeProjectionModuleName; // binary import 时回落到的模块路径
    SZrString *runtimeProjectionExportName; // binary import 时回落到的 pro export 名称
    TZrBool isRuntimeProjection;            // 是否为 runtime callable projection
    TZrBool isDeclarationTransform;         // declarationTransform role; never a runtime decorator
    TZrBool isExported;                     // visible through an imported module alias
    struct SZrImportedCompileTimeModule *ownerModule; // compiler-only source provider owner
    SZrFileRange location;                  // 声明位置
} SZrCompileTimeFunction;

typedef struct SZrImportedCompileTimeModule {
    SZrString *moduleName;                 // 逻辑模块路径
    SZrAstNode *scriptAst;                 // imported module AST（保持声明节点存活）
    SZrArray compileTimeVariables;         // SZrFunctionCompileTimeVariableInfo*
    SZrArray compileTimeFunctions;         // SZrCompileTimeFunction*
} SZrImportedCompileTimeModule;

typedef struct SZrImportedCompileTimeModuleAlias {
    SZrString *aliasName;                          // 当前脚本中的模块别名
    SZrImportedCompileTimeModule *module;          // 对应 imported module metadata
} SZrImportedCompileTimeModuleAlias;

typedef struct SZrTypeDecoratorInfo {
    SZrString *name;                       // 装饰器名称
} SZrTypeDecoratorInfo;

typedef struct SZrScopeCleanupRegistration {
    TZrUInt32 slot;
    TZrUInt32 sourceSlot;
    EZrOwnershipBuiltinKind ownershipBuiltinKind;
} SZrScopeCleanupRegistration;

// 作用域信息
typedef struct SZrScope {
    TZrSize startVarIndex;              // 作用域开始的变量索引
    TZrSize varCount;                   // 作用域内的变量数量
    TZrSize cleanupRegistrationCount;   // 作用域内 using 注册的清理数量
    SZrArray cleanupRegistrations;      // SZrScopeCleanupRegistration
    TZrUInt32 depth;                    // 作用域深度（用于逃逸分析）
    SZrCompilerState *parentCompiler;   // 父编译器（用于闭包）
} SZrScope;

// 跳转标签
typedef struct SZrLabel {
    TZrSize instructionIndex;           // 指令索引
    TZrBool isResolved;                   // 是否已解析
} SZrLabel;

// 待解析的跳转
typedef struct SZrPendingJump {
    TZrSize instructionIndex;           // 跳转指令的索引
    TZrSize labelId;                    // 目标标签 ID
} SZrPendingJump;

typedef struct SZrPendingAbsolutePatch {
    TZrSize instructionIndex;           // 需要写入绝对目标偏移的指令索引
    TZrSize labelId;                    // 目标标签 ID
} SZrPendingAbsolutePatch;

// 循环标签（用于 break/continue）
typedef struct SZrLoopLabel {
    TZrSize breakLabelId;               // break 目标标签 ID
    TZrSize continueLabelId;            // continue 目标标签 ID
    TZrSize targetScopeStackDepth;      // 跳转目标保留的作用域栈深度
} SZrLoopLabel;

typedef struct SZrCompilerCatchClauseInfo {
    SZrString *typeName;
    TZrSize targetLabelId;
} SZrCompilerCatchClauseInfo;

typedef struct SZrCompilerExceptionHandlerInfo {
    TZrMemoryOffset protectedStartInstructionOffset;
    TZrSize finallyLabelId;
    TZrSize afterFinallyLabelId;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 catchClauseCount;
    TZrBool hasFinally;
} SZrCompilerExceptionHandlerInfo;

typedef struct SZrCompilerTryContext {
    TZrUInt32 handlerIndex;
    TZrSize finallyLabelId;
} SZrCompilerTryContext;

// 导出变量信息（用于模块导出）
typedef struct SZrExportedVariable {
    SZrString *name;                    // 变量名
    TZrUInt32 stackSlot;                  // 栈槽位
    EZrAccessModifier accessModifier;   // 可见性修饰符
    EZrModuleExportKind exportKind;     // 导出种类
    EZrModuleExportReadiness readiness; // 导出就绪阶段
    TZrUInt32 callableChildIndex;       // 顶层导出函数对应的 childFunction 索引
} SZrExportedVariable;

// 函数名到子函数索引的映射（仅用于编译时查找，运行时不需要）
typedef struct SZrChildFunctionNameMap {
    SZrString *name;                    // 函数名
    TZrUInt32 childFunctionIndex;         // 子函数在 childFunctions 中的索引
} SZrChildFunctionNameMap;

// 编译时存储的 Prototype 信息
typedef struct SZrTypeGenericParameterInfo {
    SZrString *name;                    // 泛型参数名称
    EZrGenericParameterKind genericKind; // 泛型参数类别（类型 / const int）
    EZrGenericVariance variance;        // 方差信息（当前主要用于 interface 元数据）
    TZrBool requiresClass;             // class 约束
    TZrBool requiresStruct;            // struct 约束
    TZrBool requiresNew;               // new() 约束
    TZrBool requiresOwner;             // owner 约束（要求所有权泛型实参）
    EZrOwnershipQualifier requiredOwnershipQualifier; // unique/shared/weak 精确所有权约束
    SZrArray constraintTypeNames;       // 约束类型名称数组（SZrString*）
} SZrTypeGenericParameterInfo;

typedef struct SZrTypePrototypeInfo {
    SZrString *name;                    // 类型名称
    SZrAstNode *declarationNode;         // source declaration identity; NULL for metadata-only types
    EZrObjectPrototypeType type;        // STRUCT 或 CLASS
    EZrAccessModifier accessModifier;   // 访问修饰符
    TZrUInt32 modifierFlags;            // abstract/final 等类型修饰符
    TZrBool isImportedNative;           // 是否为仅用于编译期解析的导入类型 stub（native/source/binary）
    SZrString *importModuleName;         // imported stub 的 canonical module provenance
    TZrBool isNativeRuntime;            // 是否来自 native registry，需要保留 native 构造器返回值
    TZrUInt64 protocolMask;             // 稳定 protocol bit mask
    SZrArray inherits;                  // 继承的类型引用（SZrString* 数组，存储类型名称字符串）
    SZrString *extendsTypeName;         // 单继承目标（如有）
    SZrArray implements;                // 实现/扩展的接口引用（SZrString* 数组）
    SZrArray genericParameters;         // 泛型参数信息（SZrTypeGenericParameterInfo）
    SZrArray members;                   // 成员信息（字段、方法等，存储 SZrTypeMemberInfo）
    SZrArray decorators;                // 类型级 decorator 记录（SZrTypeDecoratorInfo）
    TZrBool hasDecoratorMetadata;       // 是否存在 compile-time decorator metadata
    SZrTypeValue decoratorMetadataValue; // compile-time decorator metadata 常量值
    SZrString *enumValueTypeName;       // enum 底层值类型
    TZrBool allowValueConstruction;     // 是否允许 $Type(...)
    TZrBool allowBoxedConstruction;     // 是否允许 new Type(...)
    SZrString *constructorSignature;    // 构造签名提示
    TZrUInt32 nextVirtualSlotIndex;     // 当前类型分配到的下一个 virtual slot
    TZrUInt32 nextPropertyIdentity;     // 当前类型分配到的下一个 property identity
    TZrUInt32 layoutByteSize;           // inline 布局总大小（字节）
    TZrUInt32 layoutByteAlign;          // inline 布局最大对齐（字节）
    TZrUInt32 intrinsicCapabilityId;    // registered intrinsic behavior; zero for ordinary types
} SZrTypePrototypeInfo;

#ifndef ZR_MEMBER_PARAMETER_COUNT_UNKNOWN
#define ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ((TZrUInt32)-1)
#endif

typedef enum EZrPropertyAccessorRole {
    ZR_PROPERTY_ACCESSOR_ROLE_NONE = 0,
    ZR_PROPERTY_ACCESSOR_ROLE_GET,
    ZR_PROPERTY_ACCESSOR_ROLE_SET,
    ZR_PROPERTY_ACCESSOR_ROLE_INIT,
} EZrPropertyAccessorRole;

// 成员信息（字段、方法、元函数等）
typedef struct SZrTypeMemberInfo {
    EZrAstNodeType memberType;          // 成员类型（STRUCT_FIELD, STRUCT_METHOD, CLASS_FIELD 等）
    SZrString *name;                    // 成员名称
    EZrAccessModifier accessModifier;   // 访问修饰符
    TZrBool isStatic;                     // 是否为静态成员
    TZrUInt32 modifierFlags;              // abstract/virtual/override/final/shadow 修饰符
    TZrBool isConst;                      // 是否为 const 字段
    TZrBool reservedRemovedUsingManaged;  // reserved ABI slot; must remain false
    EZrOwnershipQualifier ownershipQualifier; // 字段所有权限定符
    EZrGcBridgeKind gcBridgeKind;          // 字段显式 Gc<T>/GcBox<T> bridge kind
    EZrOwnershipQualifier receiverQualifier;  // 方法 receiver 所有权限定符
    EZrCanonicalReceiverEffect receiverEffect; // canonical readonly/writable receiver contract
    TZrBool callsClose;                   // 生命周期结束时是否需要先调用 @close
    TZrBool callsDestructor;              // 生命周期结束时是否可能触发 @destructor
    TZrUInt32 declarationOrder;           // 在当前类型中的声明顺序
    
    // 字段特定信息
    SZrType *fieldType;                 // 字段类型（用于偏移量计算，可能为ZR_NULL）
    SZrString *fieldTypeName;           // 字段类型名称（字符串表示，用于运行时类型查找）
    TZrUInt32 fieldOffset;                // 字段偏移量（编译时计算的基本偏移，运行时需要对齐）
    TZrUInt32 fieldSize;                  // 字段大小（字节数）
    
      // 方法特定信息
      SZrFunction *compiledFunction;       // 编译后的函数对象（用于最终序列化时重新落常量池）
    TZrUInt32 functionConstantIndex;      // 函数在常量池中的索引（如果方法是函数）
    TZrSymbolId symbolId;                 // canonical declaration identity
    TZrUInt32 parameterCount;             // 参数数量
    TZrUInt32 minArgumentCount;           // 允许省略尾部可选参数后的最小实参数量
    SZrArray parameterTypes;              // 参数类型数组（SZrInferredType）
    SZrArray parameterNames;              // 参数名称数组（SZrString*）
    SZrArray parameterHasDefaultValues;   // 参数默认值存在标记（TZrBool）
    SZrArray parameterDefaultValues;      // 参数默认值数组（SZrTypeValue）
    SZrArray genericParameters;           // 泛型参数信息（SZrTypeGenericParameterInfo）
    SZrArray parameterPassingModes;       // 参数传递模式（EZrParameterPassingMode）
    SZrArray decorators;                  // 成员级 compile-time decorator 记录（SZrTypeDecoratorInfo）
    TZrBool hasDecoratorMetadata;         // 是否存在成员级 compile-time decorator metadata
    SZrTypeValue decoratorMetadataValue;  // 成员级 compile-time decorator metadata 常量值
    TZrUInt32 contractRole;               // 稳定成员契约角色（EZrMemberContractRole）
    SZrAstNode *declarationNode;          // 声明节点（可选）
    EZrMetaType metaType;               // 元方法类型（如果是元方法，如CONSTRUCTOR）
    TZrBool isMetaMethod;                 // 是否为元方法
    SZrString *returnTypeName;          // 返回类型名称（字符串表示，用于运行时类型查找）
    TZrBool hasStructuredReturnType;
    SZrInferredType structuredReturnType;
    EZrModuleExportKind moduleExportKind; // module prototype member 对应的导出种类
    EZrModuleExportReadiness moduleExportReadiness; // module prototype member 对应的导出就绪阶段
    TZrMetadataToken metadataToken;       // module export/member def token, if available
    TZrMetadataToken signatureToken;      // paired signature token, if available
    TZrUInt32 signatureBlobOffset;        // signature blob range start, if available
    TZrUInt32 signatureBlobLength;        // signature blob range length, if available
    TZrUInt64 signatureHash;              // stable signature fingerprint, if available
    SZrString *ownerTypeName;             // 当前声明所在类型
    SZrString *baseDefinitionOwnerTypeName; // 覆写链根定义所属类型
    SZrString *baseDefinitionName;        // 覆写链根定义名称（属性访问器为隐藏名）
    TZrUInt32 virtualSlotIndex;           // virtual slot；非虚成员为 UINT32_MAX
    TZrUInt32 interfaceContractSlot;      // interface contract slot；当前未绑定为 UINT32_MAX
    TZrUInt32 propertyIdentity;           // property identity；非属性成员为 UINT32_MAX
    EZrPropertyAccessorRole accessorRole; // visible property 为 NONE；访问器为 GET/SET/INIT
    TZrSymbolId propertySymbolId;         // canonical visible PropertySymbol identity
    TZrTypeId propertyValueTypeId;        // canonical declared property value type
    TZrSymbolId getterAccessorSymbolId;   // visible property -> getter symbol
    TZrSymbolId setterAccessorSymbolId;   // visible property -> setter symbol
    TZrSymbolId initAccessorSymbolId;     // visible property -> init symbol
    TZrBool exportsWritableRef;           // 后续 ref-return property 合同；M1 默认 false
} SZrTypeMemberInfo;

typedef enum EZrPropertyAccessorMask {
    ZR_PROPERTY_ACCESSOR_MASK_NONE = 0,
    ZR_PROPERTY_ACCESSOR_MASK_GET = 1U << 0,
    ZR_PROPERTY_ACCESSOR_MASK_SET = 1U << 1,
    ZR_PROPERTY_ACCESSOR_MASK_INIT = 1U << 2,
} EZrPropertyAccessorMask;

typedef struct SZrPropertyRequirementQuery {
    TZrUInt32 matchingContractCount;
    TZrUInt32 requiredAccessorMask;
    TZrUInt32 presentAccessorMask;
    TZrUInt32 missingAccessorMask;
    TZrSymbolId interfacePropertySymbolId;
    TZrSymbolId interfaceGetterSymbolId;
    TZrSymbolId interfaceSetterSymbolId;
    TZrSymbolId interfaceInitializerSymbolId;
} SZrPropertyRequirementQuery;

// 常量引用路径结构
// 使用状态机编码模式，例如：5(长度), -1, -5, 0, -4, 1 表示 parent->childFunction[0]->prototypes[1]
#ifndef ZR_CONSTANT_REFERENCE_PATH_DECLARED
#define ZR_CONSTANT_REFERENCE_PATH_DECLARED
typedef struct SZrConstantReferencePath {
    TZrUInt32 depth;              // 路径深度（总步骤数）
    TZrUInt32 *steps;             // 路径步骤数组（depth个元素）
    // steps[i] 含义：
    //   - 0xFFFFFFFF (-1): parentFunction
    //   - 0xFFFFFFFE (-2): constantValueList[index] (需配合额外参数)
    //   - 0xFFFFFFFD (-3): 模块引用 (需配合模块名和索引)
    //   - 0xFFFFFFFC (-4): 下一个数值读取prototype的index
    //   - 0xFFFFFFFB (-5): 下一个数值读取childFunctionList的index
    //   - 正数: childFunctionList[index]  或者prototypes[index]
    EZrValueType type;          // 常量类型记录
} SZrConstantReferencePath;
#endif

// 引用常量值类型（用于常量池中存储）
typedef struct SZrConstantReference {
    TZrUInt32 pathDepth;          // 路径深度
    TZrUInt32 *pathSteps;         // 路径步骤（如果depth>0）
    TZrUInt32 targetIndex;        // 目标索引（用于常量池、模块等）
    TZrUInt32 referenceType;      // 引用类型（用于区分不同类型的引用）
    EZrValueType type;          // 常量类型记录
} SZrConstantReference;

typedef struct SZrQuickeningLoadTypedArithmeticProbeStats {
    TZrUInt32 getStackTypedArithmeticPairs;
    TZrUInt32 getConstantTypedArithmeticPairs;
    TZrUInt32 safeFusionCandidates;
    TZrUInt32 materializedLoadCandidates;
} SZrQuickeningLoadTypedArithmeticProbeStats;

// 初始化编译器状态
ZR_PARSER_API void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state);

// 清理解译器状态
ZR_PARSER_API void ZrParser_CompilerState_Free(SZrCompilerState *cs);

ZR_PARSER_API const SZrSemanticIrFunction *ZrParser_Compiler_PreSemanticIr(
        const SZrCompilerState *cs);
ZR_PARSER_API TZrBool ZrParser_Compiler_PreSemanticIrIsValidated(
        const SZrCompilerState *cs);
ZR_PARSER_API TZrBool ZrParser_Compiler_ValidatePreSemanticIr(
        SZrCompilerState *cs);

// 编译 AST 为函数
ZR_PARSER_API SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast);
ZR_PARSER_API SZrFunction *ZrParser_Compiler_CompileTest(SZrState *state, SZrAstNode *ast);
ZR_PARSER_API SZrFunction *ZrParser_Compiler_CompileWithCurrentModuleKey(SZrState *state,
                                                                         SZrAstNode *ast,
                                                                         SZrString *currentModuleKey);

// Registers a callable initializer under its binding name for canonical call resolution.
ZR_PARSER_API void ZrParser_Compiler_RegisterCallableValueBinding(
        SZrCompilerState *cs,
        SZrString *name,
        SZrAstNode *valueNode);

// 公开的低层编译入口，用于语义/HIR 相关测试和分阶段编译接线
ZR_PARSER_API void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API SZrAstNodeArray *ZrParser_Compiler_MatchNamedArguments(SZrCompilerState *cs,
                                                                     struct SZrFunctionCall *call,
                                                                     struct SZrAstNodeArray *paramList);
ZR_PARSER_API void ZrParser_Compiler_CompileStructDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Compiler_CompileClassDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Compiler_CompileInterfaceDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API TZrBool ZrParser_Compiler_BindPropertyDeclaration(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *propertyNode,
        SZrString *ownerTypeName,
        SZrString *superTypeName,
        TZrUInt32 declarationOrder);
ZR_PARSER_API TZrBool ZrParser_Compiler_QueryPropertyRequirements(
        const SZrCompilerState *cs,
        const SZrTypePrototypeInfo *ownerPrototype,
        TZrSymbolId propertySymbolId,
        SZrPropertyRequirementQuery *outQuery);
ZR_PARSER_API void ZrParser_Compiler_CompileUnionDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements);
ZR_PARSER_API void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node);

ZR_PARSER_API TZrBool ZrParser_Quickening_CollectLoadTypedArithmeticProbeStats(
        const SZrFunction *function,
        SZrQuickeningLoadTypedArithmeticProbeStats *outStats);

// 报告编译错误
ZR_PARSER_API void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location);
ZR_PARSER_API void ZrParser_Compiler_ClearStructuredError(SZrCompilerState *cs);
ZR_PARSER_API void ZrParser_Compiler_StructuredError(SZrCompilerState *cs,
                                                     const SZrStructuredDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_Compiler_ReportDuplicateTypeDeclaration(
        SZrCompilerState *cs,
        SZrAstNode *declaration,
        SZrAstNode *previousDeclaration);
ZR_PARSER_API TZrBool ZrParser_Compiler_ValidateVariableDeclaration(
        SZrCompilerState *cs,
        const SZrAstNode *declaration);
ZR_PARSER_API TZrBool ZrParser_Compiler_InferCallableReturnType(
        SZrCompilerState *cs,
        const SZrAstNode *declaration,
        SZrInferredType *result);
ZR_PARSER_API TZrBool ZrParser_Compiler_RegisterTypeBinding(
        SZrCompilerState *cs,
        SZrString *name,
        SZrFileRange location,
        SZrAstNode *declaration);
ZR_PARSER_API TZrBool ZrParser_Compiler_PublishCurrentDiagnostic(
        SZrCompilerState *cs);
ZR_PARSER_API void ZrParser_Compiler_PatternShapeMismatch(SZrCompilerState *cs,
                                                          SZrFileRange location,
                                                          const TZrChar *message,
                                                          const TZrChar *cause,
                                                          const TZrChar *suggestion);
ZR_PARSER_API void ZrParser_Compiler_PatternUnknownField(SZrCompilerState *cs,
                                                         SZrFileRange location,
                                                         const TZrChar *fieldName,
                                                         const TZrChar *availableFields);
ZR_PARSER_API void ZrParser_Compiler_PatternArityMismatch(SZrCompilerState *cs,
                                                          SZrFileRange location,
                                                          TZrSize expectedCount,
                                                          TZrSize actualCount,
                                                          const TZrChar *availableFields);
ZR_PARSER_API void ZrParser_Compiler_PatternVariantMismatch(SZrCompilerState *cs,
                                                            SZrFileRange location,
                                                            const TZrChar *annotationUnionName,
                                                            const TZrChar *variantName,
                                                            const TZrChar *resourceUnionName);

ZR_PARSER_API void add_pending_absolute_patch(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId);

// 校验 async/task 相关的借用与 guard 边界约束
ZR_PARSER_API TZrBool compiler_validate_task_effects(SZrCompilerState *cs, SZrAstNode *node);

// 编译期错误级别
enum EZrCompileTimeErrorLevel {
    ZR_COMPILE_TIME_ERROR_INFO,      // 信息
    ZR_COMPILE_TIME_ERROR_WARNING,   // 警告
    ZR_COMPILE_TIME_ERROR_ERROR,     // 错误
    ZR_COMPILE_TIME_ERROR_FATAL      // 致命错误（阻止编译完成）
};

typedef enum EZrCompileTimeErrorLevel EZrCompileTimeErrorLevel;

// 编译期错误报告
ZR_PARSER_API void ZrParser_CompileTime_Error(SZrCompilerState *cs, 
                                     EZrCompileTimeErrorLevel level,
                                     const TZrChar *message,
                                     SZrFileRange location);

ZR_PARSER_API void ZrParser_ExternalVariables_Analyze(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler);

// 执行编译期声明
ZR_PARSER_API TZrBool ZrParser_CompileTimeDeclaration_Execute(SZrCompilerState *cs, SZrAstNode *node);

// Evaluate compile-time build facts before the runtime compiler consumes the AST.
ZR_PARSER_API TZrBool ZrParser_CompileTime_PrepareBuildFacts(SZrState *state, SZrAstNode *ast);
ZR_PARSER_API TZrBool ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
        SZrCompilerState *cs,
        SZrAstNode *ast);

// 查询已注册的编译期变量值；如果尚未求值，会按当前编译期环境求值并缓存
ZR_PARSER_API TZrBool ZrParser_Compiler_TryGetCompileTimeValue(SZrCompilerState *cs, SZrString *name, SZrTypeValue *result);

// 在编译期上下文中直接求值 AST 表达式
ZR_PARSER_API TZrBool ZrParser_Compiler_EvaluateCompileTimeExpression(SZrCompilerState *cs, SZrAstNode *node, SZrTypeValue *result);

// 校验编译期值是否可以安全投影到运行时常量池；失败时会直接写入编译错误
ZR_PARSER_API TZrBool ZrParser_Compiler_ValidateRuntimeProjectionValue(SZrCompilerState *cs,
                                                             const SZrTypeValue *value,
                                                             SZrFileRange location);

ZR_PARSER_API TZrBool ZrParser_Compiler_ApplyCompileTimeTypeDecorators(SZrCompilerState *cs,
                                                                       SZrAstNode *typeNode,
                                                                       SZrAstNodeArray *decorators,
                                                                       SZrTypePrototypeInfo *info);
ZR_PARSER_API TZrBool ZrParser_CompileTime_ApplyMemberDecorators(SZrCompilerState *cs,
                                                                 SZrAstNode *memberNode,
                                                                 SZrAstNodeArray *decorators,
                                                                 SZrTypeMemberInfo *memberInfo);
ZR_PARSER_API TZrBool ZrParser_Compiler_IsCompileTimeDecorator(SZrCompilerState *cs,
                                                               SZrAstNode *decoratorNode);

typedef enum EZrParserSubmissionBindingKind {
    ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE = 0,
    ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE = 1
} EZrParserSubmissionBindingKind;

#define ZR_PARSER_SUBMISSION_CALLABLE_SIGNATURE_NONE ((TZrUInt32)0xFFFFFFFFu)

/* Borrowed canonical callable contract for one persisted REPL binding. */
typedef struct SZrParserSubmissionCallableSignature {
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    SZrInferredType returnType;
    SZrArray parameterTypes;         /* SZrInferredType */
    SZrArray parameterPassingModes;  /* EZrParameterPassingMode */
    SZrFileRange declarationRange;
} SZrParserSubmissionCallableSignature;

/* Input bindings are borrowed for the duration of one submission compile. */
typedef struct SZrParserSubmissionBinding {
    SZrString *name;
    EZrParserSubmissionBindingKind kind;
    SZrInferredType inferredType;
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrUInt32 placeId;
    SZrFileRange declarationRange;
    TZrUInt32 captureIndex;
    TZrUInt32 callableSignatureIndex;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 cellGeneration;
} SZrParserSubmissionBinding;

typedef struct SZrParserSubmissionContext {
    const SZrParserSubmissionBinding *bindings;
    TZrSize bindingCount;
    const SZrParserSubmissionCallableSignature *callableSignatures;
    TZrSize callableSignatureCount;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 cellGeneration;
} SZrParserSubmissionContext;

typedef struct SZrParserSubmissionResult {
    SZrParserSubmissionBinding *bindings;
    TZrSize bindingCount;
    SZrParserSubmissionCallableSignature *callableSignatures;
    TZrSize callableSignatureCount;
} SZrParserSubmissionResult;

// 编译源代码为函数（封装了从解析到编译的全流程）
// 这是提供给 globalState 的统一接口
ZR_PARSER_API struct SZrFunction *ZrParser_Source_Compile(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName);
ZR_PARSER_API struct SZrFunction *ZrParser_Source_CompileTest(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName);
ZR_PARSER_API struct SZrFunction *ZrParser_Source_CompileSubmission(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName,
        const SZrParserSubmissionContext *context,
        SZrParserSubmissionResult *outResult);
ZR_PARSER_API void ZrParser_SubmissionResult_Free(
        struct SZrState *state,
        SZrParserSubmissionResult *result);
/* Seeds an existing compiler state with one validated, read-only submission snapshot. */
ZR_PARSER_API TZrBool ZrParser_CompilerState_SeedSubmissionContext(
        SZrCompilerState *cs,
        const SZrParserSubmissionContext *context);

// 注册 compileSource 函数到 globalState
// 在 global 初始化时调用此函数来注册 parser 模块
ZR_PARSER_API void ZrParser_ToGlobalState_Register(struct SZrState *state);

// 内部辅助函数（在 compiler.c 中实现）
// 这些函数用于指令生成、常量管理、变量管理等
ZR_PARSER_API TZrSize ZrParser_Compiler_GetLocalStackFloor(const SZrCompilerState *cs);
ZR_PARSER_API void ZrParser_Compiler_TrimStackToCount(SZrCompilerState *cs, TZrSize targetCount);
ZR_PARSER_API void ZrParser_Compiler_TrimStackToSlot(SZrCompilerState *cs, TZrUInt32 slot);
ZR_PARSER_API void ZrParser_Compiler_TrimStackBy(SZrCompilerState *cs, TZrSize amount);

#endif //ZR_VM_PARSER_COMPILER_H
