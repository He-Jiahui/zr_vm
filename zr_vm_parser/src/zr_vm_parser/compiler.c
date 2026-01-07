//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include <stdio.h>
#include <string.h>
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <string.h>

// 前向声明（这些函数在其他文件中实现）
extern void compile_expression(SZrCompilerState *cs, SZrAstNode *node);
extern void compile_statement(SZrCompilerState *cs, SZrAstNode *node);
static void compile_script(SZrCompilerState *cs, SZrAstNode *node);
void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node);
static void compile_test_declaration(SZrCompilerState *cs, SZrAstNode *node);
static void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node);
static void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node);
TZrSize create_label(SZrCompilerState *cs);
void resolve_label(SZrCompilerState *cs, TZrSize labelId);

// 初始化编译器状态
void ZrCompilerStateInit(SZrCompilerState *cs, SZrState *state) {
    ZR_ASSERT(cs != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);

    cs->state = state;
    cs->currentFunction = ZR_NULL;
    cs->currentAst = ZR_NULL;

    // 初始化常量池
    ZrArrayInit(state, &cs->constants, sizeof(SZrTypeValue), 16);
    cs->constantCount = 0;

    // 初始化局部变量数组
    ZrArrayInit(state, &cs->localVars, sizeof(SZrFunctionLocalVariable), 16);
    cs->localVarCount = 0;
    cs->stackSlotCount = 0;

    // 初始化闭包变量数组
    ZrArrayInit(state, &cs->closureVars, sizeof(SZrFunctionClosureVariable), 8);
    cs->closureVarCount = 0;

    // 初始化指令数组
    ZrArrayInit(state, &cs->instructions, sizeof(TZrInstruction), 64);
    cs->instructionCount = 0;

    // 初始化作用域栈
    ZrArrayInit(state, &cs->scopeStack, sizeof(SZrScope), 8);

    // 初始化标签数组
    ZrArrayInit(state, &cs->labels, sizeof(SZrLabel), 8);

    // 初始化待解析跳转数组
    ZrArrayInit(state, &cs->pendingJumps, sizeof(SZrPendingJump), 8);

    // 初始化循环标签栈
    ZrArrayInit(state, &cs->loopLabelStack, sizeof(SZrLoopLabel), 4);

    // 初始化子函数数组
    ZrArrayInit(state, &cs->childFunctions, sizeof(SZrFunction *), 8);
    
    // 初始化函数名到子函数索引的映射数组（仅用于编译时查找）
    ZrArrayInit(state, &cs->childFunctionNameMap, sizeof(SZrChildFunctionNameMap), 8);

    // 初始化顶层函数声明
    cs->topLevelFunction = ZR_NULL;

    // 初始化错误状态
    cs->hasError = ZR_FALSE;
    cs->errorMessage = ZR_NULL;
    cs->errorLocation.start.line = 0;
    cs->errorLocation.start.column = 0;
    cs->errorLocation.end.line = 0;
    cs->errorLocation.end.column = 0;

    // 初始化测试模式
    cs->isTestMode = ZR_FALSE;
    ZrArrayInit(state, &cs->testFunctions, sizeof(SZrFunction *), 8);
    
    // 初始化尾调用上下文
    cs->isInTailCallContext = ZR_FALSE;
    
    // 初始化外部变量引用数组
    ZrArrayInit(state, &cs->referencedExternalVars, sizeof(SZrString *), 8);
    
    // 初始化类型环境
    cs->typeEnv = ZrTypeEnvironmentNew(state);
    ZrArrayInit(state, &cs->typeEnvStack, sizeof(SZrTypeEnvironment *), 8);
    
    // 初始化模块导出跟踪数组
    ZrArrayInit(state, &cs->pubVariables, sizeof(SZrExportedVariable), 8);
    ZrArrayInit(state, &cs->proVariables, sizeof(SZrExportedVariable), 8);
    ZrArrayInit(state, &cs->exportedTypes, sizeof(SZrString *), 4);  // 暂时存储类型名
    
    // 初始化脚本 AST 引用
    cs->scriptAst = ZR_NULL;
    
    // 初始化脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
    
    // 初始化类型 Prototype 信息数组
    ZrArrayInit(state, &cs->typePrototypes, sizeof(SZrTypePrototypeInfo), 8);
}

// 清理解译器状态
void ZrCompilerStateFree(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    SZrState *state = cs->state;
    if (state == ZR_NULL) {
        return;
    }

    // 释放常量池（注意：常量值可能包含 GC 对象，但由 SZrFunction 管理）
    if (cs->constants.isValid && cs->constants.head != ZR_NULL && cs->constants.capacity > 0 &&
        cs->constants.elementSize > 0) {
        ZrArrayFree(state, &cs->constants);
    }

    // 释放局部变量数组
    if (cs->localVars.isValid && cs->localVars.head != ZR_NULL && cs->localVars.capacity > 0 &&
        cs->localVars.elementSize > 0) {
        ZrArrayFree(state, &cs->localVars);
    }

    // 释放闭包变量数组
    if (cs->closureVars.isValid && cs->closureVars.head != ZR_NULL && cs->closureVars.capacity > 0 &&
        cs->closureVars.elementSize > 0) {
        ZrArrayFree(state, &cs->closureVars);
    }

    // 释放指令数组
    if (cs->instructions.isValid && cs->instructions.head != ZR_NULL && cs->instructions.capacity > 0 &&
        cs->instructions.elementSize > 0) {
        ZrArrayFree(state, &cs->instructions);
    }

    // 释放作用域栈
    if (cs->scopeStack.isValid && cs->scopeStack.head != ZR_NULL && cs->scopeStack.capacity > 0 &&
        cs->scopeStack.elementSize > 0) {
        ZrArrayFree(state, &cs->scopeStack);
    }

    // 释放标签数组
    if (cs->labels.isValid && cs->labels.head != ZR_NULL && cs->labels.capacity > 0 && cs->labels.elementSize > 0) {
        ZrArrayFree(state, &cs->labels);
    }

    // 释放待解析跳转数组
    if (cs->pendingJumps.isValid && cs->pendingJumps.head != ZR_NULL && cs->pendingJumps.capacity > 0 &&
        cs->pendingJumps.elementSize > 0) {
        ZrArrayFree(state, &cs->pendingJumps);
    }

    // 释放循环标签栈
    if (cs->loopLabelStack.isValid && cs->loopLabelStack.head != ZR_NULL && cs->loopLabelStack.capacity > 0 &&
        cs->loopLabelStack.elementSize > 0) {
        ZrArrayFree(state, &cs->loopLabelStack);
    }

    // 释放子函数数组（函数本身由 GC 管理）
    if (cs->childFunctions.isValid && cs->childFunctions.head != ZR_NULL && cs->childFunctions.capacity > 0 &&
        cs->childFunctions.elementSize > 0) {
        ZrArrayFree(state, &cs->childFunctions);
    }
    
    // 释放测试函数数组（函数本身由 GC 管理）
    if (cs->testFunctions.isValid && cs->testFunctions.head != ZR_NULL && cs->testFunctions.capacity > 0 &&
        cs->testFunctions.elementSize > 0) {
        ZrArrayFree(state, &cs->testFunctions);
    }
    
    // 释放外部变量引用数组（字符串本身由 GC 管理）
    if (cs->referencedExternalVars.isValid && cs->referencedExternalVars.head != ZR_NULL && 
        cs->referencedExternalVars.capacity > 0 && cs->referencedExternalVars.elementSize > 0) {
        ZrArrayFree(state, &cs->referencedExternalVars);
    }
    
    // 释放类型环境栈
    if (cs->typeEnvStack.isValid && cs->typeEnvStack.head != ZR_NULL && 
        cs->typeEnvStack.capacity > 0 && cs->typeEnvStack.elementSize > 0) {
        // 释放栈中的所有环境（从栈顶到栈底）
        for (TZrSize i = 0; i < cs->typeEnvStack.length; i++) {
            SZrTypeEnvironment **envPtr = (SZrTypeEnvironment **)ZrArrayGet(&cs->typeEnvStack, i);
            if (envPtr != ZR_NULL && *envPtr != ZR_NULL) {
                ZrTypeEnvironmentFree(state, *envPtr);
            }
        }
        ZrArrayFree(state, &cs->typeEnvStack);
    }
    
    // 释放当前类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrTypeEnvironmentFree(state, cs->typeEnv);
        cs->typeEnv = ZR_NULL;
    }
    
    // 释放类型 Prototype 信息数组
    if (cs->typePrototypes.isValid && cs->typePrototypes.head != ZR_NULL &&
        cs->typePrototypes.capacity > 0 && cs->typePrototypes.elementSize > 0) {
        // 释放每个 prototype 信息中的嵌套数组
        for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
            SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrArrayGet(&cs->typePrototypes, i);
            if (info != ZR_NULL) {
                // 释放 inherits 数组（字符串本身由 GC 管理）
                if (info->inherits.isValid && info->inherits.head != ZR_NULL &&
                    info->inherits.capacity > 0 && info->inherits.elementSize > 0) {
                    ZrArrayFree(state, &info->inherits);
                }
                // 释放 members 数组
                if (info->members.isValid && info->members.head != ZR_NULL &&
                    info->members.capacity > 0 && info->members.elementSize > 0) {
                    ZrArrayFree(state, &info->members);
                }
            }
        }
        ZrArrayFree(state, &cs->typePrototypes);
    }
    
    // 释放模块导出跟踪数组（字符串本身由 GC 管理）
    if (cs->pubVariables.isValid && cs->pubVariables.head != ZR_NULL && 
        cs->pubVariables.capacity > 0 && cs->pubVariables.elementSize > 0) {
        ZrArrayFree(state, &cs->pubVariables);
    }
    if (cs->proVariables.isValid && cs->proVariables.head != ZR_NULL && 
        cs->proVariables.capacity > 0 && cs->proVariables.elementSize > 0) {
        ZrArrayFree(state, &cs->proVariables);
    }
    if (cs->exportedTypes.isValid && cs->exportedTypes.head != ZR_NULL && 
        cs->exportedTypes.capacity > 0 && cs->exportedTypes.elementSize > 0) {
        ZrArrayFree(state, &cs->exportedTypes);
    }
}

