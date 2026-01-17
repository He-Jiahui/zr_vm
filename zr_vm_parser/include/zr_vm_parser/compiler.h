//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_COMPILER_H
#define ZR_VM_PARSER_COMPILER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
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
    
    // 常量池管理
    SZrArray constants;                 // 常量值数组（SZrTypeValue）
    TZrSize constantCount;               // 常量数量
    
    // 局部变量管理
    SZrArray localVars;                 // 局部变量数组（SZrFunctionLocalVariable）
    TZrSize localVarCount;              // 局部变量数量
    TZrSize stackSlotCount;             // 当前栈槽数量
    
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
    
    // 循环标签栈（用于 break/continue）
    SZrArray loopLabelStack;            // 循环标签栈（SZrLoopLabel）
    
    // 嵌套函数
    SZrArray childFunctions;            // 子函数数组（SZrFunction*）
    
    // 函数名到子函数索引的映射（仅用于编译时查找，运行时不需要）
    SZrArray childFunctionNameMap;      // 函数名映射数组（SZrChildFunctionNameMap）
    
    // 顶层函数声明（如果脚本只有一个顶层函数声明，保存它以便返回）
    SZrFunction *topLevelFunction;      // 顶层函数对象（如果存在）
    
    // 错误处理
    TBool hasError;
    const TChar *errorMessage;
    SZrFileRange errorLocation;
    TBool hasFatalError;                  // 是否有致命错误（阻止编译完成）
    
    // 测试模式
    TBool isTestMode;                    // 是否处于测试模式
    SZrArray testFunctions;              // 测试函数数组（SZrFunction*）
    
    // 尾调用优化上下文
    TBool isInTailCallContext;           // 是否处于尾调用上下文（return语句中的表达式）
    
    // 外部变量分析（用于闭包捕获）
    SZrArray referencedExternalVars;     // 引用的外部变量名数组（SZrString*），用于lambda编译时
    
    // 类型环境
    SZrTypeEnvironment *typeEnv;         // 当前类型环境
    SZrArray typeEnvStack;               // 类型环境栈（用于作用域管理）
    
    // 模块导出跟踪（仅用于脚本级变量）
    SZrArray pubVariables;               // pub 变量列表（SZrExportedVariable）
    SZrArray proVariables;                // pro 变量列表（SZrExportedVariable，包含所有 pub）
    SZrArray exportedTypes;              // TODO: 导出的类型列表（暂时作为占位）
    TBool isScriptLevel;                  // 是否在脚本级别（用于区分脚本级变量和函数内变量）
    
    // 脚本 AST 引用（用于类型查找）
    SZrAstNode *scriptAst;                // 当前编译的脚本 AST 节点（用于查找类型定义）
    
    // 类型 Prototype 信息（用于运行时创建）
    SZrArray typePrototypes;              // 待创建的 prototype 信息数组（SZrTypePrototypeInfo）
    
    // 编译期环境管理
    SZrTypeEnvironment *compileTimeTypeEnv;   // 编译期类型环境
    SZrArray compileTimeVariables;            // 编译期变量表（SZrCompileTimeVariable*）
    SZrArray compileTimeFunctions;            // 编译期函数表（SZrCompileTimeFunction*）
    TBool isInCompileTimeContext;             // 是否在编译期上下文中
} SZrCompilerState;

