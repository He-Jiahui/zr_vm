//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_COMPILER_H
#define ZR_VM_PARSER_COMPILER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/array.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_array_conf.h"

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
    
    // 错误处理
    TBool hasError;
    const TChar *errorMessage;
    SZrFileRange errorLocation;
    
    // 测试模式
    TBool isTestMode;                    // 是否处于测试模式
    SZrArray testFunctions;              // 测试函数数组（SZrFunction*）
} SZrCompilerState;

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

// 编译结果结构体
typedef struct SZrCompileResult {
    SZrFunction *mainFunction;          // 主函数（脚本主体）
    SZrFunction **testFunctions;        // 测试函数数组（SZrFunction*）
    TZrSize testFunctionCount;          // 测试函数数量
} SZrCompileResult;

// 初始化编译器状态
ZR_PARSER_API void ZrCompilerStateInit(SZrCompilerState *cs, SZrState *state);

// 清理解译器状态
ZR_PARSER_API void ZrCompilerStateFree(SZrCompilerState *cs);

// 编译 AST 为函数（旧接口，保持向后兼容）
ZR_PARSER_API SZrFunction *ZrCompilerCompile(SZrState *state, SZrAstNode *ast);

// 编译 AST 为函数和测试函数列表（新接口）
// 返回编译结果结构体，调用者需要调用 ZrCompileResultFree 来释放资源
ZR_PARSER_API TBool ZrCompilerCompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result);

// 释放编译结果（释放测试函数数组，但不释放函数对象本身，函数对象由GC管理）
ZR_PARSER_API void ZrCompileResultFree(SZrState *state, SZrCompileResult *result);

// 报告编译错误
ZR_PARSER_API void ZrCompilerError(SZrCompilerState *cs, const TChar *msg, SZrFileRange location);

// 内部辅助函数（在 compiler.c 中实现）
// 这些函数用于指令生成、常量管理、变量管理等

#endif //ZR_VM_PARSER_COMPILER_H