// 报告编译错误
// 辅助函数：根据错误消息提供解决建议
static void print_error_suggestion(const TChar *msg) {
    if (msg == ZR_NULL) {
        return;
    }
    
    // 根据错误消息内容提供解决建议
    if (strstr(msg, "INTERFACE_METHOD_SIGNATURE") != ZR_NULL ||
        strstr(msg, "INTERFACE_FIELD_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_PROPERTY_SIGNATURE") != ZR_NULL ||
        strstr(msg, "STRUCT_FIELD") != ZR_NULL ||
        strstr(msg, "STRUCT_METHOD") != ZR_NULL ||
        strstr(msg, "CLASS_FIELD") != ZR_NULL ||
        strstr(msg, "CLASS_METHOD") != ZR_NULL ||
        strstr(msg, "FUNCTION_DECLARATION") != ZR_NULL ||
        strstr(msg, "STRUCT_DECLARATION") != ZR_NULL ||
        strstr(msg, "CLASS_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_DECLARATION") != ZR_NULL ||
        strstr(msg, "ENUM_DECLARATION") != ZR_NULL ||
        strstr(msg, "cannot be used as") != ZR_NULL) {
        printf("  Suggestion: Declaration types (interface, struct, class, enum, function) cannot be used as statements or expressions.\n");
        printf("              They should only appear in their proper declaration contexts (top-level, class body, etc.).\n");
        printf("              Check if you accidentally placed a declaration inside a block or expression context.\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        printf("  Suggestion: This AST node type is not supported in expression context.\n");
        printf("              Possible causes:\n");
        printf("              1. The node was incorrectly parsed or placed in the wrong context\n");
        printf("              2. A declaration type was mistakenly used as an expression\n");
        printf("              3. Missing implementation for this node type in compile_expression\n");
        printf("              Check the AST structure and ensure the node is in the correct context.\n");
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        printf("  Suggestion: Type mismatch detected. Check:\n");
        printf("              1. Variable types match their assignments\n");
        printf("              2. Function argument types match the function signature\n");
        printf("              3. Return types match the function declaration\n");
        printf("              4. Type annotations are correct\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        printf("  Suggestion: The referenced identifier was not found. Check:\n");
        printf("              1. Variable/function is declared before use\n");
        printf("              2. Variable/function is in scope\n");
        printf("              3. Spelling is correct\n");
        printf("              4. Import statements are correct (if using modules)\n");
    } else if (strstr(msg, "Destructuring") != ZR_NULL) {
        printf("  Suggestion: Destructuring patterns can only be used in variable declarations.\n");
        printf("              They cannot be used as standalone expressions or statements.\n");
    } else if (strstr(msg, "Loop or statement") != ZR_NULL) {
        printf("  Suggestion: Control flow statements (if, while, for, etc.) cannot be used as expressions.\n");
        printf("              Use them as statements, or use expression forms (if expression, etc.) if available.\n");
    } else if (strstr(msg, "Failed to") != ZR_NULL) {
        printf("  Suggestion: Internal compiler error. This may indicate:\n");
        printf("              1. Memory allocation failure\n");
        printf("              2. Invalid compiler state\n");
        printf("              3. Bug in the compiler\n");
        printf("              Please report this issue with the source code that triggered it.\n");
    }
}

void ZrCompilerError(SZrCompilerState *cs, const TChar *msg, SZrFileRange location) {
    if (cs == ZR_NULL) {
        return;
    }

    cs->hasError = ZR_TRUE;
    cs->errorMessage = msg;
    cs->errorLocation = location;

    // 输出详细的错误信息（包含行列号）
    const TChar *sourceName = "unknown";
    TZrSize nameLen = 7; // "unknown" 的长度
    if (location.source != ZR_NULL) {
        if (location.source->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            sourceName = ZrStringGetNativeStringShort(location.source);
            nameLen = location.source->shortStringLength;
        } else {
            sourceName = ZrStringGetNativeString(location.source);
            nameLen = location.source->longStringLength;
        }
    }

    // 输出格式化的错误信息
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Compiler Error\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Error Message: %s\n", msg);
    printf("  Location: %.*s:%d:%d - %d:%d\n", 
           (int) nameLen, sourceName, 
           location.start.line, location.start.column,
           location.end.line, location.end.column);
    
    // 输出错误原因分析
    printf("\n  Error Analysis:\n");
    if (strstr(msg, "INTERFACE_METHOD_SIGNATURE") != ZR_NULL ||
        strstr(msg, "INTERFACE_FIELD_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_PROPERTY_SIGNATURE") != ZR_NULL) {
        printf("    - Problem: Interface declaration member found in invalid context\n");
        printf("    - Root Cause: Interface members (methods, fields, properties) can only appear\n");
        printf("                  inside interface declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "STRUCT_FIELD") != ZR_NULL ||
               strstr(msg, "STRUCT_METHOD") != ZR_NULL ||
               strstr(msg, "CLASS_FIELD") != ZR_NULL ||
               strstr(msg, "CLASS_METHOD") != ZR_NULL) {
        printf("    - Problem: Struct/Class member found in invalid context\n");
        printf("    - Root Cause: Struct/Class members can only appear inside struct/class\n");
        printf("                  declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        printf("    - Problem: AST node type not supported in expression context\n");
        printf("    - Root Cause: The compiler encountered a node type that cannot be compiled\n");
        printf("                  as an expression. This may indicate a parsing error or missing\n");
        printf("                  implementation for this node type.\n");
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        printf("    - Problem: Type compatibility check failed\n");
        printf("    - Root Cause: The types of operands, variables, or function arguments are\n");
        printf("                  not compatible with the operation or assignment being performed\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        printf("    - Problem: Identifier not found in current scope\n");
        printf("    - Root Cause: The variable, function, or type name was not found in the\n");
        printf("                  current scope or type environment\n");
    } else {
        printf("    - Problem: Compilation error occurred\n");
        printf("    - Root Cause: See error message above for details\n");
    }
    
    // 输出解决建议
    printf("\n  How to Fix:\n");
    print_error_suggestion(msg);
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
}

// 创建指令（辅助函数）
TZrInstruction create_instruction_0(EZrInstructionCode opcode, TUInt16 operandExtra) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = 0;
    return instruction;
}

TZrInstruction create_instruction_1(EZrInstructionCode opcode, TUInt16 operandExtra, TInt32 operand) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

TZrInstruction create_instruction_2(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt16 operand1,
                                    TUInt16 operand2) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

static TZrInstruction create_instruction_4(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt8 op0, TUInt8 op1,
                                           TUInt8 op2, TUInt8 op3) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand0[0] = op0;
    instruction.instruction.operand.operand0[1] = op1;
    instruction.instruction.operand.operand0[2] = op2;
    instruction.instruction.operand.operand0[3] = op3;
    return instruction;
}

// 添加指令到当前函数
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    ZrArrayPush(cs->state, &cs->instructions, &instruction);
    // instructionCount 应该与 instructions.length 保持同步
    cs->instructionCount = cs->instructions.length;
}

// 添加常量到常量池
TUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value) {
    if (cs == ZR_NULL || cs->hasError || value == ZR_NULL) {
        return 0;
    }

    // 检查常量是否已存在（常量去重）
    // 遍历已有的常量，查找相同的值
    for (TZrSize i = 0; i < cs->constants.length; i++) {
        SZrTypeValue *existingValue = (SZrTypeValue *)ZrArrayGet(&cs->constants, i);
        if (existingValue != ZR_NULL) {
            // 使用 ZrValueEqual 比较常量值
            if (ZrValueEqual(cs->state, existingValue, value)) {
                // 找到相同的常量，返回已有常量的索引
                return (TUInt32)i;
            }
        }
    }

    // 常量不存在，添加新常量
    ZrArrayPush(cs->state, &cs->constants, value);
    TUInt32 index = (TUInt32) cs->constantCount;
    cs->constantCount++;
    return index;
}

// 分配局部变量槽位
TUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || cs->hasError || name == ZR_NULL) {
        return 0;
    }

    SZrFunctionLocalVariable localVar;
    localVar.name = name;
    localVar.offsetActivate = (TZrMemoryOffset) cs->instructionCount;
    localVar.offsetDead = 0; // 将在变量作用域结束时设置

    ZrArrayPush(cs->state, &cs->localVars, &localVar);
    // localVarCount 应该与 localVars.length 保持同步
    cs->localVarCount = cs->localVars.length;
    TUInt32 index = (TUInt32) (cs->localVarCount - 1);
    cs->stackSlotCount++;

    return index;
}

// 查找局部变量
TUInt32 find_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TUInt32) -1;
    }

    // 从当前作用域开始查找
    // 使用 localVars.length 而不是 localVarCount，确保同步
    TZrSize varCount = cs->localVars.length;
    for (TZrSize i = varCount; i > 0; i--) {
        TZrSize index = i - 1;
        // 确保索引在有效范围内
        if (index < cs->localVars.length) {
            SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *) ZrArrayGet(&cs->localVars, index);
            if (var != ZR_NULL && var->name != ZR_NULL) {
                // 比较字符串
                if (ZrStringEqual(var->name, name)) {
                    return (TUInt32) index;
                }
            }
        }
    }

    return (TUInt32) -1;
}

// 查找闭包变量
TUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TUInt32) -1;
    }

    // 在闭包变量数组中查找
    TZrSize closureVarCount = cs->closureVars.length;
    for (TZrSize i = 0; i < closureVarCount; i++) {
        SZrFunctionClosureVariable *var = (SZrFunctionClosureVariable *) ZrArrayGet(&cs->closureVars, i);
        if (var != ZR_NULL && var->name != ZR_NULL) {
            if (ZrStringEqual(var->name, name)) {
                return (TUInt32) i;
            }
        }
    }

    return (TUInt32) -1;
}

// 分配闭包变量
TUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TBool inStack) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return 0;
    }

    // 检查是否已存在
    TUInt32 existingIndex = find_closure_var(cs, name);
    if (existingIndex != (TUInt32) -1) {
        return existingIndex;
    }

    // 创建新的闭包变量
    SZrFunctionClosureVariable closureVar;
    closureVar.name = name;
    closureVar.inStack = inStack;
    closureVar.index = (TUInt32) cs->closureVarCount;
    closureVar.valueType = ZR_VALUE_TYPE_NULL; // 类型将在运行时确定

    ZrArrayPush(cs->state, &cs->closureVars, &closureVar);
    cs->closureVarCount++;
    
    return (TUInt32) (cs->closureVarCount - 1);
}