// 编译期变量信息
typedef struct SZrCompileTimeVariable {
    SZrString *name;                       // 变量名
    SZrInferredType type;                  // 变量类型
    SZrAstNode *value;                     // 变量值（AST节点，用于编译期求值）
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

// 作用域信息
typedef struct SZrScope {
    TZrSize startVarIndex;              // 作用域开始的变量索引
    TZrSize varCount;                   // 作用域内的变量数量
    SZrCompilerState *parentCompiler;   // 父编译器（用于闭包）
} SZrScope;

// 跳转标签
typedef struct SZrLabel {
    TZrSize instructionIndex;           // 指令索引
    TBool isResolved;                   // 是否已解析
} SZrLabel;

// 待解析的跳转
typedef struct SZrPendingJump {
    TZrSize instructionIndex;           // 跳转指令的索引
    TZrSize labelId;                    // 目标标签 ID
} SZrPendingJump;

// 循环标签（用于 break/continue）
typedef struct SZrLoopLabel {
    TZrSize breakLabelId;               // break 目标标签 ID
    TZrSize continueLabelId;            // continue 目标标签 ID
} SZrLoopLabel;

// 导出变量信息（用于模块导出）
typedef struct SZrExportedVariable {
    SZrString *name;                    // 变量名
    TUInt32 stackSlot;                  // 栈槽位
    EZrAccessModifier accessModifier;   // 可见性修饰符
} SZrExportedVariable;

// 函数名到子函数索引的映射（仅用于编译时查找，运行时不需要）
typedef struct SZrChildFunctionNameMap {
    SZrString *name;                    // 函数名
    TUInt32 childFunctionIndex;         // 子函数在 childFunctions 中的索引
} SZrChildFunctionNameMap;

// 编译时存储的 Prototype 信息
typedef struct SZrTypePrototypeInfo {
    SZrString *name;                    // 类型名称
    EZrObjectPrototypeType type;        // STRUCT 或 CLASS
    EZrAccessModifier accessModifier;   // 访问修饰符
    SZrArray inherits;                  // 继承的类型引用（SZrString* 数组，存储类型名称字符串）
    SZrArray members;                   // 成员信息（字段、方法等，存储 SZrTypeMemberInfo）
} SZrTypePrototypeInfo;

// 成员信息（字段、方法、元函数等）
typedef struct SZrTypeMemberInfo {
    EZrAstNodeType memberType;          // 成员类型（STRUCT_FIELD, STRUCT_METHOD, CLASS_FIELD 等）
    SZrString *name;                    // 成员名称
    EZrAccessModifier accessModifier;   // 访问修饰符
    TBool isStatic;                     // 是否为静态成员
    
    // 字段特定信息
    SZrType *fieldType;                 // 字段类型（用于偏移量计算，可能为ZR_NULL）
    SZrString *fieldTypeName;           // 字段类型名称（字符串表示，用于运行时类型查找）
    TUInt32 fieldOffset;                // 字段偏移量（编译时计算的基本偏移，运行时需要对齐）
    TUInt32 fieldSize;                  // 字段大小（字节数）
    
    // 方法特定信息
    TUInt32 functionConstantIndex;      // 函数在常量池中的索引（如果方法是函数）
    TUInt32 parameterCount;             // 参数数量
    EZrMetaType metaType;               // 元方法类型（如果是元方法，如CONSTRUCTOR）
    TBool isMetaMethod;                 // 是否为元方法
    SZrString *returnTypeName;          // 返回类型名称（字符串表示，用于运行时类型查找）
} SZrTypeMemberInfo;

// 编译结果结构体
typedef struct SZrCompileResult {
    SZrFunction *mainFunction;          // 主函数（脚本主体）
    SZrFunction **testFunctions;        // 测试函数数组（SZrFunction*）
    TZrSize testFunctionCount;          // 测试函数数量
} SZrCompileResult;

// 常量引用路径步骤类型
// 注意：使用负数作为特殊标记，实际使用时会转换为TUInt32（作为无符号整数存储）
enum EZrConstantReferenceStepType {
    ZR_CONSTANT_REF_STEP_PARENT = -1,        // 向上引用parent function
    ZR_CONSTANT_REF_STEP_CHILD = 0,          // 0: childFunctionList[index] (需配合额外参数，实际为正数)
    ZR_CONSTANT_REF_STEP_CONSTANT_POOL = -2, // 当前函数的常量池索引
    ZR_CONSTANT_REF_STEP_MODULE = -3,        // 模块引用
    ZR_CONSTANT_REF_STEP_PROTOTYPE_INDEX = -4, // 下一个数值读取prototype的index
    ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX = -5, // 下一个数值读取childFunctionList的index
    // 未来可扩展其他类型
};

typedef enum EZrConstantReferenceStepType EZrConstantReferenceStepType;

// 辅助宏：将步骤类型转换为TUInt32（用于存储）
#define ZR_CONSTANT_REF_STEP_TO_UINT32(step) ((TUInt32)(TInt32)(step))
#define ZR_CONSTANT_REF_STEP_FROM_UINT32(step) ((TInt32)(TUInt32)(step))

// 常量引用路径结构
// 使用状态机编码模式，例如：5(长度), -1, -5, 0, -4, 1 表示 parent->childFunction[0]->prototypes[1]
typedef struct SZrConstantReferencePath {
    TUInt32 depth;              // 路径深度（总步骤数）
    TUInt32 *steps;             // 路径步骤数组（depth个元素）
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
    TUInt32 pathDepth;          // 路径深度
    TUInt32 *pathSteps;         // 路径步骤（如果depth>0）
    TUInt32 targetIndex;        // 目标索引（用于常量池、模块等）
    TUInt32 referenceType;      // 引用类型（用于区分不同类型的引用）
    EZrValueType type;          // 常量类型记录
} SZrConstantReference;

// 初始化编译器状态
ZR_PARSER_API void ZrCompilerStateInit(SZrCompilerState *cs, SZrState *state);

// 清理解译器状态
ZR_PARSER_API void ZrCompilerStateFree(SZrCompilerState *cs);

// 编译 AST 为函数
ZR_PARSER_API SZrFunction *ZrCompilerCompile(SZrState *state, SZrAstNode *ast);

// 编译 AST 为函数和测试函数列表（新接口）
// 返回编译结果结构体，调用者需要调用 ZrCompileResultFree 来释放资源
ZR_PARSER_API TBool ZrCompilerCompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result);

