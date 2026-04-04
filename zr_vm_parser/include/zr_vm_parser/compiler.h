//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_COMPILER_H
#define ZR_VM_PARSER_COMPILER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
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

// 编译器状态结构
typedef struct SZrCompilerState {
    SZrState *state;                    // VM 状态
    SZrFunction *currentFunction;       // 当前编译的函数
    SZrAstNode *currentAst;             // 当前编译的 AST 节点
    SZrSemanticContext *semanticContext; // 统一语义记录上下文
    SZrHirModule *hirModule;            // 当前脚本的 HIR 模块句柄
    
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
    
    // 闭包管理
    SZrArray closureVars;               // 闭包变量数组（SZrFunctionClosureVariable）
    TZrSize closureVarCount;             // 闭包变量数量
    
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
    TZrBool hasFatalError;                  // 是否有致命错误（阻止编译完成）
    TZrBool hasCompileTimeError;            // 是否发生过编译期错误（不能在后续语句中被吞掉）
    
    // 测试模式
    TZrBool isTestMode;                    // 是否处于测试模式
    SZrArray testFunctions;              // 测试函数数组（SZrFunction*）
    
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
    
    // 类型 Prototype 信息（用于运行时创建）
    SZrArray typePrototypes;              // 待创建的 prototype 信息数组（SZrTypePrototypeInfo）
    struct SZrTypePrototypeInfo *currentTypePrototypeInfo; // 当前正在构建的类型原型
    TZrBool externBindingsPredeclared;    // 是否已预注册 source-level extern 编译期绑定
    
    // 编译期环境管理
    SZrTypeEnvironment *compileTimeTypeEnv;   // 编译期类型环境
    SZrArray compileTimeVariables;            // 编译期变量表（SZrCompileTimeVariable*）
    SZrArray compileTimeFunctions;            // 编译期函数表（SZrCompileTimeFunction*）
    SZrArray compileTimeDecoratorClasses;     // 编译期装饰器类表（SZrCompileTimeDecoratorClass*）
    TZrBool isInCompileTimeContext;             // 是否在编译期上下文中
    
    // 构造函数上下文
    TZrBool isInConstructor;                     // 是否在构造函数中编译
    SZrAstNode *currentFunctionNode;          // 当前编译的函数 AST 节点（用于访问参数信息）
    SZrString *currentTypeName;               // 当前编译的类型名称（用于成员字段 const 检查）
    SZrAstNode *currentTypeNode;              // 当前编译的类型声明节点（用于 const 成员初始化检查）
    
    // const 变量跟踪（用于编译时检查）
    SZrArray constLocalVars;                   // const 局部变量名数组（SZrString*）
    SZrArray constParameters;                  // const 参数名数组（SZrString*）
    SZrArray constructorInitializedConstFields; // 构造函数中已初始化的 const 成员名数组（SZrString*）
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
    SZrFileRange location;                  // 声明位置
} SZrCompileTimeFunction;

typedef struct SZrCompileTimeDecoratorClass {
    SZrString *name;                       // 装饰器类名
    SZrAstNode *declaration;               // ClassDeclaration / StructDeclaration
    SZrAstNode *decorateMethod;            // @decorate 元方法节点
    SZrAstNode *constructorMethod;         // @constructor 元方法节点（可选）
    TZrBool isStructDecorator;             // 是否来自 %compileTime struct
    SZrFileRange location;                 // 声明位置
} SZrCompileTimeDecoratorClass;

typedef struct SZrTypeDecoratorInfo {
    SZrString *name;                       // 装饰器名称
} SZrTypeDecoratorInfo;

// 作用域信息
typedef struct SZrScope {
    TZrSize startVarIndex;              // 作用域开始的变量索引
    TZrSize varCount;                   // 作用域内的变量数量
    TZrSize cleanupRegistrationCount;   // 作用域内 using 注册的清理数量
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
    SZrArray constraintTypeNames;       // 约束类型名称数组（SZrString*）
} SZrTypeGenericParameterInfo;

typedef struct SZrTypePrototypeInfo {
    SZrString *name;                    // 类型名称
    EZrObjectPrototypeType type;        // STRUCT 或 CLASS
    EZrAccessModifier accessModifier;   // 访问修饰符
    TZrBool isImportedNative;           // 是否为仅用于编译期解析的原生导入类型
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
} SZrTypePrototypeInfo;