// 查找子函数索引（在当前编译器的 childFunctions 中通过函数名查找）
// 返回子函数在 childFunctions 数组中的索引，如果未找到返回 (TUInt32)-1
// 注意：这个函数用于在编译时查找子函数索引
// 通过编译时建立的函数名到索引的映射来查找，不依赖遍历比较函数名
TUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TUInt32) -1;
    }
    
    // 遍历函数名映射数组，查找匹配的函数名
    // 这个映射在编译函数声明时建立，仅用于编译时查找
    // 运行时查找完全基于索引，不依赖函数名
    for (TZrSize i = 0; i < cs->childFunctionNameMap.length; i++) {
        SZrChildFunctionNameMap *map = (SZrChildFunctionNameMap *)ZrArrayGet(&cs->childFunctionNameMap, i);
        if (map != ZR_NULL && map->name != ZR_NULL) {
            if (ZrStringEqual(map->name, name)) {
                // 找到匹配的函数名，返回对应的子函数索引
                return map->childFunctionIndex;
            }
        }
    }
    
    return (TUInt32) -1;
}

// 分配栈槽
TUInt32 allocate_stack_slot(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return 0;
    }

    TUInt32 slot = (TUInt32) cs->stackSlotCount;
    cs->stackSlotCount++;
    return slot;
}

// 进入新作用域
void enter_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    SZrScope scope;
    scope.startVarIndex = cs->localVarCount;
    scope.varCount = 0;
    // 如果当前编译器有父编译器，则新作用域的父编译器就是当前编译器
    // 否则，如果当前编译器是顶层编译器，其父编译器为NULL
    scope.parentCompiler = cs->currentFunction != ZR_NULL ? cs : ZR_NULL;

    ZrArrayPush(cs->state, &cs->scopeStack, &scope);
}

// 退出作用域
void exit_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    if (cs->scopeStack.length == 0) {
        return;
    }

    SZrScope *scope = (SZrScope *) ZrArrayPop(&cs->scopeStack);
    if (scope != ZR_NULL) {
        // 标记作用域内变量的结束位置
        TZrMemoryOffset endOffset = (TZrMemoryOffset) cs->instructionCount;
        // 使用 localVars.length 而不是 localVarCount，确保同步
        TZrSize varCount = cs->localVars.length;
        for (TZrSize i = scope->startVarIndex; i < scope->startVarIndex + scope->varCount; i++) {
            if (i < varCount) {
                SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *) ZrArrayGet(&cs->localVars, i);
                if (var != ZR_NULL) {
                    var->offsetDead = endOffset;
                }
            }
        }
    }
}

// 进入类型作用域（推入新环境）
void enter_type_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 创建新的类型环境
    SZrTypeEnvironment *newEnv = ZrTypeEnvironmentNew(cs->state);
    if (newEnv == ZR_NULL) {
        return;
    }
    
    // 设置父环境为当前环境
    newEnv->parent = cs->typeEnv;
    
    // 将当前环境推入栈
    if (cs->typeEnv != ZR_NULL) {
        ZrArrayPush(cs->state, &cs->typeEnvStack, &cs->typeEnv);
    }
    
    // 设置新环境为当前环境
    cs->typeEnv = newEnv;
}

// 退出类型作用域（弹出环境）
void exit_type_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (cs->typeEnvStack.length == 0) {
        // 如果栈为空，说明这是最外层环境，只需要释放当前环境
        if (cs->typeEnv != ZR_NULL) {
            ZrTypeEnvironmentFree(cs->state, cs->typeEnv);
            cs->typeEnv = ZR_NULL;
        }
        return;
    }
    
    // 释放当前环境
    if (cs->typeEnv != ZR_NULL) {
        ZrTypeEnvironmentFree(cs->state, cs->typeEnv);
    }
    
    // 从栈中弹出父环境
    SZrTypeEnvironment **parentEnvPtr = (SZrTypeEnvironment **)ZrArrayPop(&cs->typeEnvStack);
    if (parentEnvPtr != ZR_NULL) {
        cs->typeEnv = *parentEnvPtr;
    } else {
        cs->typeEnv = ZR_NULL;
    }
}

// 创建标签
TZrSize create_label(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return (TZrSize) -1;
    }

    SZrLabel label;
    label.instructionIndex = cs->instructionCount;
    label.isResolved = ZR_FALSE;

    ZrArrayPush(cs->state, &cs->labels, &label);
    return cs->labels.length - 1;
}

// 解析标签
void resolve_label(SZrCompilerState *cs, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError || labelId >= cs->labels.length) {
        return;
    }

    SZrLabel *label = (SZrLabel *) ZrArrayGet(&cs->labels, labelId);
    if (label != ZR_NULL) {
        label->instructionIndex = cs->instructionCount;
        label->isResolved = ZR_TRUE;

        // 填充所有指向该标签的跳转指令的偏移量
        for (TZrSize i = 0; i < cs->pendingJumps.length; i++) {
            SZrPendingJump *pendingJump = (SZrPendingJump *) ZrArrayGet(&cs->pendingJumps, i);
            if (pendingJump != ZR_NULL && pendingJump->labelId == labelId &&
                pendingJump->instructionIndex < cs->instructions.length) {
                TZrInstruction *jumpInst =
                        (TZrInstruction *) ZrArrayGet(&cs->instructions, pendingJump->instructionIndex);
                if (jumpInst != ZR_NULL) {
                    // 计算相对偏移：目标指令索引 - 当前指令索引
                    TInt32 offset = (TInt32) label->instructionIndex - (TInt32) pendingJump->instructionIndex;
                    jumpInst->instruction.operand.operand2[0] = offset;
                }
            }
        }
    }
}

// 添加待解析的跳转（在 compiler.c 中定义，在 compile_statement.c 和 compile_expression.c 中使用）
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    SZrPendingJump pendingJump;
    pendingJump.instructionIndex = instructionIndex;
    pendingJump.labelId = labelId;

    ZrArrayPush(cs->state, &cs->pendingJumps, &pendingJump);
}

// 外部变量分析辅助函数（用于闭包捕获）

// 记录引用的外部变量（简化实现：直接添加到列表）
static void record_external_var_reference(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < cs->referencedExternalVars.length; i++) {
        SZrString **varName = (SZrString **)ZrArrayGet(&cs->referencedExternalVars, i);
        if (varName != ZR_NULL && *varName == name) {
            return; // 已存在
        }
    }
    
    // 添加到列表
    ZrArrayPush(cs->state, &cs->referencedExternalVars, &name);
}

// 递归遍历AST节点，查找所有标识符引用
static void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames) {
    if (cs == ZR_NULL || node == ZR_NULL || identifierNames == ZR_NULL) {
        return;
    }
    
    // 如果是标识符节点，添加到集合中
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = node->data.identifier.name;
        if (name != ZR_NULL) {
            // 检查是否已存在（避免重复）
            TBool exists = ZR_FALSE;
            for (TZrSize i = 0; i < identifierNames->length; i++) {
                SZrString **existingName = (SZrString **)ZrArrayGet(identifierNames, i);
                if (existingName != ZR_NULL && *existingName == name) {
                    exists = ZR_TRUE;
                    break;
                }
            }
            if (!exists) {
                ZrArrayPush(cs->state, identifierNames, &name);
            }
        }
        return;
    }
    
    // 递归遍历所有子节点
    // 根据节点类型访问不同的子节点字段
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL) {
                collect_identifiers_from_node(cs, binExpr->left, identifierNames);
            }
            if (binExpr->right != ZR_NULL) {
                collect_identifiers_from_node(cs, binExpr->right, identifierNames);
            }
            break;
        }
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            if (unaryExpr->argument != ZR_NULL) {
                collect_identifiers_from_node(cs, unaryExpr->argument, identifierNames);
            }
            break;
        }
        case ZR_AST_LOGICAL_EXPRESSION: {
            SZrLogicalExpression *logicalExpr = &node->data.logicalExpression;
            if (logicalExpr->left != ZR_NULL) {
                collect_identifiers_from_node(cs, logicalExpr->left, identifierNames);
            }
            if (logicalExpr->right != ZR_NULL) {
                collect_identifiers_from_node(cs, logicalExpr->right, identifierNames);
            }
            break;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL) {
                collect_identifiers_from_node(cs, assignExpr->left, identifierNames);
            }
            if (assignExpr->right != ZR_NULL) {
                collect_identifiers_from_node(cs, assignExpr->right, identifierNames);
            }
            break;
        }
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
            if (condExpr->test != ZR_NULL) {
                collect_identifiers_from_node(cs, condExpr->test, identifierNames);
            }
            if (condExpr->consequent != ZR_NULL) {
                collect_identifiers_from_node(cs, condExpr->consequent, identifierNames);
            }
            if (condExpr->alternate != ZR_NULL) {
                collect_identifiers_from_node(cs, condExpr->alternate, identifierNames);
            }
            break;
        }
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            // 注意：SZrFunctionCall 结构可能没有 callee 字段，需要检查实际结构
            // 函数调用在 primary expression 中处理，这里暂时跳过
            if (funcCall->args != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    SZrAstNode *arg = funcCall->args->nodes[i];
                    if (arg != ZR_NULL) {
                        collect_identifiers_from_node(cs, arg, identifierNames);
                    }
                }
            }
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *memberExpr = &node->data.memberExpression;
            if (memberExpr->property != ZR_NULL) {
                collect_identifiers_from_node(cs, memberExpr->property, identifierNames);
            }
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->property != ZR_NULL) {
                collect_identifiers_from_node(cs, primary->property, identifierNames);
            }
            if (primary->members != ZR_NULL) {
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    SZrAstNode *member = primary->members->nodes[i];
                    if (member != ZR_NULL) {
                        collect_identifiers_from_node(cs, member, identifierNames);
                    }
                }
            }
            break;
        }
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arrayLit = &node->data.arrayLiteral;
            if (arrayLit->elements != ZR_NULL) {
                for (TZrSize i = 0; i < arrayLit->elements->count; i++) {
                    SZrAstNode *elem = arrayLit->elements->nodes[i];
                    if (elem != ZR_NULL) {
                        collect_identifiers_from_node(cs, elem, identifierNames);
                    }
                }
            }
            break;
        }
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *objLit = &node->data.objectLiteral;
            if (objLit->properties != ZR_NULL) {
                for (TZrSize i = 0; i < objLit->properties->count; i++) {
                    SZrAstNode *prop = objLit->properties->nodes[i];
                    if (prop != ZR_NULL) {
                        collect_identifiers_from_node(cs, prop, identifierNames);
                    }
                }
            }
            break;
        }
        case ZR_AST_KEY_VALUE_PAIR: {
            SZrKeyValuePair *kv = &node->data.keyValuePair;
            if (kv->key != ZR_NULL) {
                collect_identifiers_from_node(cs, kv->key, identifierNames);
            }
            if (kv->value != ZR_NULL) {
                collect_identifiers_from_node(cs, kv->value, identifierNames);
            }
            break;
        }
        case ZR_AST_LAMBDA_EXPRESSION: {
            SZrLambdaExpression *lambda = &node->data.lambdaExpression;
            if (lambda->block != ZR_NULL) {
                collect_identifiers_from_node(cs, lambda->block, identifierNames);
            }
            break;
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            if (ifExpr->condition != ZR_NULL) {
                collect_identifiers_from_node(cs, ifExpr->condition, identifierNames);
            }
            if (ifExpr->thenExpr != ZR_NULL) {
                collect_identifiers_from_node(cs, ifExpr->thenExpr, identifierNames);
            }
            if (ifExpr->elseExpr != ZR_NULL) {
                collect_identifiers_from_node(cs, ifExpr->elseExpr, identifierNames);
            }
            break;
        }
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    SZrAstNode *stmt = block->body->nodes[i];
                    if (stmt != ZR_NULL) {
                        collect_identifiers_from_node(cs, stmt, identifierNames);
                    }
                }
            }
            break;
        }
        default:
            // 其他节点类型暂时不处理，可以根据需要扩展
            break;
    }
}