// 释放编译结果（释放测试函数数组，但不释放函数对象本身，函数对象由GC管理）
ZR_PARSER_API void ZrCompileResultFree(SZrState *state, SZrCompileResult *result);

// 报告编译错误
ZR_PARSER_API void ZrCompilerError(SZrCompilerState *cs, const TChar *msg, SZrFileRange location);

// 编译期错误级别
enum EZrCompileTimeErrorLevel {
    ZR_COMPILE_TIME_ERROR_INFO,      // 信息
    ZR_COMPILE_TIME_ERROR_WARNING,   // 警告
    ZR_COMPILE_TIME_ERROR_ERROR,     // 错误
    ZR_COMPILE_TIME_ERROR_FATAL      // 致命错误（阻止编译完成）
};

typedef enum EZrCompileTimeErrorLevel EZrCompileTimeErrorLevel;

// 编译期错误报告
ZR_PARSER_API void ZrCompileTimeError(SZrCompilerState *cs, 
                                     EZrCompileTimeErrorLevel level,
                                     const TChar *message,
                                     SZrFileRange location);

ZR_PARSER_API void analyze_external_variables(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler);

// 执行编译期声明
ZR_PARSER_API TBool execute_compile_time_declaration(SZrCompilerState *cs, SZrAstNode *node);

// 编译源代码为函数（封装了从解析到编译的全流程）
// 这是提供给 globalState 的统一接口
ZR_PARSER_API struct SZrFunction *ZrParserCompileSource(struct SZrState *state, const TChar *source, TZrSize sourceLength, struct SZrString *sourceName);

// 注册 compileSource 函数到 globalState
// 在 global 初始化时调用此函数来注册 parser 模块
ZR_PARSER_API void ZrParserRegisterToGlobalState(struct SZrState *state);

// 内部辅助函数（在 compiler.c 中实现）
// 这些函数用于指令生成、常量管理、变量管理等

#endif //ZR_VM_PARSER_COMPILER_H