// 成员信息（字段、方法、元函数等）
typedef struct SZrTypeMemberInfo {
    EZrAstNodeType memberType;          // 成员类型（STRUCT_FIELD, STRUCT_METHOD, CLASS_FIELD 等）
    SZrString *name;                    // 成员名称
    EZrAccessModifier accessModifier;   // 访问修饰符
    TZrBool isStatic;                     // 是否为静态成员
    TZrBool isConst;                      // 是否为 const 字段
    TZrBool isUsingManaged;               // 是否显式使用 field-scoped using
    EZrOwnershipQualifier ownershipQualifier; // 字段所有权限定符
    EZrOwnershipQualifier receiverQualifier;  // 方法 receiver 所有权限定符
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
      TZrUInt32 parameterCount;             // 参数数量
      SZrArray parameterTypes;              // 参数类型数组（SZrInferredType）
      SZrArray genericParameters;           // 泛型参数信息（SZrTypeGenericParameterInfo）
      SZrArray parameterPassingModes;       // 参数传递模式（EZrParameterPassingMode）
      SZrAstNode *declarationNode;          // 声明节点（可选）
      EZrMetaType metaType;               // 元方法类型（如果是元方法，如CONSTRUCTOR）
    TZrBool isMetaMethod;                 // 是否为元方法
    SZrString *returnTypeName;          // 返回类型名称（字符串表示，用于运行时类型查找）
} SZrTypeMemberInfo;

// 编译结果结构体
typedef struct SZrCompileResult {
    SZrFunction *mainFunction;          // 主函数（脚本主体）
    SZrFunction **testFunctions;        // 测试函数数组（SZrFunction*）
    TZrSize testFunctionCount;          // 测试函数数量
} SZrCompileResult;

// 常量引用路径结构
// 使用状态机编码模式，例如：5(长度), -1, -5, 0, -4, 1 表示 parent->childFunction[0]->prototypes[1]
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

// 引用常量值类型（用于常量池中存储）
typedef struct SZrConstantReference {
    TZrUInt32 pathDepth;          // 路径深度
    TZrUInt32 *pathSteps;         // 路径步骤（如果depth>0）
    TZrUInt32 targetIndex;        // 目标索引（用于常量池、模块等）
    TZrUInt32 referenceType;      // 引用类型（用于区分不同类型的引用）
    EZrValueType type;          // 常量类型记录
} SZrConstantReference;

// 初始化编译器状态
ZR_PARSER_API void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state);

// 清理解译器状态
ZR_PARSER_API void ZrParser_CompilerState_Free(SZrCompilerState *cs);

// 编译 AST 为函数
ZR_PARSER_API SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast);

// 公开的低层编译入口，用于语义/HIR 相关测试和分阶段编译接线
ZR_PARSER_API void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API SZrAstNodeArray *ZrParser_Compiler_MatchNamedArguments(SZrCompilerState *cs,
                                                                     struct SZrFunctionCall *call,
                                                                     struct SZrAstNodeArray *paramList);
ZR_PARSER_API void ZrParser_Compiler_CompileStructDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Compiler_CompileClassDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Compiler_CompileInterfaceDeclaration(SZrCompilerState *cs, SZrAstNode *node);
ZR_PARSER_API void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements);
ZR_PARSER_API void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node);

// 编译 AST 为函数和测试函数列表（新接口）
// 返回编译结果结构体，调用者需要调用 ZrParser_CompileResult_Free 来释放资源
ZR_PARSER_API TZrBool ZrParser_Compiler_CompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result);

// 释放编译结果（释放测试函数数组，但不释放函数对象本身，函数对象由GC管理）
ZR_PARSER_API void ZrParser_CompileResult_Free(SZrState *state, SZrCompileResult *result);

// 报告编译错误
ZR_PARSER_API void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location);

ZR_PARSER_API void add_pending_absolute_patch(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId);

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
ZR_PARSER_API TZrBool ZrParser_Compiler_IsCompileTimeDecorator(SZrCompilerState *cs,
                                                               SZrAstNode *decoratorNode);

// 编译源代码为函数（封装了从解析到编译的全流程）
// 这是提供给 globalState 的统一接口
ZR_PARSER_API struct SZrFunction *ZrParser_Source_Compile(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName);

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