// 分析AST节点中的外部变量引用（完整实现）
void analyze_external_variables(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler) {
    if (cs == ZR_NULL || node == ZR_NULL || parentCompiler == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 1. 收集所有标识符引用
    SZrArray identifierNames;
    ZrArrayInit(cs->state, &identifierNames, sizeof(SZrString *), 16);
    collect_identifiers_from_node(cs, node, &identifierNames);
    
    // 2. 检查每个标识符是否是外部变量
    for (TZrSize i = 0; i < identifierNames.length; i++) {
        SZrString **namePtr = (SZrString **)ZrArrayGet(&identifierNames, i);
        if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
            continue;
        }
        SZrString *name = *namePtr;
        
        // 在当前编译器中查找（局部变量和闭包变量）
        TUInt32 localIndex = find_local_var(cs, name);
        TUInt32 closureIndex = find_closure_var(cs, name);
        
        // 如果既不是局部变量也不是闭包变量，可能是外部变量
        if (localIndex == (TUInt32)-1 && closureIndex == (TUInt32)-1) {
            // 在父编译器中查找（外部作用域的变量）
            TUInt32 parentLocalIndex = find_local_var(parentCompiler, name);
            if (parentLocalIndex != (TUInt32)-1) {
                // 这是外部变量，需要捕获到闭包中
                // 检查是否已经分配过
                if (find_closure_var(cs, name) == (TUInt32)-1) {
                    // 在闭包变量列表中分配
                    allocate_closure_var(cs, name, ZR_TRUE); // inStack = true，表示在栈上
                }
            }
        }
    }
    
    // 3. 清理临时数组
    ZrArrayFree(cs->state, &identifierNames);
}

// 指令优化函数（占位实现，后续用于压缩和优化指令）

// 压缩指令（消除冗余指令、合并指令等）
static void compress_instructions(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 指令压缩已在 optimize_instructions 中实现
    // - 消除冗余的GET_STACK/SET_STACK指令
    // - 合并连续的常量加载
    // - 优化跳转指令
    // - 其他优化
}

// 消除冗余指令
static void eliminate_redundant_instructions(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError || cs->instructions.length == 0) {
        return;
    }
    
    // 消除无用的栈操作
    // 例如：连续的 SET_STACK 和 GET_STACK 到同一个槽位可以优化
    // 注意：这是一个简化实现，更复杂的死代码消除需要数据流分析
    
    // 遍历指令序列，查找冗余模式
    TZrSize i = 0;
    while (i < cs->instructions.length - 1) {
        TZrInstruction *inst1 = (TZrInstruction *)ZrArrayGet(&cs->instructions, i);
        TZrInstruction *inst2 = (TZrInstruction *)ZrArrayGet(&cs->instructions, i + 1);
        
        if (inst1 != ZR_NULL && inst2 != ZR_NULL) {
            EZrInstructionCode opcode1 = (EZrInstructionCode)inst1->instruction.operationCode;
            EZrInstructionCode opcode2 = (EZrInstructionCode)inst2->instruction.operationCode;
            
            // 检查 SET_STACK 后立即 GET_STACK 到同一个槽位（可以消除）
            if (opcode1 == ZR_INSTRUCTION_ENUM(SET_STACK) && opcode2 == ZR_INSTRUCTION_ENUM(GET_STACK)) {
                TUInt16 destSlot1 = inst1->instruction.operandExtra;
                TUInt16 destSlot2 = inst2->instruction.operandExtra;
                TInt32 srcSlot1 = inst1->instruction.operand.operand1[0];
                TInt32 srcSlot2 = inst2->instruction.operand.operand1[0];
                
                // 如果 SET_STACK 的目标槽位和 GET_STACK 的源槽位相同，且目标槽位也相同
                if (destSlot1 == (TUInt16)srcSlot2 && destSlot1 == destSlot2) {
                    // 这是冗余操作，可以消除 GET_STACK
                    // 移除 inst2
                    for (TZrSize j = i + 1; j < cs->instructions.length - 1; j++) {
                        TZrInstruction *src = (TZrInstruction *)ZrArrayGet(&cs->instructions, j + 1);
                        TZrInstruction *dst = (TZrInstruction *)ZrArrayGet(&cs->instructions, j);
                        if (src != ZR_NULL && dst != ZR_NULL) {
                            *dst = *src;
                        }
                    }
                    cs->instructions.length--;
                    cs->instructionCount--;
                    continue; // 不增加 i，继续检查当前位置
                }
            }
        }
        
        i++;
    }
    
    // 消除无用的标签（标签没有被引用）
    // 注意：这需要分析跳转指令，暂时跳过
}

// 优化跳转指令
static void optimize_jumps(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError || cs->instructions.length == 0) {
        return;
    }
    
    // 消除连续跳转
    // 例如：JUMP -> label1, label1: JUMP -> label2 可以优化为 JUMP -> label2
    // 注意：这需要先解析所有标签，然后进行跳转链分析
    
    // 遍历指令序列，查找连续跳转
    TZrSize i = 0;
    while (i < cs->instructions.length - 1) {
        TZrInstruction *inst1 = (TZrInstruction *)ZrArrayGet(&cs->instructions, i);
        
        if (inst1 != ZR_NULL) {
            EZrInstructionCode opcode1 = (EZrInstructionCode)inst1->instruction.operationCode;
            
            // 检查是否是跳转指令
            if (opcode1 == ZR_INSTRUCTION_ENUM(JUMP) || opcode1 == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
                // 查找跳转目标
                // 注意：这需要标签解析后才能进行，暂时跳过详细实现
                // 这里只做基本检查
            }
        }
        
        i++;
    }
    
    // 合并相同目标的跳转
    // 如果多个跳转指令跳转到同一个标签，可以考虑优化
    // 注意：这需要更复杂的分析，暂时跳过
}

// 主编译优化入口
static void optimize_instructions(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 执行各种优化
    compress_instructions(cs);
    eliminate_redundant_instructions(cs);
    optimize_jumps(cs);
    
    // TODO: 添加更多优化步骤
}

// 编译表达式（在 compile_expression.c 中实现）
// 这里只声明，不实现

// 编译语句（在 compile_statement.c 中实现）
// 这里只声明，不实现

// 编译函数声明
void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_FUNCTION_DECLARATION) {
        ZrCompilerError(cs, "Expected function declaration", node->location);
        return;
    }

    SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;

    // 注册函数类型到类型环境（在编译函数体之前注册，以便递归调用）
    if (cs->typeEnv != ZR_NULL && funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
        SZrInferredType returnType;
        if (funcDecl->returnType != ZR_NULL) {
            // 从返回类型注解推断类型
            if (convert_ast_type_to_inferred_type(cs, funcDecl->returnType, &returnType)) {
                // 收集参数类型
                SZrArray paramTypes;
                ZrArrayInit(cs->state, &paramTypes, sizeof(SZrInferredType), 8);
                if (funcDecl->params != ZR_NULL) {
                    for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                        SZrAstNode *paramNode = funcDecl->params->nodes[i];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                            SZrParameter *param = &paramNode->data.parameter;
                            if (param->typeInfo != ZR_NULL) {
                                SZrInferredType paramType;
                                if (convert_ast_type_to_inferred_type(cs, param->typeInfo, &paramType)) {
                                    ZrArrayPush(cs->state, &paramTypes, &paramType);
                                    ZrInferredTypeFree(cs->state, &paramType);
                                }
                            } else {
                                // 没有类型注解，使用对象类型
                                SZrInferredType paramType;
                                ZrInferredTypeInit(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                                ZrArrayPush(cs->state, &paramTypes, &paramType);
                                ZrInferredTypeFree(cs->state, &paramType);
                            }
                        }
                    }
                }
                // 注册函数类型
                ZrTypeEnvironmentRegisterFunction(cs->state, cs->typeEnv, funcDecl->name->name, &returnType, &paramTypes);
                ZrInferredTypeFree(cs->state, &returnType);
                ZrArrayFree(cs->state, &paramTypes);
            }
        } else {
            // 没有返回类型注解，使用对象类型
            SZrInferredType returnType;
            ZrInferredTypeInit(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
            SZrArray paramTypes;
            ZrArrayInit(cs->state, &paramTypes, sizeof(SZrInferredType), 8);
            ZrTypeEnvironmentRegisterFunction(cs->state, cs->typeEnv, funcDecl->name->name, &returnType, &paramTypes);
            ZrInferredTypeFree(cs->state, &returnType);
            ZrArrayFree(cs->state, &paramTypes);
        }
    }

    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;

    // 创建新的函数对象
    cs->currentFunction = ZrFunctionNew(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create function object", node->location);
        return;
    }

    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;

    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;

    // 进入函数作用域（嵌套函数需要设置父编译器引用）
    // 保存父编译器引用（如果有）
    SZrCompilerState *parentCompiler = (oldFunction != ZR_NULL) ? cs : ZR_NULL;
    enter_scope(cs);
    // 设置父编译器引用（在 enter_scope 之后，因为需要访问 scopeStack）
    if (parentCompiler != ZR_NULL && cs->scopeStack.length > 0) {
        SZrScope *currentScope = (SZrScope *)ZrArrayGet(&cs->scopeStack, cs->scopeStack.length - 1);
        if (currentScope != ZR_NULL) {
            currentScope->parentCompiler = parentCompiler;
        }
    }

    // 1. 编译参数列表
    TUInt32 parameterCount = 0;
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < funcDecl->params->count; i++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        TUInt32 paramIndex = allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        // 注册参数类型到类型环境（用于类型推断）
                        if (cs->typeEnv != ZR_NULL) {
                            SZrInferredType paramType;
                            if (param->typeInfo != ZR_NULL) {
                                // 从类型注解推断类型
                                if (convert_ast_type_to_inferred_type(cs, param->typeInfo, &paramType)) {
                                    ZrTypeEnvironmentRegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                    ZrInferredTypeFree(cs->state, &paramType);
                                }
                            } else {
                                // 没有类型注解，注册为对象类型（默认）
                                ZrInferredTypeInit(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                                ZrTypeEnvironmentRegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                ZrInferredTypeFree(cs->state, &paramType);
                            }
                        }

                        // 如果有默认值，编译默认值表达式
                        if (param->defaultValue != ZR_NULL) {
                            compile_expression(cs, param->defaultValue);
                            TUInt32 defaultSlot = cs->stackSlotCount - 1;

                            // 生成 SET_STACK 指令
                            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                       (TUInt16) paramIndex, (TInt32) defaultSlot);
                            emit_instruction(cs, inst);
                        }
                    }
                }
            }
        }
    }

    // 检查是否有可变参数
    TBool hasVariableArguments = (funcDecl->args != ZR_NULL);

    // 2. 编译函数体
    if (funcDecl->body != ZR_NULL) {
        compile_statement(cs, funcDecl->body);
    }

    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrValueResetAsNull(&nullValue);
            TUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) resultSlot,
                                                       (TInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
            // 确保 length > 0 才访问数组
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrArrayGet(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrValueResetAsNull(&nullValue);
                        TUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TUInt16) resultSlot, (TInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrValueResetAsNull(&nullValue);
                TUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) resultSlot,
                                                           (TInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    // 退出函数作用域
    exit_scope(cs);

    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;

    // 复制指令列表
    // 使用 instructions.length 而不是 instructionCount，确保同步
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TUInt32) cs->instructions.length;
            // 同步 instructionCount
            cs->instructionCount = cs->instructions.length;
        }
    } else {
        // 如果没有指令，设置为0（函数体可能为空或只有隐式返回）
        newFunc->instructionsLength = 0;
        newFunc->instructionsList = ZR_NULL;
    }

    // 复制常量列表
    // 使用 constants.length 而不是 constantCount，确保同步
    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *) ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TUInt32) cs->constants.length;
            // 同步 constantCount
            cs->constantCount = cs->constants.length;
        }
    }

    // 复制局部变量列表
    // 使用 localVars.length 而不是 localVarCount，确保同步
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrMemoryRawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TUInt32) cs->localVars.length;
            // 同步 localVarCount
            cs->localVarCount = cs->localVars.length;
        }
    }

    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *) ZrMemoryRawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TUInt32) cs->closureVarCount;
        }
    }

    // 设置函数元数据
    newFunc->stackSize = (TUInt32) cs->stackSlotCount;
    newFunc->parameterCount = (TUInt16) parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TUInt32) node->location.end.line : 0;
    
    // 设置函数名（函数名由函数自身持有）
    // 如果有名称，存储函数名；匿名函数（lambda）为 ZR_NULL
    if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
        newFunc->functionName = funcDecl->name->name;
    } else {
        newFunc->functionName = ZR_NULL;  // 匿名函数
    }

    // 将新函数添加到子函数列表
    // 注意：这里需要在父编译器的上下文中操作
    // 函数名已经存储在函数对象中（newFunc->functionName），父函数只需要维护子函数列表的索引
    // 检查是否是顶层函数声明（脚本级别的函数声明，而不是嵌套函数）
    // 如果当前编译器是脚本级别（isScriptLevel == ZR_TRUE），则这是顶层函数声明
    if (oldFunction != ZR_NULL && !cs->isScriptLevel) {
        // 将新函数添加到子函数列表（嵌套函数）
        // 子函数在 childFunctions 中的索引就是添加时的位置（cs->childFunctions.length）
        ZrArrayPush(cs->state, &cs->childFunctions, &newFunc);
    } else {
        // 如果是顶层函数声明（脚本级别的函数声明），将其保存到编译器状态
        // 这样 ZrCompilerCompile 可以返回它
        cs->topLevelFunction = newFunc;
    }

    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
}

// 编译测试声明
// 语法：%test("test_name") { ... }
// 要求：只有测试模式进入，如果throw抛出则测试失败，正常到函数末尾则测试成功
static void compile_test_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TEST_DECLARATION) {
        ZrCompilerError(cs, "Expected test declaration", node->location);
        return;
    }

    SZrTestDeclaration *testDecl = &node->data.testDeclaration;

    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;

    // 创建新的测试函数对象
    cs->currentFunction = ZrFunctionNew(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrCompilerError(cs, "Failed to create test function object", node->location);
        return;
    }

    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;

    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;

    // 进入函数作用域
    enter_scope(cs);

    // 测试函数没有参数
    // 计算参数数量和可变参数标志
    TUInt32 parameterCount = 0;
    if (testDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < testDecl->params->count; i++) {
            SZrAstNode *paramNode = testDecl->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    parameterCount++;
                }
            }
        }
    }
    TBool hasVariableArguments = (testDecl->args != ZR_NULL);

    // 生成测试模式检查（只有测试模式才执行）
    // 测试模式检查在运行时进行，这里先编译测试体
    // TODO: 实现测试模式标志检查（可以通过全局变量或函数参数实现）

    // 创建成功标签（正常执行到末尾）
    TZrSize successLabelId = create_label(cs);

    // 创建失败标签（捕获到异常）
    TZrSize failLabelId = create_label(cs);

    // 创建返回标签（统一返回点）
    TZrSize returnLabelId = create_label(cs);

    // 用 TRY 包裹测试体，捕获异常
    // 生成 TRY 指令
    TZrInstruction tryInst = create_instruction_0(ZR_INSTRUCTION_ENUM(TRY), 0);
    emit_instruction(cs, tryInst);

    // 编译测试体
    if (testDecl->body != ZR_NULL) {
        compile_statement(cs, testDecl->body);
    }

    // 如果正常执行到这里（没有抛出异常），测试成功
    // 跳转到成功处理
    TZrInstruction jumpSuccessInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpSuccessIndex = cs->instructionCount;
    emit_instruction(cs, jumpSuccessInst);
    add_pending_jump(cs, jumpSuccessIndex, successLabelId);

    // 生成 CATCH 指令（捕获异常）
    TZrInstruction catchInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CATCH), 0);
    emit_instruction(cs, catchInst);

    // 如果捕获到异常，跳转到失败处理
    TZrInstruction jumpFailInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpFailIndex = cs->instructionCount;
    emit_instruction(cs, jumpFailInst);
    add_pending_jump(cs, jumpFailIndex, failLabelId);

    // 解析成功标签
    resolve_label(cs, successLabelId);

    // 生成成功常量（1 表示测试通过）
    TUInt32 successSlot = allocate_stack_slot(cs);
    SZrTypeValue successValue;
    ZrValueInitAsInt(cs->state, &successValue, 1); // 1 表示成功
    TUInt32 successConstantIndex = add_constant(cs, &successValue);
    TZrInstruction successInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) successSlot,
                                                      (TInt32) successConstantIndex);
    emit_instruction(cs, successInst);

    // 跳转到返回处理
    TZrInstruction jumpReturnInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpReturnIndex = cs->instructionCount;
    emit_instruction(cs, jumpReturnInst);
    add_pending_jump(cs, jumpReturnIndex, returnLabelId);

    // 解析失败标签
    resolve_label(cs, failLabelId);

    // 生成失败常量（0 表示测试失败）
    TUInt32 failSlot = allocate_stack_slot(cs);
    SZrTypeValue failValue;
    ZrValueInitAsInt(cs->state, &failValue, 0); // 0 表示失败
    TUInt32 failConstantIndex = add_constant(cs, &failValue);
    TZrInstruction failInst =
            create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) failSlot, (TInt32) failConstantIndex);
    emit_instruction(cs, failInst);

    // 解析返回标签
    resolve_label(cs, returnLabelId);

    // 如果没有显式返回，添加隐式返回（返回测试结果）
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回成功
            TUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue successValue2;
            ZrValueInitAsInt(cs->state, &successValue2, 1);
            TUInt32 constantIndex = add_constant(cs, &successValue2);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) resultSlot,
                                                       (TInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrArrayGet(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回（使用成功槽位）
                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) successSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回成功
                TUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue successValue2;
                ZrValueInitAsInt(cs->state, &successValue2, 1);
                TUInt32 constantIndex = add_constant(cs, &successValue2);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) resultSlot,
                                                           (TInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    // 退出函数作用域
    exit_scope(cs);

    // 将编译结果复制到函数对象（参考 compile_function_declaration）
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;

    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TUInt32) cs->instructions.length;
            cs->instructionCount = cs->instructions.length;
        }
    }

    // 复制常量列表
    // 使用 constants.length 而不是 constantCount，确保同步
    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *) ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TUInt32) cs->constants.length;
            // 同步 constantCount
            cs->constantCount = cs->constants.length;
        }
    }

    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrMemoryRawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TUInt32) cs->localVars.length;
            cs->localVarCount = cs->localVars.length;
        }
    }

    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *) ZrMemoryRawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TUInt32) cs->closureVarCount;
        }
    }

    // 设置函数元数据
    newFunc->stackSize = (TUInt32) cs->stackSlotCount;
    newFunc->parameterCount = (TUInt16) parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TUInt32) node->location.end.line : 0;

    // 将测试函数添加到测试函数列表
    ZrArrayPush(cs->state, &cs->testFunctions, &newFunc);

    // 恢复旧的编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
}

// 编译 struct 声明
static void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_STRUCT_DECLARATION) {
        ZrCompilerError(cs, "Expected struct declaration node", node->location);
        return;
    }
    
    SZrStructDeclaration *structDecl = &node->data.structDeclaration;
    
    // 获取类型名称
    if (structDecl->name == ZR_NULL) {
        ZrCompilerError(cs, "Struct declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = structDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrCompilerError(cs, "Struct name is null", node->location);
        return;
    }
    
    // 创建 prototype 信息结构
    SZrTypePrototypeInfo info;
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    info.accessModifier = structDecl->accessModifier;
    
    // 初始化继承数组
    ZrArrayInit(cs->state, &info.inherits, sizeof(SZrString *), 4);
    
    // 处理继承关系
    if (structDecl->inherits != ZR_NULL && structDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < structDecl->inherits->count; i++) {
            SZrAstNode *inheritType = structDecl->inherits->nodes[i];
            if (inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE) {
                SZrType *type = &inheritType->data.type;
                // 提取类型名称（简化处理，只处理简单类型名）
                if (type->name != ZR_NULL && type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *inheritTypeName = type->name->data.identifier.name;
                    if (inheritTypeName != ZR_NULL) {
                        ZrArrayPush(cs->state, &info.inherits, &inheritTypeName);
                    }
                }
            }
        }
    }
    
    // 初始化成员数组
    ZrArrayInit(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 16);
    
    // 处理成员信息
    if (structDecl->members != ZR_NULL && structDecl->members->count > 0) {
        for (TZrSize i = 0; i < structDecl->members->count; i++) {
            SZrAstNode *member = structDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            SZrTypeMemberInfo memberInfo;
            memberInfo.memberType = member->type;
            memberInfo.isStatic = ZR_FALSE;
            memberInfo.accessModifier = ZR_ACCESS_PRIVATE;
            memberInfo.name = ZR_NULL;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_STRUCT_FIELD: {
                    SZrStructField *field = &member->data.structField;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isStatic = field->isStatic;
                    if (field->name != ZR_NULL) {
                        memberInfo.name = field->name->name;
                    }
                    break;
                }
                case ZR_AST_STRUCT_METHOD: {
                    SZrStructMethod *method = &member->data.structMethod;
                    memberInfo.accessModifier = method->access;
                    memberInfo.isStatic = method->isStatic;
                    if (method->name != ZR_NULL) {
                        memberInfo.name = method->name->name;
                    }
                    break;
                }
                case ZR_AST_STRUCT_META_FUNCTION: {
                    SZrStructMetaFunction *metaFunc = &member->data.structMetaFunction;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                    }
                    break;
                }
                default:
                    // 忽略未知成员类型
                    continue;
            }
            
            if (memberInfo.name != ZR_NULL) {
                ZrArrayPush(cs->state, &info.members, &memberInfo);
            }
        }
    }
    
    // 将 prototype 信息添加到数组
    ZrArrayPush(cs->state, &cs->typePrototypes, &info);
}

// 编译 class 声明
static void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CLASS_DECLARATION) {
        ZrCompilerError(cs, "Expected class declaration node", node->location);
        return;
    }
    
    SZrClassDeclaration *classDecl = &node->data.classDeclaration;
    
    // 获取类型名称
    if (classDecl->name == ZR_NULL) {
        ZrCompilerError(cs, "Class declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = classDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrCompilerError(cs, "Class name is null", node->location);
        return;
    }
    
    // 创建 prototype 信息结构
    SZrTypePrototypeInfo info;
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    info.accessModifier = classDecl->accessModifier;
    
    // 初始化继承数组
    ZrArrayInit(cs->state, &info.inherits, sizeof(SZrString *), 4);
    
    // 处理继承关系
    if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
            SZrAstNode *inheritType = classDecl->inherits->nodes[i];
            if (inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE) {
                SZrType *type = &inheritType->data.type;
                // 提取类型名称（简化处理，只处理简单类型名）
                if (type->name != ZR_NULL && type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *inheritTypeName = type->name->data.identifier.name;
                    if (inheritTypeName != ZR_NULL) {
                        ZrArrayPush(cs->state, &info.inherits, &inheritTypeName);
                    }
                }
            }
        }
    }
    
    // 初始化成员数组
    ZrArrayInit(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 16);
    
    // 处理成员信息
    if (classDecl->members != ZR_NULL && classDecl->members->count > 0) {
        for (TZrSize i = 0; i < classDecl->members->count; i++) {
            SZrAstNode *member = classDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            SZrTypeMemberInfo memberInfo;
            memberInfo.memberType = member->type;
            memberInfo.isStatic = ZR_FALSE;
            memberInfo.accessModifier = ZR_ACCESS_PRIVATE;
            memberInfo.name = ZR_NULL;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_CLASS_FIELD: {
                    SZrClassField *field = &member->data.classField;
                    memberInfo.accessModifier = field->access;
                    if (field->name != ZR_NULL) {
                        memberInfo.name = field->name->name;
                    }
                    break;
                }
                case ZR_AST_CLASS_METHOD: {
                    SZrClassMethod *method = &member->data.classMethod;
                    memberInfo.accessModifier = method->access;
                    memberInfo.isStatic = method->isStatic;
                    if (method->name != ZR_NULL) {
                        memberInfo.name = method->name->name;
                    }
                    break;
                }
                case ZR_AST_CLASS_PROPERTY: {
                    SZrClassProperty *property = &member->data.classProperty;
                    memberInfo.accessModifier = property->access;
                    // ClassProperty 没有直接的 name 字段，需要从 modifier 中提取
                    // 暂时跳过，TODO: 实现从 PropertyGet/PropertySet 中提取名称
                    break;
                }
                case ZR_AST_CLASS_META_FUNCTION: {
                    SZrClassMetaFunction *metaFunc = &member->data.classMetaFunction;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                    }
                    break;
                }
                default:
                    // 忽略未知成员类型
                    continue;
            }
            
            if (memberInfo.name != ZR_NULL) {
                ZrArrayPush(cs->state, &info.members, &memberInfo);
            }
        }
    }
    
    // 将 prototype 信息添加到数组
    ZrArrayPush(cs->state, &cs->typePrototypes, &info);
}

// 编译脚本
static void compile_script(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_SCRIPT) {
        ZrCompilerError(cs, "Expected script node", node->location);
        return;
    }

    SZrScript *script = &node->data.script;

    // 设置脚本级别标志（用于区分脚本级变量和函数内变量）
    cs->isScriptLevel = ZR_TRUE;
    
    // 保存脚本 AST 引用（用于类型查找）
    cs->scriptAst = node;

    // 1. 编译模块声明（如果有）
    if (script->moduleName != ZR_NULL) {
        // TODO: 处理模块声明（注册模块到全局模块表）
        // 目前先跳过
    }

    // 2. 编译顶层语句
    if (script->statements != ZR_NULL) {
        printf("  Compiling %zu top-level statements (statements array: %p)...\n", script->statements->count,
               (void *) script->statements);
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL) {
                // 根据语句类型编译
                switch (stmt->type) {
                    case ZR_AST_FUNCTION_DECLARATION:
                        compile_function_declaration(cs, stmt);
                        break;
                    case ZR_AST_VARIABLE_DECLARATION:
                    case ZR_AST_EXPRESSION_STATEMENT:
                    case ZR_AST_BLOCK:
                    case ZR_AST_RETURN_STATEMENT:
                    case ZR_AST_IF_EXPRESSION:
                    case ZR_AST_WHILE_LOOP:
                    case ZR_AST_FOR_LOOP:
                    case ZR_AST_FOREACH_LOOP:
                        compile_statement(cs, stmt);
                        break;
                    case ZR_AST_TEST_DECLARATION:
                        compile_test_declaration(cs, stmt);
                        break;
                    default:
                        // 其他顶层声明类型（struct, class, interface, enum, intermediate）
                        // 目前先跳过，后续实现
                        printf("    Skipping statement type %d (not implemented yet)\n", stmt->type);
                        break;
                }

                // 即使有错误，也继续编译后续语句（除非是致命错误）
                // 这样可以尽可能多地编译成功的语句
                if (cs->hasError) {
                    printf("    Compilation error at statement %zu, resetting error and continuing...\n", i);
                    // 重置错误状态，继续编译后续语句
                    cs->hasError = ZR_FALSE;
                    cs->errorMessage = ZR_NULL;
                }
            }
        }
        printf("  Finished compiling statements, total instructions: %zu\n", cs->instructionCount);
    }

    // 3. 在返回前添加导出收集代码（如果有导出的变量）
    // 导出收集在运行时进行（在 zr.import 中执行完 __entry 后）
    // 这里只需要确保导出信息被正确记录到函数中
    // 导出的变量信息已存储在 cs->pubVariables 和 cs->proVariables 中
    // 这些信息将在编译完成后复制到函数的 exportedVariables 字段中
    
    // 4. 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        // 使用 instructions.length 而不是 instructionCount，确保同步
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrValueResetAsNull(&nullValue);
            TUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) resultSlot,
                                                       (TInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrArrayGet(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrValueResetAsNull(&nullValue);
                        TUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TUInt16) resultSlot, (TInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrValueResetAsNull(&nullValue);
                TUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TUInt16) resultSlot,
                                                           (TInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }
    
    // 5. 将 prototype 信息存储到常量池中，供运行时使用
    // 运行时创建逻辑将在 zr.import 中实现（在创建模块后）
    // 这里我们将每个 prototype 的信息序列化为常量：
    // - 类型名称（字符串）
    // - 类型（STRUCT/CLASS，整数）
    // - 访问修饰符（整数）
    // - 继承关系（字符串数组）
    // - 成员信息（简化处理，暂时只存储成员名称）
    
    if (cs->typePrototypes.length > 0) {
        // 将 prototype 信息存储到常量池
        // 使用特殊标记来标识 prototype 信息常量
        // 格式：第一个常量是标记（特殊值），后续是 prototype 信息
        for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
            SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrArrayGet(&cs->typePrototypes, i);
            if (info == ZR_NULL || info->name == ZR_NULL) {
                continue;
            }
            
            // 存储类型名称
            SZrTypeValue typeNameValue;
            ZrValueInitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(info->name));
            typeNameValue.type = ZR_VALUE_TYPE_STRING;
            add_constant(cs, &typeNameValue);
            
            // 存储类型（STRUCT/CLASS）
            SZrTypeValue typeValue;
            ZrValueInitAsUInt(cs->state, &typeValue, (TUInt64)info->type);
            add_constant(cs, &typeValue);
            
            // 存储访问修饰符
            SZrTypeValue accessModifierValue;
            ZrValueInitAsUInt(cs->state, &accessModifierValue, (TUInt64)info->accessModifier);
            add_constant(cs, &accessModifierValue);
            
            // 存储继承关系数量
            SZrTypeValue inheritsCountValue;
            ZrValueInitAsUInt(cs->state, &inheritsCountValue, (TUInt64)info->inherits.length);
            add_constant(cs, &inheritsCountValue);
            
            // 存储每个继承类型名称
            for (TZrSize j = 0; j < info->inherits.length; j++) {
                SZrString **inheritTypeNamePtr = (SZrString **)ZrArrayGet(&info->inherits, j);
                if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                    SZrTypeValue inheritTypeNameValue;
                    ZrValueInitAsRawObject(cs->state, &inheritTypeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(*inheritTypeNamePtr));
                    inheritTypeNameValue.type = ZR_VALUE_TYPE_STRING;
                    add_constant(cs, &inheritTypeNameValue);
                }
            }
            
            // 存储成员数量
            SZrTypeValue membersCountValue;
            ZrValueInitAsUInt(cs->state, &membersCountValue, (TUInt64)info->members.length);
            add_constant(cs, &membersCountValue);
            
            // 存储每个成员名称（简化处理）
            for (TZrSize j = 0; j < info->members.length; j++) {
                SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrArrayGet(&info->members, j);
                if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL) {
                    SZrTypeValue memberNameValue;
                    ZrValueInitAsRawObject(cs->state, &memberNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->name));
                    memberNameValue.type = ZR_VALUE_TYPE_STRING;
                    add_constant(cs, &memberNameValue);
                }
            }
        }
        
        // 存储 prototype 数量（用于运行时知道有多少个 prototype 需要创建）
        // 使用一个特殊标记常量来标识 prototype 信息的开始
        // 格式：第一个常量是 prototype 数量
        SZrTypeValue prototypeCountValue;
        ZrValueInitAsUInt(cs->state, &prototypeCountValue, (TUInt64)cs->typePrototypes.length);
        // 在常量池开头插入（需要特殊处理，暂时先添加到末尾，运行时从末尾读取）
        // TODO: 实现更优雅的存储方式
    }
    
    // 重置脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
}

// 主编译入口（占位实现）
SZrFunction *ZrCompilerCompile(SZrState *state, SZrAstNode *ast) {
    if (state == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    SZrCompilerState cs;
    ZrCompilerStateInit(&cs, state);

    // 创建新函数
    cs.currentFunction = ZrFunctionNew(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrCompilerStateFree(&cs);
        return ZR_NULL;
    }

    // 编译脚本
    compile_script(&cs, ast);

    if (cs.hasError) {
        // 错误信息已在 ZrCompilerError 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrFunctionFree(state, cs.currentFunction);
        }
        if (cs.topLevelFunction != ZR_NULL) {
            ZrFunctionFree(state, cs.topLevelFunction);
        }
        ZrCompilerStateFree(&cs);
        return ZR_NULL;
    }

    // 如果有顶层函数声明，返回它；否则返回脚本函数
    SZrFunction *func = (cs.topLevelFunction != ZR_NULL) ? cs.topLevelFunction : cs.currentFunction;
    SZrGlobalState *global = state->global;

    // 注意：如果返回的是 topLevelFunction，它的指令已经在 compile_function_declaration 中复制了
    // 我们只需要处理脚本包装函数（currentFunction）的情况
    if (func == cs.currentFunction) {
        // 1. 复制指令列表（仅对脚本包装函数）
        // 使用 instructions.length 而不是 instructionCount，确保同步
        if (cs.instructions.length > 0) {
            TZrSize instSize = cs.instructions.length * sizeof(TZrInstruction);
            func->instructionsList =
                    (TZrInstruction *) ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->instructionsList == ZR_NULL) {
                ZrFunctionFree(state, func);
                ZrCompilerStateFree(&cs);
                return ZR_NULL;
            }
            memcpy(func->instructionsList, cs.instructions.head, instSize);
            func->instructionsLength = (TUInt32) cs.instructions.length;
            // 同步 instructionCount
            cs.instructionCount = cs.instructions.length;
        } else {
            func->instructionsLength = 0;
            func->instructionsList = ZR_NULL;
        }
    }

    // 注意：如果返回的是 topLevelFunction，它的常量、局部变量、闭包变量等
    // 已经在 compile_function_declaration 中复制了
    // 我们只需要处理脚本包装函数（currentFunction）的情况
    if (func == cs.currentFunction) {
        // 2. 复制常量列表（仅对脚本包装函数）
        // 使用 constants.length 而不是 constantCount，确保同步
        if (cs.constants.length > 0) {
            TZrSize constSize = cs.constants.length * sizeof(SZrTypeValue);
            func->constantValueList =
                    (SZrTypeValue *) ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->constantValueList == ZR_NULL) {
                ZrFunctionFree(state, func);
                ZrCompilerStateFree(&cs);
                return ZR_NULL;
            }
            memcpy(func->constantValueList, cs.constants.head, constSize);
            func->constantValueLength = (TUInt32) cs.constants.length;
            // 同步 constantCount
            cs.constantCount = cs.constants.length;
        } else {
            func->constantValueLength = 0;
            func->constantValueList = ZR_NULL;
        }

        // 3. 复制局部变量列表（仅对脚本包装函数）
        // 使用 localVars.length 而不是 localVarCount，确保同步
        if (cs.localVars.length > 0) {
            TZrSize localVarSize = cs.localVars.length * sizeof(SZrFunctionLocalVariable);
            func->localVariableList = (SZrFunctionLocalVariable *) ZrMemoryRawMallocWithType(
                    global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->localVariableList == ZR_NULL) {
                ZrFunctionFree(state, func);
                ZrCompilerStateFree(&cs);
                return ZR_NULL;
            }
            memcpy(func->localVariableList, cs.localVars.head, localVarSize);
            func->localVariableLength = (TUInt32) cs.localVars.length;
            // 同步 localVarCount
            cs.localVarCount = cs.localVars.length;
        } else {
            func->localVariableLength = 0;
            func->localVariableList = ZR_NULL;
        }

        // 4. 复制闭包变量列表（仅对脚本包装函数）
        if (cs.closureVarCount > 0) {
            TZrSize closureVarSize = cs.closureVarCount * sizeof(SZrFunctionClosureVariable);
            func->closureValueList = (SZrFunctionClosureVariable *) ZrMemoryRawMallocWithType(
                    global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->closureValueList == ZR_NULL) {
                ZrFunctionFree(state, func);
                ZrCompilerStateFree(&cs);
                return ZR_NULL;
            }
            memcpy(func->closureValueList, cs.closureVars.head, closureVarSize);
            func->closureValueLength = (TUInt32) cs.closureVarCount;
        } else {
            func->closureValueLength = 0;
            func->closureValueList = ZR_NULL;
        }
    }

    // 5. 复制子函数列表
    if (cs.childFunctions.length > 0) {
        TZrSize childFuncSize = cs.childFunctions.length * sizeof(SZrFunction);
        func->childFunctionList =
                (struct SZrFunction *) ZrMemoryRawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->childFunctionList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        // 从指针数组复制到对象数组
        SZrFunction **srcArray = (SZrFunction **) cs.childFunctions.head;
        for (TZrSize i = 0; i < cs.childFunctions.length; i++) {
            if (srcArray[i] != ZR_NULL) {
                func->childFunctionList[i] = *srcArray[i];
            }
        }
        func->childFunctionLength = (TUInt32) cs.childFunctions.length;
    }

    // 6. 复制导出变量信息（合并 pubVariables 和 proVariables）
    // proVariables 已经包含所有 pubVariables，所以只需要复制 proVariables
    if (cs.proVariables.length > 0) {
        TZrSize exportVarSize = cs.proVariables.length * sizeof(struct SZrFunctionExportedVariable);
        func->exportedVariables = (struct SZrFunctionExportedVariable *) ZrMemoryRawMallocWithType(
                global, exportVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->exportedVariables == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_NULL;
        }
        // 从 SZrExportedVariable 复制到 SZrFunctionExportedVariable
        for (TZrSize i = 0; i < cs.proVariables.length; i++) {
            SZrExportedVariable *src = (SZrExportedVariable *) ZrArrayGet(&cs.proVariables, i);
            if (src != ZR_NULL) {
                func->exportedVariables[i].name = src->name;
                func->exportedVariables[i].stackSlot = src->stackSlot;
                func->exportedVariables[i].accessModifier = (TUInt8) src->accessModifier;
            }
        }
        func->exportedVariableLength = (TUInt32) cs.proVariables.length;
    } else {
        func->exportedVariables = ZR_NULL;
        func->exportedVariableLength = 0;
    }

    // 7. 设置函数元数据
    // 注意：脚本入口函数没有参数（它是脚本的包装函数）
    // 脚本中的函数声明会被编译为子函数，它们的参数信息已通过 compile_function_declaration 正确设置
    // 但是，如果返回的是顶层函数声明，参数信息已经在 compile_function_declaration 中设置了，不应该覆盖
    func->stackSize = (TUInt32) cs.stackSlotCount;
    if (cs.topLevelFunction == ZR_NULL) {
        // 只有当返回的是脚本函数时，才设置参数数量为0
        func->parameterCount = 0;  // 脚本入口函数没有参数
        func->hasVariableArguments = ZR_FALSE;  // 脚本入口函数不支持可变参数
    }
    // 如果返回的是顶层函数，parameterCount 和 hasVariableArguments 已经在 compile_function_declaration 中设置了
    func->lineInSourceStart = (ast->location.start.line > 0) ? (TUInt32) ast->location.start.line : 0;
    func->lineInSourceEnd = (ast->location.end.line > 0) ? (TUInt32) ast->location.end.line : 0;

    // 确保所有字段都被正确初始化（避免 ZrFunctionFree 中的断言失败）
    if (func->instructionsList == ZR_NULL) {
        func->instructionsLength = 0;
    }
    if (func->constantValueList == ZR_NULL) {
        func->constantValueLength = 0;
    }
    if (func->localVariableList == ZR_NULL) {
        func->localVariableLength = 0;
    }
    if (func->closureValueList == ZR_NULL) {
        func->closureValueLength = 0;
    }
    if (func->childFunctionList == ZR_NULL) {
        func->childFunctionLength = 0;
    }
    if (func->executionLocationInfoList == ZR_NULL) {
        func->executionLocationInfoLength = 0;
    }
    if (func->exportedVariables == ZR_NULL) {
        func->exportedVariableLength = 0;
    }

    // 执行指令优化（占位实现，后续填充具体逻辑）
    optimize_instructions(&cs);

    ZrCompilerStateFree(&cs);
    return func;
}

// 编译 AST 为函数和测试函数列表（新接口）
TBool ZrCompilerCompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result) {
    if (state == ZR_NULL || ast == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    // 初始化结果结构体
    result->mainFunction = ZR_NULL;
    result->testFunctions = ZR_NULL;
    result->testFunctionCount = 0;

    SZrCompilerState cs;
    ZrCompilerStateInit(&cs, state);

    // 创建新函数
    cs.currentFunction = ZrFunctionNew(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrCompilerStateFree(&cs);
        return ZR_FALSE;
    }

    // 编译脚本
    compile_script(&cs, ast);

    if (cs.hasError) {
        // 错误信息已在 ZrCompilerError 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrFunctionFree(state, cs.currentFunction);
        }
        ZrCompilerStateFree(&cs);
        return ZR_FALSE;
    }

    // 将编译结果复制到 SZrFunction（与ZrCompilerCompile相同的逻辑）
    SZrFunction *func = cs.currentFunction;
    SZrGlobalState *global = state->global;

    // 1. 复制指令列表
    if (cs.instructions.length > 0) {
        TZrSize instSize = cs.instructions.length * sizeof(TZrInstruction);
        func->instructionsList =
                (TZrInstruction *) ZrMemoryRawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->instructionsList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        memcpy(func->instructionsList, cs.instructions.head, instSize);
        func->instructionsLength = (TUInt32) cs.instructions.length;
        cs.instructionCount = cs.instructions.length;
    }

    // 2. 复制常量列表
    // 使用 constants.length 而不是 constantCount，确保同步
    if (cs.constants.length > 0) {
        TZrSize constSize = cs.constants.length * sizeof(SZrTypeValue);
        func->constantValueList =
                (SZrTypeValue *) ZrMemoryRawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->constantValueList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        memcpy(func->constantValueList, cs.constants.head, constSize);
        func->constantValueLength = (TUInt32) cs.constants.length;
        // 同步 constantCount
        cs.constantCount = cs.constants.length;
    }

    // 3. 复制局部变量列表
    if (cs.localVars.length > 0) {
        TZrSize localVarSize = cs.localVars.length * sizeof(SZrFunctionLocalVariable);
        func->localVariableList = (SZrFunctionLocalVariable *) ZrMemoryRawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->localVariableList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        memcpy(func->localVariableList, cs.localVars.head, localVarSize);
        func->localVariableLength = (TUInt32) cs.localVars.length;
        cs.localVarCount = cs.localVars.length;
    }

    // 4. 复制闭包变量列表
    if (cs.closureVarCount > 0) {
        TZrSize closureVarSize = cs.closureVarCount * sizeof(SZrFunctionClosureVariable);
        func->closureValueList = (SZrFunctionClosureVariable *) ZrMemoryRawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->closureValueList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        memcpy(func->closureValueList, cs.closureVars.head, closureVarSize);
        func->closureValueLength = (TUInt32) cs.closureVarCount;
    }

    // 5. 复制子函数列表
    if (cs.childFunctions.length > 0) {
        TZrSize childFuncSize = cs.childFunctions.length * sizeof(SZrFunction);
        func->childFunctionList =
                (struct SZrFunction *) ZrMemoryRawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->childFunctionList == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        SZrFunction **srcArray = (SZrFunction **) cs.childFunctions.head;
        for (TZrSize i = 0; i < cs.childFunctions.length; i++) {
            if (srcArray[i] != ZR_NULL) {
                func->childFunctionList[i] = *srcArray[i];
            }
        }
        func->childFunctionLength = (TUInt32) cs.childFunctions.length;
    }

    // 6. 复制导出变量信息（合并 pubVariables 和 proVariables）
    // proVariables 已经包含所有 pubVariables，所以只需要复制 proVariables
    if (cs.proVariables.length > 0) {
        TZrSize exportVarSize = cs.proVariables.length * sizeof(struct SZrFunctionExportedVariable);
        func->exportedVariables = (struct SZrFunctionExportedVariable *) ZrMemoryRawMallocWithType(
                global, exportVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->exportedVariables == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        // 从 SZrExportedVariable 复制到 SZrFunctionExportedVariable
        for (TZrSize i = 0; i < cs.proVariables.length; i++) {
            SZrExportedVariable *src = (SZrExportedVariable *) ZrArrayGet(&cs.proVariables, i);
            if (src != ZR_NULL) {
                func->exportedVariables[i].name = src->name;
                func->exportedVariables[i].stackSlot = src->stackSlot;
                func->exportedVariables[i].accessModifier = (TUInt8) src->accessModifier;
            }
        }
        func->exportedVariableLength = (TUInt32) cs.proVariables.length;
    } else {
        func->exportedVariables = ZR_NULL;
        func->exportedVariableLength = 0;
    }

    // 7. 设置函数元数据
    func->stackSize = (TUInt32) cs.stackSlotCount;
    func->parameterCount = 0;
    func->hasVariableArguments = ZR_FALSE;
    func->lineInSourceStart = (ast->location.start.line > 0) ? (TUInt32) ast->location.start.line : 0;
    func->lineInSourceEnd = (ast->location.end.line > 0) ? (TUInt32) ast->location.end.line : 0;

    // 确保所有字段都被正确初始化
    if (func->instructionsList == ZR_NULL) {
        func->instructionsLength = 0;
    }
    if (func->constantValueList == ZR_NULL) {
        func->constantValueLength = 0;
    }
    if (func->localVariableList == ZR_NULL) {
        func->localVariableLength = 0;
    }
    if (func->closureValueList == ZR_NULL) {
        func->closureValueLength = 0;
    }
    if (func->childFunctionList == ZR_NULL) {
        func->childFunctionLength = 0;
    }
    if (func->executionLocationInfoList == ZR_NULL) {
        func->executionLocationInfoLength = 0;
    }
    if (func->exportedVariables == ZR_NULL) {
        func->exportedVariableLength = 0;
    }

    // 执行指令优化（占位实现，后续填充具体逻辑）
    optimize_instructions(&cs);

    // 复制测试函数列表
    result->mainFunction = func;
    if (cs.testFunctions.length > 0) {
        TZrSize testFuncSize = cs.testFunctions.length * sizeof(SZrFunction *);
        result->testFunctions = (SZrFunction **) ZrMemoryRawMallocWithType(
                global, testFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (result->testFunctions == ZR_NULL) {
            ZrFunctionFree(state, func);
            ZrCompilerStateFree(&cs);
            return ZR_FALSE;
        }
        // 复制测试函数指针
        SZrFunction **srcTestArray = (SZrFunction **) cs.testFunctions.head;
        for (TZrSize i = 0; i < cs.testFunctions.length; i++) {
            result->testFunctions[i] = srcTestArray[i];
        }
        result->testFunctionCount = cs.testFunctions.length;
    }

    ZrCompilerStateFree(&cs);
    return ZR_TRUE;
}

// 释放编译结果
void ZrCompileResultFree(SZrState *state, SZrCompileResult *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    // 释放测试函数数组（函数对象本身由GC管理，不需要释放）
    if (result->testFunctions != ZR_NULL && result->testFunctionCount > 0) {
        SZrGlobalState *global = state->global;
        TZrSize testFuncSize = result->testFunctionCount * sizeof(SZrFunction *);
        ZrMemoryRawFreeWithType(global, result->testFunctions, testFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        result->testFunctions = ZR_NULL;
        result->testFunctionCount = 0;
    }

    // 主函数由调用者负责释放（如果不需要可以调用ZrFunctionFree）
    // 这里不释放，因为调用者可能还需要使用
}

// 编译源代码为函数（封装了从解析到编译的全流程）
struct SZrFunction *ZrParserCompileSource(struct SZrState *state, const TChar *source, TZrSize sourceLength, struct SZrString *sourceName) {
    if (state == ZR_NULL || source == ZR_NULL || sourceLength == 0) {
        return ZR_NULL;
    }
    
    // 解析源代码为AST
    SZrAstNode *ast = ZrParserParse(state, source, sourceLength, sourceName);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 编译AST为函数
    SZrFunction *func = ZrCompilerCompile(state, ast);
    
    // 释放AST
    ZrParserFreeAst(state, ast);
    
    return func;
}

// 注册 compileSource 函数到 globalState
void ZrParserRegisterToGlobalState(struct SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }
    
    // 使用 API 设置 compileSource 函数指针，避免直接访问内部结构
    ZrGlobalStateSetCompileSource(state->global, ZrParserCompileSource);
}
