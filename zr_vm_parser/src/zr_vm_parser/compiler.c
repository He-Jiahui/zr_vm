//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include <stdio.h>
#include <string.h>
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

// 前向声明编译期执行函数
extern TZrBool ZrParser_CompileTimeDeclaration_Execute(SZrCompilerState *cs, SZrAstNode *node);

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/gc.h"

#include <string.h>

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

// 前向声明（这些函数在其他文件中实现）
extern void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node);
extern void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);
static void compile_script(SZrCompilerState *cs, SZrAstNode *node);
void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node);
void ZrParser_Compiler_PredeclareFunctionBindings(SZrCompilerState *cs, SZrAstNodeArray *statements);
void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements);
static void compile_test_declaration(SZrCompilerState *cs, SZrAstNode *node);
static void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node);
static void compile_extern_block_declaration(SZrCompilerState *cs, SZrAstNode *node);
void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node);

// 编译期执行函数（在 compile_time_executor.c 中实现）
extern TZrBool ZrParser_CompileTimeDeclaration_Execute(SZrCompilerState *cs, SZrAstNode *node);
static void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node);
static void compile_meta_function(SZrCompilerState *cs, SZrAstNode *node, EZrMetaType metaType);
static SZrFunction *compile_class_member_function(SZrCompilerState *cs, SZrAstNode *node,
                                                  SZrString *superTypeName,
                                                  TZrBool injectThis, TZrUInt32 *outParameterCount);
static SZrString *create_hidden_extern_local_name(SZrCompilerState *cs, const TZrChar *prefix);
static EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node);
static EZrOwnershipQualifier get_implicit_this_ownership_qualifier(EZrOwnershipQualifier receiverQualifier);
TZrSize create_label(SZrCompilerState *cs);
void resolve_label(SZrCompilerState *cs, TZrSize labelId);

// 将prototype信息序列化为二进制数据（不存储到常量池）
// 返回：序列化数据的指针和大小，通过参数返回
// 返回 ZR_TRUE 表示成功，ZR_FALSE 表示失败
// 注意：outData 指向的内存需要调用者释放
static TZrBool serialize_prototype_info_to_binary(SZrCompilerState *cs, SZrTypePrototypeInfo *info, 
                                                 TZrByte **outData, TZrSize *outSize);
static TZrInt64 compiler_internal_import_native_entry(SZrState *state);

static EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    switch (node->type) {
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.receiverQualifier;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.receiverQualifier;
        default:
            return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
}

static EZrOwnershipQualifier get_implicit_this_ownership_qualifier(EZrOwnershipQualifier receiverQualifier) {
    if (receiverQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
        receiverQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) {
        return ZR_OWNERSHIP_QUALIFIER_BORROWED;
    }

    return receiverQualifier;
}

// 初始化编译器状态
void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state) {
    ZR_ASSERT(cs != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);

    cs->state = state;
    cs->currentFunction = ZR_NULL;
    cs->currentAst = ZR_NULL;
    cs->semanticContext = ZrParser_SemanticContext_New(state);
    cs->hirModule = ZR_NULL;

    // 初始化常量池
    ZrCore_Array_Init(state, &cs->constants, sizeof(SZrTypeValue), 16);
    cs->constantCount = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;

    // 初始化局部变量数组
    ZrCore_Array_Init(state, &cs->localVars, sizeof(SZrFunctionLocalVariable), 16);
    cs->localVarCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;

    // 初始化闭包变量数组
    ZrCore_Array_Init(state, &cs->closureVars, sizeof(SZrFunctionClosureVariable), 8);
    cs->closureVarCount = 0;

    // 初始化指令数组
    ZrCore_Array_Init(state, &cs->instructions, sizeof(TZrInstruction), 64);
    cs->instructionCount = 0;

    // 初始化作用域栈
    ZrCore_Array_Init(state, &cs->scopeStack, sizeof(SZrScope), 8);

    // 初始化标签数组
    ZrCore_Array_Init(state, &cs->labels, sizeof(SZrLabel), 8);

    // 初始化待解析跳转数组
    ZrCore_Array_Init(state, &cs->pendingJumps, sizeof(SZrPendingJump), 8);
    ZrCore_Array_Init(state, &cs->pendingAbsolutePatches, sizeof(SZrPendingAbsolutePatch), 8);

    // 初始化循环标签栈
    ZrCore_Array_Init(state, &cs->loopLabelStack, sizeof(SZrLoopLabel), 4);
    ZrCore_Array_Init(state, &cs->tryContextStack, sizeof(SZrCompilerTryContext), 4);

    // 初始化调试与异常元数据
    ZrCore_Array_Init(state, &cs->executionLocations, sizeof(SZrFunctionExecutionLocationInfo), 32);
    ZrCore_Array_Init(state, &cs->catchClauseInfos, sizeof(SZrCompilerCatchClauseInfo), 8);
    ZrCore_Array_Init(state, &cs->exceptionHandlerInfos, sizeof(SZrCompilerExceptionHandlerInfo), 4);

    // 初始化子函数数组
    ZrCore_Array_Init(state, &cs->childFunctions, sizeof(SZrFunction *), 8);
    
    // 初始化函数名到子函数索引的映射数组（仅用于编译时查找）
    ZrCore_Array_Init(state, &cs->childFunctionNameMap, sizeof(SZrChildFunctionNameMap), 8);

    // 初始化顶层函数声明
    cs->topLevelFunction = ZR_NULL;

    // 初始化错误状态
    cs->hasError = ZR_FALSE;
    cs->hasFatalError = ZR_FALSE;
    cs->hasCompileTimeError = ZR_FALSE;
    cs->errorMessage = ZR_NULL;
    cs->errorLocation.start.line = 0;
    cs->errorLocation.start.column = 0;
    cs->errorLocation.end.line = 0;
    cs->errorLocation.end.column = 0;

    // 初始化测试模式
    cs->isTestMode = ZR_FALSE;
    ZrCore_Array_Init(state, &cs->testFunctions, sizeof(SZrFunction *), 8);
    
    // 初始化尾调用上下文
    cs->isInTailCallContext = ZR_FALSE;
    
    // 初始化外部变量引用数组
    ZrCore_Array_Init(state, &cs->referencedExternalVars, sizeof(SZrString *), 8);
    
    // 初始化类型环境
    cs->typeEnv = ZrParser_TypeEnvironment_New(state);
    if (cs->typeEnv != ZR_NULL) {
        cs->typeEnv->semanticContext = cs->semanticContext;
    }
    ZrCore_Array_Init(state, &cs->typeEnvStack, sizeof(SZrTypeEnvironment *), 8);
    
    // 初始化模块导出跟踪数组
    ZrCore_Array_Init(state, &cs->pubVariables, sizeof(SZrExportedVariable), 8);
    ZrCore_Array_Init(state, &cs->proVariables, sizeof(SZrExportedVariable), 8);
    ZrCore_Array_Init(state, &cs->exportedTypes, sizeof(SZrString *), 4);  // TODO: 暂时存储类型名
    
    // 初始化脚本 AST 引用
    cs->scriptAst = ZR_NULL;
    
    // 初始化脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
    
    // 初始化类型 Prototype 信息数组
    ZrCore_Array_Init(state, &cs->typePrototypes, sizeof(SZrTypePrototypeInfo), 8);
    cs->currentTypePrototypeInfo = ZR_NULL;
    cs->externBindingsPredeclared = ZR_FALSE;
    
    // 初始化编译期环境
    cs->compileTimeTypeEnv = ZrParser_TypeEnvironment_New(state);
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        cs->compileTimeTypeEnv->semanticContext = ZR_NULL;
    }
    ZrCore_Array_Init(state, &cs->compileTimeVariables, sizeof(SZrCompileTimeVariable*), 8);
    ZrCore_Array_Init(state, &cs->compileTimeFunctions, sizeof(SZrCompileTimeFunction*), 8);
    cs->isInCompileTimeContext = ZR_FALSE;
    
    // 初始化构造函数上下文
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = ZR_NULL;
    cs->currentTypeName = ZR_NULL;
    
    // 初始化 const 变量跟踪数组
    ZrCore_Array_Init(state, &cs->constLocalVars, sizeof(SZrString *), 8);
    ZrCore_Array_Init(state, &cs->constParameters, sizeof(SZrString *), 8);
}

// 清理解译器状态
void ZrParser_CompilerState_Free(SZrCompilerState *cs) {
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
        ZrCore_Array_Free(state, &cs->constants);
    }

    // 释放局部变量数组
    if (cs->localVars.isValid && cs->localVars.head != ZR_NULL && cs->localVars.capacity > 0 &&
        cs->localVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->localVars);
    }

    // 释放闭包变量数组
    if (cs->closureVars.isValid && cs->closureVars.head != ZR_NULL && cs->closureVars.capacity > 0 &&
        cs->closureVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->closureVars);
    }

    // 释放指令数组
    if (cs->instructions.isValid && cs->instructions.head != ZR_NULL && cs->instructions.capacity > 0 &&
        cs->instructions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->instructions);
    }

    // 释放作用域栈
    if (cs->scopeStack.isValid && cs->scopeStack.head != ZR_NULL && cs->scopeStack.capacity > 0 &&
        cs->scopeStack.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->scopeStack);
    }

    // 释放标签数组
    if (cs->labels.isValid && cs->labels.head != ZR_NULL && cs->labels.capacity > 0 && cs->labels.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->labels);
    }

    // 释放待解析跳转数组
    if (cs->pendingJumps.isValid && cs->pendingJumps.head != ZR_NULL && cs->pendingJumps.capacity > 0 &&
        cs->pendingJumps.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->pendingJumps);
    }

    if (cs->pendingAbsolutePatches.isValid && cs->pendingAbsolutePatches.head != ZR_NULL &&
        cs->pendingAbsolutePatches.capacity > 0 && cs->pendingAbsolutePatches.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->pendingAbsolutePatches);
    }

    // 释放循环标签栈
    if (cs->loopLabelStack.isValid && cs->loopLabelStack.head != ZR_NULL && cs->loopLabelStack.capacity > 0 &&
        cs->loopLabelStack.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->loopLabelStack);
    }

    if (cs->tryContextStack.isValid && cs->tryContextStack.head != ZR_NULL && cs->tryContextStack.capacity > 0 &&
        cs->tryContextStack.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->tryContextStack);
    }

    if (cs->executionLocations.isValid && cs->executionLocations.head != ZR_NULL &&
        cs->executionLocations.capacity > 0 && cs->executionLocations.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->executionLocations);
    }

    if (cs->catchClauseInfos.isValid && cs->catchClauseInfos.head != ZR_NULL &&
        cs->catchClauseInfos.capacity > 0 && cs->catchClauseInfos.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->catchClauseInfos);
    }

    if (cs->exceptionHandlerInfos.isValid && cs->exceptionHandlerInfos.head != ZR_NULL &&
        cs->exceptionHandlerInfos.capacity > 0 && cs->exceptionHandlerInfos.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->exceptionHandlerInfos);
    }

    // 释放子函数数组（函数本身由 GC 管理）
    if (cs->childFunctions.isValid && cs->childFunctions.head != ZR_NULL && cs->childFunctions.capacity > 0 &&
        cs->childFunctions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->childFunctions);
    }
    
    // 释放测试函数数组（函数本身由 GC 管理）
    if (cs->testFunctions.isValid && cs->testFunctions.head != ZR_NULL && cs->testFunctions.capacity > 0 &&
        cs->testFunctions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->testFunctions);
    }
    
    // 释放外部变量引用数组（字符串本身由 GC 管理）
    if (cs->referencedExternalVars.isValid && cs->referencedExternalVars.head != ZR_NULL && 
        cs->referencedExternalVars.capacity > 0 && cs->referencedExternalVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->referencedExternalVars);
    }
    
    // 释放类型环境栈
    if (cs->typeEnvStack.isValid && cs->typeEnvStack.head != ZR_NULL && 
        cs->typeEnvStack.capacity > 0 && cs->typeEnvStack.elementSize > 0) {
        // 释放栈中的所有环境（从栈顶到栈底）
        for (TZrSize i = 0; i < cs->typeEnvStack.length; i++) {
            SZrTypeEnvironment **envPtr = (SZrTypeEnvironment **)ZrCore_Array_Get(&cs->typeEnvStack, i);
            if (envPtr != ZR_NULL && *envPtr != ZR_NULL) {
                ZrParser_TypeEnvironment_Free(state, *envPtr);
            }
        }
        ZrCore_Array_Free(state, &cs->typeEnvStack);
    }
    
    // 释放当前类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, cs->typeEnv);
        cs->typeEnv = ZR_NULL;
    }

    if (cs->hirModule != ZR_NULL) {
        ZrParser_HirModule_Free(state, cs->hirModule);
        cs->hirModule = ZR_NULL;
    }
    
    // 释放类型 Prototype 信息数组
    if (cs->typePrototypes.isValid && cs->typePrototypes.head != ZR_NULL &&
        cs->typePrototypes.capacity > 0 && cs->typePrototypes.elementSize > 0) {
        // 释放每个 prototype 信息中的嵌套数组
        for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
            SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
            if (info != ZR_NULL) {
                // 释放 inherits 数组（字符串本身由 GC 管理）
                if (info->inherits.isValid && info->inherits.head != ZR_NULL &&
                    info->inherits.capacity > 0 && info->inherits.elementSize > 0) {
                    ZrCore_Array_Free(state, &info->inherits);
                }
                if (info->implements.isValid && info->implements.head != ZR_NULL &&
                    info->implements.capacity > 0 && info->implements.elementSize > 0) {
                    ZrCore_Array_Free(state, &info->implements);
                }
                // 释放 members 数组
                if (info->members.isValid && info->members.head != ZR_NULL &&
                    info->members.capacity > 0 && info->members.elementSize > 0) {
                    ZrCore_Array_Free(state, &info->members);
                }
            }
        }
        ZrCore_Array_Free(state, &cs->typePrototypes);
    }
    
    // 释放模块导出跟踪数组（字符串本身由 GC 管理）
    if (cs->pubVariables.isValid && cs->pubVariables.head != ZR_NULL && 
        cs->pubVariables.capacity > 0 && cs->pubVariables.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->pubVariables);
    }
    if (cs->proVariables.isValid && cs->proVariables.head != ZR_NULL && 
        cs->proVariables.capacity > 0 && cs->proVariables.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->proVariables);
    }
    if (cs->exportedTypes.isValid && cs->exportedTypes.head != ZR_NULL && 
        cs->exportedTypes.capacity > 0 && cs->exportedTypes.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->exportedTypes);
    }
    
    // 释放编译期变量表
    if (cs->compileTimeVariables.isValid && cs->compileTimeVariables.head != ZR_NULL && 
        cs->compileTimeVariables.capacity > 0 && cs->compileTimeVariables.elementSize > 0) {
        // 释放每个编译期变量及其类型
        for (TZrSize i = 0; i < cs->compileTimeVariables.length; i++) {
            SZrCompileTimeVariable **varPtr = (SZrCompileTimeVariable**)ZrCore_Array_Get(&cs->compileTimeVariables, i);
            if (varPtr != ZR_NULL && *varPtr != ZR_NULL) {
                SZrCompileTimeVariable *var = *varPtr;
                // 释放类型（包括嵌套的elementTypes）
                ZrParser_InferredType_Free(state, &var->type);
                // 释放变量结构体本身（字符串和AST节点由GC管理）
                ZrCore_Memory_RawFreeWithType(state->global, var, sizeof(SZrCompileTimeVariable), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeVariables);
    }
    
    // 释放 const 变量跟踪数组
    if (cs->constLocalVars.isValid && cs->constLocalVars.head != ZR_NULL && 
        cs->constLocalVars.capacity > 0 && cs->constLocalVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->constLocalVars);
    }
    if (cs->constParameters.isValid && cs->constParameters.head != ZR_NULL && 
        cs->constParameters.capacity > 0 && cs->constParameters.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->constParameters);
    }
    
    // 释放编译期函数表
    if (cs->compileTimeFunctions.isValid && cs->compileTimeFunctions.head != ZR_NULL && 
        cs->compileTimeFunctions.capacity > 0 && cs->compileTimeFunctions.elementSize > 0) {
        // 释放每个编译期函数及其类型信息
        for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
            SZrCompileTimeFunction **funcPtr = (SZrCompileTimeFunction**)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
            if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL) {
                SZrCompileTimeFunction *func = *funcPtr;
                // 释放返回类型（包括嵌套的elementTypes）
                ZrParser_InferredType_Free(state, &func->returnType);
                // 释放参数类型数组中的每个类型
                if (func->paramTypes.isValid && func->paramTypes.head != ZR_NULL && 
                    func->paramTypes.capacity > 0 && func->paramTypes.elementSize > 0) {
                    for (TZrSize j = 0; j < func->paramTypes.length; j++) {
                        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, j);
                        if (paramType != ZR_NULL) {
                            ZrParser_InferredType_Free(state, paramType);
                        }
                    }
                    ZrCore_Array_Free(state, &func->paramTypes);
                }
                // 释放函数结构体本身（字符串和AST节点由GC管理）
                ZrCore_Memory_RawFreeWithType(state->global, func, sizeof(SZrCompileTimeFunction), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeFunctions);
    }
    
    // 释放编译期类型环境
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, cs->compileTimeTypeEnv);
        cs->compileTimeTypeEnv = ZR_NULL;
    }

    if (cs->semanticContext != ZR_NULL) {
        ZrParser_SemanticContext_Free(cs->semanticContext);
        cs->semanticContext = ZR_NULL;
    }
}

// 报告编译错误
// 辅助函数：根据错误消息提供解决建议
static void print_error_suggestion(const TZrChar *msg) {
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
        fprintf(stderr, "  Suggestion: Declaration types (interface, struct, class, enum, function) cannot be used as statements or expressions.\n");
        fprintf(stderr, "              They should only appear in their proper declaration contexts (top-level, class body, etc.).\n");
        fprintf(stderr, "              Check if you accidentally placed a declaration inside a block or expression context.\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: This AST node type is not supported in expression context.\n");
        fprintf(stderr, "              Possible causes:\n");
        fprintf(stderr, "              1. The node was incorrectly parsed or placed in the wrong context\n");
        fprintf(stderr, "              2. A declaration type was mistakenly used as an expression\n");
        fprintf(stderr, "              3. Missing implementation for this node type in ZrParser_Expression_Compile\n");
        fprintf(stderr, "              Check the AST structure and ensure the node is in the correct context.\n");
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Type mismatch detected. Check:\n");
        fprintf(stderr, "              1. Variable types match their assignments\n");
        fprintf(stderr, "              2. Function argument types match the function signature\n");
        fprintf(stderr, "              3. Return types match the function declaration\n");
        fprintf(stderr, "              4. Type annotations are correct\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: The referenced identifier was not found. Check:\n");
        fprintf(stderr, "              1. Variable/function is declared before use\n");
        fprintf(stderr, "              2. Variable/function is in scope\n");
        fprintf(stderr, "              3. Spelling is correct\n");
        fprintf(stderr, "              4. Import statements are correct (if using modules)\n");
    } else if (strstr(msg, "Destructuring") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Destructuring patterns can only be used in variable declarations.\n");
        fprintf(stderr, "              They cannot be used as standalone expressions or statements.\n");
    } else if (strstr(msg, "Loop or statement") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Control flow statements (if, while, for, etc.) cannot be used as expressions.\n");
        fprintf(stderr, "              Use them as statements, or use expression forms (if expression, etc.) if available.\n");
    } else if (strstr(msg, "Failed to") != ZR_NULL) {
        fprintf(stderr, "  Suggestion: Internal compiler error. This may indicate:\n");
        fprintf(stderr, "              1. Memory allocation failure\n");
        fprintf(stderr, "              2. Invalid compiler state\n");
        fprintf(stderr, "              3. Bug in the compiler\n");
        fprintf(stderr, "              Please report this issue with the source code that triggered it.\n");
    }
}

// 编译期错误报告
void ZrParser_CompileTime_Error(SZrCompilerState *cs, EZrCompileTimeErrorLevel level, const TZrChar *message, SZrFileRange location) {
    if (cs == ZR_NULL || message == ZR_NULL) {
        return;
    }
    
    const TZrChar *levelStr = "INFO";
    switch (level) {
        case ZR_COMPILE_TIME_ERROR_INFO:
            levelStr = "INFO";
            break;
        case ZR_COMPILE_TIME_ERROR_WARNING:
            levelStr = "WARNING";
            break;
        case ZR_COMPILE_TIME_ERROR_ERROR:
            levelStr = "ERROR";
            cs->hasError = ZR_TRUE;
            cs->hasCompileTimeError = ZR_TRUE;
            break;
        case ZR_COMPILE_TIME_ERROR_FATAL:
            levelStr = "FATAL";
            cs->hasError = ZR_TRUE;
            cs->hasCompileTimeError = ZR_TRUE;
            break;
    }
    
    // 输出错误信息
    const TZrChar *fileName = "<unknown>";
    if (location.source != ZR_NULL) {
        TZrNativeString nameStr = ZrCore_String_GetNativeString(location.source);
        if (nameStr != ZR_NULL) {
            fileName = nameStr;
        }
    }
    
    fprintf(stderr, "[CompileTime %s] %s:%d:%d: %s\n", 
           levelStr, fileName, location.start.line, location.start.column, message);
    
    // 如果是致命错误，设置错误信息
    if (level == ZR_COMPILE_TIME_ERROR_ERROR || level == ZR_COMPILE_TIME_ERROR_FATAL) {
        if (cs->errorMessage == ZR_NULL) {
            cs->errorMessage = (level == ZR_COMPILE_TIME_ERROR_FATAL)
                                       ? "Fatal compile-time evaluation failed"
                                       : "Compile-time evaluation failed";
            cs->errorLocation = location;
        }
    }

    if (level == ZR_COMPILE_TIME_ERROR_FATAL) {
        cs->hasFatalError = ZR_TRUE;
    }
}

void ZrParser_Compiler_Error(SZrCompilerState *cs, const TZrChar *msg, SZrFileRange location) {
    if (cs == ZR_NULL) {
        return;
    }

    cs->hasError = ZR_TRUE;
    cs->errorMessage = msg;
    cs->errorLocation = location;

    // 输出详细的错误信息（包含行列号）
    const TZrChar *sourceName = "unknown";
    TZrSize nameLen = 7; // "unknown" 的长度
    if (location.source != ZR_NULL) {
        if (location.source->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            sourceName = ZrCore_String_GetNativeStringShort(location.source);
            nameLen = location.source->shortStringLength;
        } else {
            sourceName = ZrCore_String_GetNativeString(location.source);
            nameLen = location.source->longStringLength;
        }
    }

    // 输出格式化的错误信息
    fprintf(stderr, "\n");
    fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "Compiler Error\n");
    fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "  Error Message: %s\n", msg);
    fprintf(stderr, "  Location: %.*s:%d:%d - %d:%d\n", 
           (int) nameLen, sourceName, 
           location.start.line, location.start.column,
           location.end.line, location.end.column);
    
    // 输出错误原因分析
    fprintf(stderr, "\n  Error Analysis:\n");
    if (strstr(msg, "INTERFACE_METHOD_SIGNATURE") != ZR_NULL ||
        strstr(msg, "INTERFACE_FIELD_DECLARATION") != ZR_NULL ||
        strstr(msg, "INTERFACE_PROPERTY_SIGNATURE") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Interface declaration member found in invalid context\n");
        fprintf(stderr, "    - Root Cause: Interface members (methods, fields, properties) can only appear\n");
        fprintf(stderr, "                  inside interface declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "STRUCT_FIELD") != ZR_NULL ||
               strstr(msg, "STRUCT_METHOD") != ZR_NULL ||
               strstr(msg, "CLASS_FIELD") != ZR_NULL ||
               strstr(msg, "CLASS_METHOD") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Struct/Class member found in invalid context\n");
        fprintf(stderr, "    - Root Cause: Struct/Class members can only appear inside struct/class\n");
        fprintf(stderr, "                  declaration bodies, not in statements or expressions\n");
    } else if (strstr(msg, "Unexpected expression type") != ZR_NULL) {
        fprintf(stderr, "    - Problem: AST node type not supported in expression context\n");
        fprintf(stderr, "    - Root Cause: The compiler encountered a node type that cannot be compiled\n");
        fprintf(stderr, "                  as an expression. This may indicate a parsing error or missing\n");
        fprintf(stderr, "                  implementation for this node type.\n");
    } else if (strstr(msg, "Type mismatch") != ZR_NULL ||
               strstr(msg, "Incompatible types") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Type compatibility check failed\n");
        fprintf(stderr, "    - Root Cause: The types of operands, variables, or function arguments are\n");
        fprintf(stderr, "                  not compatible with the operation or assignment being performed\n");
    } else if (strstr(msg, "not found") != ZR_NULL) {
        fprintf(stderr, "    - Problem: Identifier not found in current scope\n");
        fprintf(stderr, "    - Root Cause: The variable, function, or type name was not found in the\n");
        fprintf(stderr, "                  current scope or type environment\n");
    } else {
        fprintf(stderr, "    - Problem: Compilation error occurred\n");
        fprintf(stderr, "    - Root Cause: See error message above for details\n");
    }
    
    // 输出解决建议
    fprintf(stderr, "\n  How to Fix:\n");
    print_error_suggestion(msg);
    
    fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
    fprintf(stderr, "\n");
}

// 创建指令（辅助函数）
TZrInstruction create_instruction_0(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = 0;
    return instruction;
}

TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operand) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

TZrInstruction create_instruction_2(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operand1,
                                    TZrUInt16 operand2) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

static TZrInstruction create_instruction_4(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt8 op0, TZrUInt8 op1,
                                           TZrUInt8 op2, TZrUInt8 op3) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TZrUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand0[0] = op0;
    instruction.instruction.operand.operand0[1] = op1;
    instruction.instruction.operand.operand0[2] = op2;
    instruction.instruction.operand.operand0[3] = op3;
    return instruction;
}

static TZrBool compiler_copy_range_to_raw(SZrCompilerState *cs,
                                          TZrPtr *outMemory,
                                          const TZrPtr source,
                                          TZrSize count,
                                          TZrSize elementSize) {
    TZrSize bytes;

    if (outMemory == ZR_NULL) {
        return ZR_FALSE;
    }

    *outMemory = ZR_NULL;
    if (cs == ZR_NULL || cs->state == ZR_NULL || count == 0 || source == ZR_NULL || elementSize == 0) {
        return ZR_TRUE;
    }

    bytes = count * elementSize;
    *outMemory = ZrCore_Memory_RawMallocWithType(cs->state->global, bytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (*outMemory == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(*outMemory, source, bytes);
    return ZR_TRUE;
}

static TZrBool compiler_copy_function_exception_metadata_slice(SZrCompilerState *cs,
                                                               SZrFunction *function,
                                                               TZrSize executionStart,
                                                               TZrSize catchStart,
                                                               TZrSize handlerStart,
                                                               SZrAstNode *sourceNode) {
    TZrSize executionCount;
    TZrSize catchCount;
    TZrSize handlerCount;

    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    executionCount = (cs->executionLocations.length > executionStart)
                             ? (cs->executionLocations.length - executionStart)
                             : 0;
    catchCount = (cs->catchClauseInfos.length > catchStart)
                         ? (cs->catchClauseInfos.length - catchStart)
                         : 0;
    handlerCount = (cs->exceptionHandlerInfos.length > handlerStart)
                           ? (cs->exceptionHandlerInfos.length - handlerStart)
                           : 0;

    function->executionLocationInfoList = ZR_NULL;
    function->executionLocationInfoLength = 0;
    function->catchClauseList = ZR_NULL;
    function->catchClauseCount = 0;
    function->exceptionHandlerList = ZR_NULL;
    function->exceptionHandlerCount = 0;
    function->sourceCodeList = (sourceNode != ZR_NULL) ? sourceNode->location.source : ZR_NULL;

    if (executionCount > 0) {
        SZrFunctionExecutionLocationInfo *src =
                (SZrFunctionExecutionLocationInfo *)ZrCore_Array_Get(&cs->executionLocations, executionStart);
        TZrPtr copied = ZR_NULL;
        if (!compiler_copy_range_to_raw(cs,
                                        &copied,
                                        src,
                                        executionCount,
                                        sizeof(SZrFunctionExecutionLocationInfo))) {
            return ZR_FALSE;
        }
        function->executionLocationInfoList = (SZrFunctionExecutionLocationInfo *)copied;
        function->executionLocationInfoLength = (TZrUInt32)executionCount;
    }

    if (catchCount > 0) {
        function->catchClauseList = (SZrFunctionCatchClauseInfo *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                catchCount * sizeof(SZrFunctionCatchClauseInfo),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->catchClauseList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < catchCount; index++) {
            SZrCompilerCatchClauseInfo *src =
                    (SZrCompilerCatchClauseInfo *)ZrCore_Array_Get(&cs->catchClauseInfos, catchStart + index);
            SZrFunctionCatchClauseInfo *dst = &function->catchClauseList[index];
            SZrLabel *targetLabel = (src != ZR_NULL && src->targetLabelId < cs->labels.length)
                                            ? (SZrLabel *)ZrCore_Array_Get(&cs->labels, src->targetLabelId)
                                            : ZR_NULL;

            dst->typeName = (src != ZR_NULL) ? src->typeName : ZR_NULL;
            dst->targetInstructionOffset =
                    (targetLabel != ZR_NULL) ? (TZrMemoryOffset)targetLabel->instructionIndex : 0;
        }
        function->catchClauseCount = (TZrUInt32)catchCount;
    }

    if (handlerCount > 0) {
        function->exceptionHandlerList = (SZrFunctionExceptionHandlerInfo *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                handlerCount * sizeof(SZrFunctionExceptionHandlerInfo),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exceptionHandlerList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < handlerCount; index++) {
            SZrCompilerExceptionHandlerInfo *src =
                    (SZrCompilerExceptionHandlerInfo *)ZrCore_Array_Get(&cs->exceptionHandlerInfos,
                                                                        handlerStart + index);
            SZrFunctionExceptionHandlerInfo *dst = &function->exceptionHandlerList[index];
            SZrLabel *finallyLabel = (src != ZR_NULL && src->finallyLabelId < cs->labels.length)
                                             ? (SZrLabel *)ZrCore_Array_Get(&cs->labels, src->finallyLabelId)
                                             : ZR_NULL;
            SZrLabel *afterFinallyLabel = (src != ZR_NULL && src->afterFinallyLabelId < cs->labels.length)
                                                  ? (SZrLabel *)ZrCore_Array_Get(&cs->labels, src->afterFinallyLabelId)
                                                  : ZR_NULL;

            if (src == ZR_NULL) {
                memset(dst, 0, sizeof(*dst));
                continue;
            }

            dst->protectedStartInstructionOffset = src->protectedStartInstructionOffset;
            dst->finallyTargetInstructionOffset =
                    (finallyLabel != ZR_NULL) ? (TZrMemoryOffset)finallyLabel->instructionIndex : 0;
            dst->afterFinallyInstructionOffset =
                    (afterFinallyLabel != ZR_NULL) ? (TZrMemoryOffset)afterFinallyLabel->instructionIndex : 0;
            dst->catchClauseStartIndex = (TZrUInt32)(src->catchClauseStartIndex - catchStart);
            dst->catchClauseCount = src->catchClauseCount;
            dst->hasFinally = src->hasFinally;
        }
        function->exceptionHandlerCount = (TZrUInt32)handlerCount;
    }

    return ZR_TRUE;
}

// 添加指令到当前函数
void emit_instruction(SZrCompilerState *cs, TZrInstruction instruction) {
    SZrFunctionExecutionLocationInfo locationInfo;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    ZrCore_Array_Push(cs->state, &cs->instructions, &instruction);
    // instructionCount 应该与 instructions.length 保持同步
    cs->instructionCount = cs->instructions.length;

    locationInfo.currentInstructionOffset = (TZrMemoryOffset)(cs->instructionCount - 1);
    locationInfo.lineInSource = (cs->currentAst != ZR_NULL && cs->currentAst->location.start.line > 0)
                                        ? (TZrUInt32)cs->currentAst->location.start.line
                                        : 0;
    ZrCore_Array_Push(cs->state, &cs->executionLocations, &locationInfo);
}

// 添加常量到常量池
TZrUInt32 add_constant(SZrCompilerState *cs, SZrTypeValue *value) {
    if (cs == ZR_NULL || cs->hasError || value == ZR_NULL) {
        return 0;
    }

    if (cs->constantCount != cs->constants.length) {
        cs->constantCount = cs->constants.length;
    }

    // 检查常量是否已存在（常量去重）
    // 遍历已有的常量，查找相同的值
    for (TZrSize i = 0; i < cs->constants.length; i++) {
        SZrTypeValue *existingValue = (SZrTypeValue *)ZrCore_Array_Get(&cs->constants, i);
        if (existingValue != ZR_NULL) {
            // 使用 ZrCore_Value_Equal 比较常量值
            if (ZrCore_Value_Equal(cs->state, existingValue, value)) {
                // 找到相同的常量，返回已有常量的索引
                return (TZrUInt32)i;
            }
        }
    }

    // 常量不存在，添加新常量
    ZrCore_Array_Push(cs->state, &cs->constants, value);
    TZrUInt32 index = (TZrUInt32)(cs->constants.length - 1);
    cs->constantCount = cs->constants.length;
    return index;
}

#define ZR_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH 64

static TZrBool compiler_value_is_compile_time_function_pointer(SZrCompilerState *cs, const SZrTypeValue *value) {
    TZrPtr pointerValue;

    if (cs == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_FALSE;
    }

    pointerValue = value->value.nativeObject.nativePointer;
    if (pointerValue == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (TZrPtr)(*funcPtr) == pointerValue) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_is_runtime_safe_compile_time_value_internal(SZrCompilerState *cs,
                                                                  const SZrTypeValue *value,
                                                                  SZrRawObject **visitedObjects,
                                                                  TZrSize visitedCount,
                                                                  TZrUInt32 depth) {
    if (cs == ZR_NULL || value == ZR_NULL || visitedObjects == ZR_NULL) {
        return ZR_FALSE;
    }

    if (depth > ZR_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return ZR_FALSE;
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY: {
            SZrRawObject *rawObject = ZrCore_Value_GetRawObject(value);
            SZrObject *object;

            if (rawObject == ZR_NULL) {
                return ZR_TRUE;
            }

            for (TZrSize i = 0; i < visitedCount; i++) {
                if (visitedObjects[i] == rawObject) {
                    return ZR_TRUE;
                }
            }

            if (visitedCount >= ZR_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH) {
                return ZR_FALSE;
            }

            object = ZR_CAST_OBJECT(cs->state, rawObject);
            if (object == ZR_NULL || !object->nodeMap.isValid || object->nodeMap.buckets == ZR_NULL) {
                return ZR_TRUE;
            }

            visitedObjects[visitedCount] = rawObject;
            for (TZrSize bucketIndex = 0; bucketIndex < object->nodeMap.capacity; bucketIndex++) {
                SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
                while (pair != ZR_NULL) {
                    if (!compiler_is_runtime_safe_compile_time_value_internal(cs, &pair->key, visitedObjects,
                                                                              visitedCount + 1, depth + 1) ||
                        !compiler_is_runtime_safe_compile_time_value_internal(cs, &pair->value, visitedObjects,
                                                                              visitedCount + 1, depth + 1)) {
                        return ZR_FALSE;
                    }
                    pair = pair->next;
                }
            }
            return ZR_TRUE;
        }
        default:
            return ZR_TRUE;
    }
}

ZR_PARSER_API TZrBool ZrParser_Compiler_ValidateRuntimeProjectionValue(SZrCompilerState *cs,
                                                             const SZrTypeValue *value,
                                                             SZrFileRange location) {
    SZrRawObject *visitedObjects[ZR_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH];
    const TZrChar *message;

    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < ZR_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH; i++) {
        visitedObjects[i] = ZR_NULL;
    }

    if (compiler_is_runtime_safe_compile_time_value_internal(cs, value, visitedObjects, 0, 0)) {
        return ZR_TRUE;
    }

    cs->hasCompileTimeError = ZR_TRUE;
    cs->hasFatalError = ZR_TRUE;
    message = compiler_value_is_compile_time_function_pointer(cs, value)
                      ? "Compile-time value cannot be projected to runtime because it is a compile-time-only function reference"
                      : "Compile-time value cannot be projected to runtime because it contains native pointer values such as compile-time-only function references";
    ZrParser_Compiler_Error(cs, message, location);
    return ZR_FALSE;
}

// 分配局部变量槽位
TZrUInt32 allocate_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || cs->hasError || name == ZR_NULL) {
        return 0;
    }

    TZrUInt32 stackSlot = (TZrUInt32)cs->stackSlotCount;
    SZrFunctionLocalVariable localVar;
    localVar.name = name;
    localVar.stackSlot = stackSlot;
    localVar.offsetActivate = (TZrMemoryOffset) cs->instructionCount;
    localVar.offsetDead = 0; // 将在变量作用域结束时设置

    ZrCore_Array_Push(cs->state, &cs->localVars, &localVar);
    // localVarCount 应该与 localVars.length 保持同步
    cs->localVarCount = cs->localVars.length;
    cs->stackSlotCount++;
    if (cs->maxStackSlotCount < cs->stackSlotCount) {
        cs->maxStackSlotCount = cs->stackSlotCount;
    }

    if (cs->scopeStack.length > 0) {
        SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (scope != ZR_NULL) {
            scope->varCount++;
        }
    }

    return stackSlot;
}

TZrSize ZrParser_Compiler_GetLocalStackFloor(const SZrCompilerState *cs) {
    TZrSize floor = 0;

    if (cs == ZR_NULL || !cs->localVars.isValid) {
        return 0;
    }

    for (TZrSize i = 0; i < cs->localVars.length; i++) {
        const SZrFunctionLocalVariable *var =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get((SZrArray *)&cs->localVars, i);
        if (var != ZR_NULL) {
            TZrSize nextSlot = (TZrSize)var->stackSlot + 1;
            if (nextSlot > floor) {
                floor = nextSlot;
            }
        }
    }

    return floor;
}

static TZrUInt32 compiler_get_cached_null_constant_index(SZrCompilerState *cs) {
    SZrTypeValue nullValue;

    if (cs == ZR_NULL) {
        return 0;
    }

    if (cs->hasCachedNullConstantIndex) {
        return cs->cachedNullConstantIndex;
    }

    ZrCore_Value_ResetAsNull(&nullValue);
    cs->cachedNullConstantIndex = add_constant(cs, &nullValue);
    cs->hasCachedNullConstantIndex = ZR_TRUE;
    return cs->cachedNullConstantIndex;
}

void ZrParser_Compiler_TrimStackToCount(SZrCompilerState *cs, TZrSize targetCount) {
    TZrSize localFloor;

    if (cs == ZR_NULL) {
        return;
    }

    localFloor = ZrParser_Compiler_GetLocalStackFloor(cs);
    if (targetCount < localFloor) {
        targetCount = localFloor;
    }

    if (cs->stackSlotCount > targetCount) {
        TZrUInt32 nullConstantIndex = compiler_get_cached_null_constant_index(cs);
        for (TZrSize slot = targetCount; slot < cs->stackSlotCount; slot++) {
            emit_instruction(cs,
                             create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                  (TZrUInt16)slot,
                                                  (TZrInt32)nullConstantIndex));
        }
        cs->stackSlotCount = targetCount;
    }
}

void ZrParser_Compiler_TrimStackToSlot(SZrCompilerState *cs, TZrUInt32 slot) {
    ZrParser_Compiler_TrimStackToCount(cs, (TZrSize)slot + 1);
}

void ZrParser_Compiler_TrimStackBy(SZrCompilerState *cs, TZrSize amount) {
    TZrSize targetCount;

    if (cs == ZR_NULL) {
        return;
    }

    targetCount = (amount < cs->stackSlotCount) ? (cs->stackSlotCount - amount) : 0;
    ZrParser_Compiler_TrimStackToCount(cs, targetCount);
}

// 查找局部变量
TZrUInt32 find_local_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TZrUInt32) -1;
    }

    // 从当前作用域开始查找
    // 使用 localVars.length 而不是 localVarCount，确保同步
    TZrSize varCount = cs->localVars.length;
    for (TZrSize i = varCount; i > 0; i--) {
        TZrSize index = i - 1;
        // 确保索引在有效范围内
        if (index < cs->localVars.length) {
            SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *) ZrCore_Array_Get(&cs->localVars, index);
            if (var != ZR_NULL && var->name != ZR_NULL) {
                // 比较字符串
                if (ZrCore_String_Equal(var->name, name)) {
                    return var->stackSlot;
                }
            }
        }
    }

    return (TZrUInt32) -1;
}

// 查找闭包变量
TZrUInt32 find_closure_var(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TZrUInt32) -1;
    }

    // 在闭包变量数组中查找
    TZrSize closureVarCount = cs->closureVars.length;
    for (TZrSize i = 0; i < closureVarCount; i++) {
        SZrFunctionClosureVariable *var = (SZrFunctionClosureVariable *) ZrCore_Array_Get(&cs->closureVars, i);
        if (var != ZR_NULL && var->name != ZR_NULL) {
            if (ZrCore_String_Equal(var->name, name)) {
                return (TZrUInt32) i;
            }
        }
    }

    return (TZrUInt32) -1;
}

// 分配闭包变量
TZrUInt32 allocate_closure_var(SZrCompilerState *cs, SZrString *name, TZrBool inStack) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return 0;
    }

    // 检查是否已存在
    TZrUInt32 existingIndex = find_closure_var(cs, name);
    if (existingIndex != (TZrUInt32) -1) {
        return existingIndex;
    }

    // 创建新的闭包变量
    SZrFunctionClosureVariable closureVar;
    closureVar.name = name;
    closureVar.inStack = inStack;
    closureVar.index = (TZrUInt32) cs->closureVarCount;
    closureVar.valueType = ZR_VALUE_TYPE_NULL; // 类型将在运行时确定

    ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVar);
    cs->closureVarCount++;
    
    return (TZrUInt32) (cs->closureVarCount - 1);
}

// 查找子函数索引（在当前编译器的 childFunctions 中通过函数名查找）
// 返回子函数在 childFunctions 数组中的索引，如果未找到返回 (TZrUInt32)-1
// 注意：这个函数用于在编译时查找子函数索引
// 通过编译时建立的函数名到索引的映射来查找，不依赖遍历比较函数名
TZrUInt32 find_child_function_index(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return (TZrUInt32) -1;
    }
    
    // 遍历函数名映射数组，查找匹配的函数名
    // 这个映射在编译函数声明时建立，仅用于编译时查找
    // 运行时查找完全基于索引，不依赖函数名
    for (TZrSize i = 0; i < cs->childFunctionNameMap.length; i++) {
        SZrChildFunctionNameMap *map = (SZrChildFunctionNameMap *)ZrCore_Array_Get(&cs->childFunctionNameMap, i);
        if (map != ZR_NULL && map->name != ZR_NULL) {
            if (ZrCore_String_Equal(map->name, name)) {
                // 找到匹配的函数名，返回对应的子函数索引
                return map->childFunctionIndex;
            }
        }
    }
    
    return (TZrUInt32) -1;
}

// 生成函数引用路径常量
// 用于在编译函数调用时，如果是子函数调用，生成引用路径常量
// targetFunction: 目标函数（子函数）
// 返回：常量池索引（存储引用路径常量），失败返回0
// 注意：生成的路径格式为：[ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX, childIndex]
//       如果目标函数在parent中，则：[ZR_CONSTANT_REF_STEP_PARENT, ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX, childIndex]
static TZrUInt32 generate_function_reference_path_constant(SZrCompilerState *cs, TZrUInt32 childFunctionIndex) {
    if (cs == ZR_NULL || childFunctionIndex == (TZrUInt32)-1) {
        return 0;
    }
    
    // 生成引用路径：直接子函数引用
    // 路径格式：[ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX, childIndex]
    TZrUInt32 pathDepth = 2;
    TZrUInt32 *pathSteps = (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, pathDepth * sizeof(TZrUInt32));
    if (pathSteps == ZR_NULL) {
        return 0;
    }
    
    pathSteps[0] = ZR_CONSTANT_REF_STEP_TO_UINT32(ZR_CONSTANT_REF_STEP_CHILD_FUNC_INDEX);
    pathSteps[1] = childFunctionIndex;
    
    // 将路径序列化为字符串类型常量（与prototype类似，使用字符串类型存储二进制数据）
    // 格式：pathDepth (TZrUInt32) + pathSteps (TZrUInt32数组)
    TZrSize serializedSize = sizeof(TZrUInt32) + pathDepth * sizeof(TZrUInt32);
    TZrByte *serializedData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, serializedSize);
    if (serializedData == ZR_NULL) {
        ZrCore_Memory_RawFree(cs->state->global, pathSteps, pathDepth * sizeof(TZrUInt32));
        return 0;
    }
    
    // 写入路径深度和步骤
    *(TZrUInt32 *)serializedData = pathDepth;
    ZrCore_Memory_RawCopy(serializedData + sizeof(TZrUInt32), (TZrByte *)pathSteps, pathDepth * sizeof(TZrUInt32));
    
    // 创建字符串常量存储二进制数据
    SZrString *serializedString = ZrCore_String_Create(cs->state, (TZrNativeString)serializedData, serializedSize);
    if (serializedString == ZR_NULL) {
        ZrCore_Memory_RawFree(cs->state->global, serializedData, serializedSize);
        ZrCore_Memory_RawFree(cs->state->global, pathSteps, pathDepth * sizeof(TZrUInt32));
        return 0;
    }
    
    // 将字符串存储到常量池
    SZrTypeValue serializedValue;
    ZrCore_Value_InitAsRawObject(cs->state, &serializedValue, ZR_CAST_RAW_OBJECT_AS_SUPER(serializedString));
    serializedValue.type = ZR_VALUE_TYPE_STRING;
    
    TZrUInt32 constantIndex = add_constant(cs, &serializedValue);
    
    // 释放临时分配的内存
    ZrCore_Memory_RawFree(cs->state->global, serializedData, serializedSize);
    ZrCore_Memory_RawFree(cs->state->global, pathSteps, pathDepth * sizeof(TZrUInt32));
    
    return constantIndex;
}

// 分配栈槽
TZrUInt32 allocate_stack_slot(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return 0;
    }

    TZrUInt32 slot = (TZrUInt32) cs->stackSlotCount;
    cs->stackSlotCount++;
    if (cs->maxStackSlotCount < cs->stackSlotCount) {
        cs->maxStackSlotCount = cs->stackSlotCount;
    }
    return slot;
}

static SZrString *extract_simple_type_name_from_type_node(SZrAstNode *typeNode) {
    if (typeNode == ZR_NULL || typeNode->type != ZR_AST_TYPE) {
        return ZR_NULL;
    }

    SZrType *type = &typeNode->data.type;
    if (type->name == ZR_NULL || type->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return type->name->data.identifier.name;
}

static TZrBool compiler_type_has_constructor(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
        if (info == ZR_NULL || info->name == ZR_NULL || !ZrCore_String_Equal(info->name, typeName)) {
            continue;
        }

        for (TZrSize memberIndex = 0; memberIndex < info->members.length; memberIndex++) {
            SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, memberIndex);
            if (memberInfo != ZR_NULL && memberInfo->isMetaMethod &&
                memberInfo->metaType == ZR_META_CONSTRUCTOR) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }

    return ZR_FALSE;
}

static void emit_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    SZrTypeValue constantValue = *value;
    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)slot,
                                               (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
}

static void emit_string_constant_to_slot(SZrCompilerState *cs, TZrUInt32 slot, SZrString *value) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    SZrTypeValue constantValue;
    ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    constantValue.type = ZR_VALUE_TYPE_STRING;
    emit_constant_to_slot(cs, slot, &constantValue);
}

static void compiler_register_function_type_binding(SZrCompilerState *cs, SZrFunctionDeclaration *funcDecl) {
    if (cs == ZR_NULL || funcDecl == ZR_NULL || cs->typeEnv == ZR_NULL ||
        funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL || cs->hasError) {
        return;
    }

    if (funcDecl->returnType != ZR_NULL) {
        SZrInferredType returnType;
        if (ZrParser_AstTypeToInferredType_Convert(cs, funcDecl->returnType, &returnType)) {
            SZrArray paramTypes;
            ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), 8);
            if (funcDecl->params != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    SZrAstNode *paramNode = funcDecl->params->nodes[i];
                    if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                        SZrParameter *param = &paramNode->data.parameter;
                        if (param->typeInfo != ZR_NULL) {
                            SZrInferredType paramType;
                            if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                            }
                        } else {
                            SZrInferredType paramType;
                            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                            ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
                        }
                    }
                }
            }
            ZrParser_TypeEnvironment_RegisterFunction(cs->state, cs->typeEnv, funcDecl->name->name, &returnType, &paramTypes);
            ZrParser_InferredType_Free(cs->state, &returnType);
            for (TZrSize i = 0; i < paramTypes.length; i++) {
                SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
                if (paramType != ZR_NULL) {
                    ZrParser_InferredType_Free(cs->state, paramType);
                }
            }
            ZrCore_Array_Free(cs->state, &paramTypes);
        }
    } else {
        SZrInferredType returnType;
        SZrArray paramTypes;

        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), 8);
        ZrParser_TypeEnvironment_RegisterFunction(cs->state, cs->typeEnv, funcDecl->name->name, &returnType, &paramTypes);
        ZrParser_InferredType_Free(cs->state, &returnType);
        ZrCore_Array_Free(cs->state, &paramTypes);
    }
}

static void compiler_register_named_value_binding_to_env(SZrCompilerState *cs,
                                                         SZrTypeEnvironment *env,
                                                         SZrString *name,
                                                         SZrString *typeName) {
    SZrInferredType existingType;
    SZrInferredType inferredType;

    if (cs == ZR_NULL || env == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &existingType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_TypeEnvironment_LookupVariable(cs->state, env, name, &existingType)) {
        ZrParser_InferredType_Free(cs->state, &existingType);
        return;
    }
    ZrParser_InferredType_Free(cs->state, &existingType);

    if (typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    } else {
        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    }
    ZrParser_TypeEnvironment_RegisterVariable(cs->state, env, name, &inferredType);
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

static void compiler_register_extern_function_type_binding_to_env(SZrCompilerState *cs,
                                                                  SZrTypeEnvironment *env,
                                                                  SZrExternFunctionDeclaration *functionDecl) {
    SZrInferredType returnType;
    SZrArray paramTypes;

    if (cs == ZR_NULL || env == ZR_NULL || functionDecl == ZR_NULL ||
        functionDecl->name == ZR_NULL || functionDecl->name->name == ZR_NULL) {
        return;
    }

    if (functionDecl->returnType != ZR_NULL) {
        if (!ZrParser_AstTypeToInferredType_Convert(cs, functionDecl->returnType, &returnType)) {
            return;
        }
    } else {
        ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_NULL);
    }

    ZrCore_Array_Init(cs->state, &paramTypes, sizeof(SZrInferredType), functionDecl->params != ZR_NULL
                                                                         ? functionDecl->params->count
                                                                         : 0);
    if (functionDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < functionDecl->params->count; i++) {
            SZrAstNode *paramNode = functionDecl->params->nodes[i];
            SZrInferredType paramType;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.typeInfo != ZR_NULL) {
                if (!ZrParser_AstTypeToInferredType_Convert(cs, paramNode->data.parameter.typeInfo, &paramType)) {
                    continue;
                }
            } else {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }
            ZrCore_Array_Push(cs->state, &paramTypes, &paramType);
        }
    }

    ZrParser_TypeEnvironment_RegisterFunction(cs->state, env, functionDecl->name->name, &returnType, &paramTypes);

    ZrParser_InferredType_Free(cs->state, &returnType);
    for (TZrSize i = 0; i < paramTypes.length; i++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&paramTypes, i);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, paramType);
        }
    }
    ZrCore_Array_Free(cs->state, &paramTypes);
}

static TZrUInt32 find_local_var_in_current_scope(SZrCompilerState *cs, SZrString *name) {
    SZrScope *scope;
    TZrSize startIndex;

    if (cs == ZR_NULL || name == ZR_NULL || cs->scopeStack.length == 0) {
        return (TZrUInt32)-1;
    }

    scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
    startIndex = scope != ZR_NULL ? scope->startVarIndex : 0;
    if (startIndex > cs->localVars.length) {
        startIndex = cs->localVars.length;
    }

    for (TZrSize i = cs->localVars.length; i > startIndex; i--) {
        SZrFunctionLocalVariable *var =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, i - 1);
        if (var != ZR_NULL && var->name != ZR_NULL && ZrCore_String_Equal(var->name, name)) {
            return var->stackSlot;
        }
    }

    return (TZrUInt32)-1;
}

void ZrParser_Compiler_PredeclareFunctionBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError) {
        return;
    }

    for (TZrSize i = 0; i < statements->count; i++) {
        SZrAstNode *stmt = statements->nodes[i];
        SZrFunctionDeclaration *funcDecl;
        TZrUInt32 slot;
        SZrTypeValue nullValue;

        if (stmt == ZR_NULL || stmt->type != ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }

        funcDecl = &stmt->data.functionDeclaration;
        if (funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL) {
            continue;
        }

        compiler_register_function_type_binding(cs, funcDecl);
        if (cs->hasError) {
            return;
        }

        if (find_local_var_in_current_scope(cs, funcDecl->name->name) != (TZrUInt32)-1) {
            continue;
        }

        slot = allocate_local_var(cs, funcDecl->name->name);
        ZrCore_Value_ResetAsNull(&nullValue);
        emit_constant_to_slot(cs, slot, &nullValue);
        if (cs->hasError) {
            return;
        }
    }
}

static TZrUInt32 emit_load_global_identifier(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    TZrUInt32 globalSlot = allocate_stack_slot(cs);
    TZrInstruction getGlobalInst = create_instruction_0(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)globalSlot);
    emit_instruction(cs, getGlobalInst);

    TZrUInt32 keySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, keySlot, name);

    TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)globalSlot,
                                                       (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
    emit_instruction(cs, getTableInst);
    ZrParser_Compiler_TrimStackToSlot(cs, globalSlot);
    return globalSlot;
}

static TZrInt64 compiler_internal_import_native_entry(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *result;
    SZrTypeValue *pathValue;
    SZrString *path;
    SZrObjectModule *module;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    argBase = functionBase + 1;
    result = ZrCore_Stack_GetValue(functionBase);

    if (state->stackTop.valuePointer <= argBase) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        result = ZrCore_Stack_GetValue(functionBase);
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    pathValue = ZrCore_Stack_GetValue(argBase);
    if (pathValue == ZR_NULL || pathValue->type != ZR_VALUE_TYPE_STRING) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        result = ZrCore_Stack_GetValue(functionBase);
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    path = ZR_CAST_STRING(state, pathValue->value.object);
    module = ZrCore_Module_ImportByPath(state, path);

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    result = ZrCore_Stack_GetValue(functionBase);
    if (module == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    } else {
        ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
        result->type = ZR_VALUE_TYPE_OBJECT;
    }

    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

ZR_PARSER_API TZrUInt32 ZrParser_Compiler_EmitImportModuleExpression(SZrCompilerState *cs,
                                                                     SZrString *moduleName,
                                                                     SZrFileRange location) {
    SZrClosureNative *importClosure;
    SZrTypeValue importCallable;
    TZrUInt32 functionSlot;
    TZrUInt32 argumentSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return (TZrUInt32)-1;
    }

    importClosure = ZrCore_ClosureNative_New(cs->state, 0);
    if (importClosure == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to create internal import callable", location);
        return (TZrUInt32)-1;
    }
    importClosure->nativeFunction = compiler_internal_import_native_entry;
    ZrCore_RawObject_MarkAsPermanent(cs->state, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));

    ZrCore_Value_InitAsRawObject(cs->state, &importCallable, ZR_CAST_RAW_OBJECT_AS_SUPER(importClosure));
    importCallable.isNative = ZR_TRUE;

    functionSlot = allocate_stack_slot(cs);
    emit_constant_to_slot(cs, functionSlot, &importCallable);

    argumentSlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, argumentSlot, moduleName);

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          1));
    ZrParser_Compiler_TrimStackToSlot(cs, functionSlot);
    return functionSlot;
}

static void emit_object_field_assignment_from_expression(SZrCompilerState *cs,
                                                         TZrUInt32 objectSlot,
                                                         SZrString *fieldName,
                                                         SZrAstNode *expression) {
    if (cs == ZR_NULL || fieldName == ZR_NULL || expression == ZR_NULL || cs->hasError) {
        return;
    }

    ZrParser_Expression_Compile(cs, expression);
    if (cs->hasError || cs->stackSlotCount == 0) {
        return;
    }

    TZrUInt32 valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    TZrUInt32 keySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, keySlot, fieldName);

    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)valueSlot,
                                                       (TZrUInt16)objectSlot, (TZrUInt16)keySlot);
    emit_instruction(cs, setTableInst);
}

static void emit_class_static_field_initializers(SZrCompilerState *cs, SZrAstNode *classNode) {
    if (cs == ZR_NULL || classNode == ZR_NULL || classNode->type != ZR_AST_CLASS_DECLARATION || cs->hasError) {
        return;
    }

    SZrClassDeclaration *classDecl = &classNode->data.classDeclaration;
    if (classDecl->name == ZR_NULL || classDecl->name->name == ZR_NULL || classDecl->members == ZR_NULL) {
        return;
    }

    TZrUInt32 prototypeSlot = (TZrUInt32)-1;
    for (TZrSize i = 0; i < classDecl->members->count; i++) {
        SZrAstNode *member = classDecl->members->nodes[i];
        if (member == ZR_NULL || member->type != ZR_AST_CLASS_FIELD) {
            continue;
        }

        SZrClassField *field = &member->data.classField;
        if (!field->isStatic || field->init == ZR_NULL || field->name == ZR_NULL || field->name->name == ZR_NULL) {
            continue;
        }

        if (prototypeSlot == (TZrUInt32)-1) {
            prototypeSlot = emit_load_global_identifier(cs, classDecl->name->name);
            if (prototypeSlot == (TZrUInt32)-1 || cs->hasError) {
                return;
            }
        }

        emit_object_field_assignment_from_expression(cs, prototypeSlot, field->name->name, field->init);
        if (cs->hasError) {
            return;
        }
    }
}

static SZrString *create_hidden_property_accessor_name(SZrCompilerState *cs, SZrString *propertyName,
                                                       TZrBool isSetter) {
    if (cs == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrNativeString propertyNameString = ZrCore_String_GetNativeString(propertyName);
    if (propertyNameString == ZR_NULL) {
        return ZR_NULL;
    }

    TZrSize prefixLength = strlen(prefix);
    TZrSize propertyNameLength = propertyName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                                         ? propertyName->shortStringLength
                                         : propertyName->longStringLength;
    TZrSize bufferSize = prefixLength + propertyNameLength + 1;
    TZrChar *buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, bufferSize);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, prefix, prefixLength);
    memcpy(buffer + prefixLength, propertyNameString, propertyNameLength);
    buffer[bufferSize - 1] = '\0';

    SZrString *result = ZrCore_String_CreateFromNative(cs->state, buffer);
    ZrCore_Memory_RawFree(cs->state->global, buffer, bufferSize);
    return result;
}

static void emit_super_constructor_call(SZrCompilerState *cs, SZrString *superTypeName, SZrAstNodeArray *superArgs) {
    if (cs == ZR_NULL || superTypeName == ZR_NULL || cs->hasError) {
        return;
    }

    TZrUInt32 prototypeSlot = emit_load_global_identifier(cs, superTypeName);
    if (prototypeSlot == (TZrUInt32)-1 || cs->hasError) {
        return;
    }

    SZrString *constructorName = ZrCore_String_CreateFromNative(cs->state, "__constructor");
    if (constructorName == ZR_NULL) {
        SZrFileRange location = cs->currentFunctionNode != ZR_NULL ? cs->currentFunctionNode->location
                                                                   : cs->errorLocation;
        ZrParser_Compiler_Error(cs, "Failed to create super constructor lookup key", location);
        return;
    }

    TZrUInt32 constructorKeySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, constructorKeySlot, constructorName);

    TZrUInt32 functionSlot = allocate_stack_slot(cs);
    TZrInstruction getConstructorInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)functionSlot,
                                                             (TZrUInt16)prototypeSlot,
                                                             (TZrUInt16)constructorKeySlot);
    emit_instruction(cs, getConstructorInst);

    TZrUInt32 receiverSlot = allocate_stack_slot(cs);
    TZrInstruction copyReceiverInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)receiverSlot, 0);
    emit_instruction(cs, copyReceiverInst);

    TZrUInt32 argCount = 1;
    if (superArgs != ZR_NULL) {
        for (TZrSize i = 0; i < superArgs->count; i++) {
            SZrAstNode *argNode = superArgs->nodes[i];
            if (argNode == ZR_NULL) {
                continue;
            }

            TZrUInt32 targetSlot = allocate_stack_slot(cs);
            ZrParser_Expression_Compile(cs, argNode);
            if (cs->hasError || cs->stackSlotCount == 0) {
                return;
            }

            TZrUInt32 valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
            if (valueSlot != targetSlot) {
                TZrInstruction moveArgInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                                  (TZrUInt16)targetSlot, (TZrInt32)valueSlot);
                emit_instruction(cs, moveArgInst);
            }
            argCount++;
        }
    }

    TZrInstruction callSuperInst = create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), (TZrUInt16)functionSlot,
                                                        (TZrUInt16)functionSlot, (TZrUInt16)argCount);
    emit_instruction(cs, callSuperInst);
}

static SZrString *create_hidden_extern_local_name(SZrCompilerState *cs, const TZrChar *prefix) {
    TZrChar buffer[96];
    int length;

    if (cs == ZR_NULL || cs->state == ZR_NULL || prefix == ZR_NULL) {
        return ZR_NULL;
    }

    length = snprintf(buffer,
                      sizeof(buffer),
                      "__zr_extern_%s_%u_%u",
                      prefix,
                      (unsigned)cs->scopeStack.length,
                      (unsigned)cs->localVars.length);
    if (length < 0) {
        return ZR_NULL;
    }

    if ((size_t)length >= sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
        buffer[length] = '\0';
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)length);
}

static TZrBool extern_compiler_string_equals(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeValue = ZrCore_String_GetNativeString(value);
    return nativeValue != ZR_NULL && strcmp(nativeValue, literal) == 0;
}

static TZrBool extern_compiler_identifier_equals(SZrAstNode *node, const TZrChar *literal) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    return extern_compiler_string_equals(node->data.identifier.name, literal);
}

static TZrBool extern_compiler_make_string_value(SZrCompilerState *cs, const TZrChar *text, SZrTypeValue *outValue) {
    SZrString *stringObject;

    if (cs == ZR_NULL || text == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    stringObject = ZrCore_String_CreateFromNative(cs->state, text);
    if (stringObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    outValue->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

typedef struct ZrExternCompilerTempRoot {
    SZrState *state;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor slotAnchor;
    TZrBool active;
} ZrExternCompilerTempRoot;

static TZrBool extern_compiler_temp_root_begin(SZrCompilerState *cs, ZrExternCompilerTempRoot *root) {
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer slot;

    if (cs == ZR_NULL || cs->state == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(root, 0, sizeof(*root));
    root->state = cs->state;
    savedStackTop = cs->state->stackTop.valuePointer;
    if (savedStackTop == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(cs->state, savedStackTop, &root->savedStackTopAnchor);
    slot = ZrCore_Function_CheckStackAndAnchor(cs->state, 1, savedStackTop, savedStackTop, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    slot = ZrCore_Function_StackAnchorRestore(cs->state, &root->slotAnchor);
    if (slot == ZR_NULL) {
        memset(root, 0, sizeof(*root));
        return ZR_FALSE;
    }

    cs->state->stackTop.valuePointer = slot + 1;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(slot));
    root->active = ZR_TRUE;
    return ZR_TRUE;
}

static SZrTypeValue *extern_compiler_temp_root_value(ZrExternCompilerTempRoot *root) {
    TZrStackValuePointer slot;

    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return ZR_NULL;
    }

    slot = ZrCore_Function_StackAnchorRestore(root->state, &root->slotAnchor);
    return slot != ZR_NULL ? ZrCore_Stack_GetValue(slot) : ZR_NULL;
}

static TZrBool extern_compiler_temp_root_set_value(ZrExternCompilerTempRoot *root, const SZrTypeValue *value) {
    SZrTypeValue *slotValue = extern_compiler_temp_root_value(root);
    if (slotValue == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    *slotValue = *value;
    return ZR_TRUE;
}

static TZrBool extern_compiler_temp_root_set_object(ZrExternCompilerTempRoot *root,
                                                    SZrObject *object,
                                                    EZrValueType type) {
    SZrTypeValue *slotValue = extern_compiler_temp_root_value(root);
    if (slotValue == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(root->state, slotValue, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    slotValue->type = type;
    return ZR_TRUE;
}

static void extern_compiler_temp_root_end(ZrExternCompilerTempRoot *root) {
    if (root == ZR_NULL || !root->active || root->state == ZR_NULL) {
        return;
    }

    root->state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(root->state, &root->savedStackTopAnchor);
    memset(root, 0, sizeof(*root));
}

static TZrBool extern_compiler_set_object_field(SZrCompilerState *cs,
                                                SZrObject *object,
                                                const TZrChar *fieldName,
                                                const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;
    ZrExternCompilerTempRoot objectRoot;
    ZrExternCompilerTempRoot valueRoot;
    TZrBool objectRootActive = ZR_FALSE;
    TZrBool valueRootActive = ZR_FALSE;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (extern_compiler_temp_root_begin(cs, &objectRoot)) {
        objectRootActive = extern_compiler_temp_root_set_object(&objectRoot, object, ZR_VALUE_TYPE_OBJECT);
    }
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY || value->type == ZR_VALUE_TYPE_STRING) &&
        value->value.object != ZR_NULL &&
        extern_compiler_temp_root_begin(cs, &valueRoot)) {
        valueRootActive = extern_compiler_temp_root_set_value(&valueRoot, value);
    }

    fieldString = ZrCore_String_CreateFromNative(cs->state, fieldName);
    if (fieldString == ZR_NULL) {
        if (valueRootActive) {
            extern_compiler_temp_root_end(&valueRoot);
        }
        if (objectRootActive) {
            extern_compiler_temp_root_end(&objectRoot);
        }
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    if (valueRootActive) {
        extern_compiler_temp_root_end(&valueRoot);
    }
    if (objectRootActive) {
        extern_compiler_temp_root_end(&objectRoot);
    }
    return ZR_TRUE;
}

static TZrBool extern_compiler_push_array_value(SZrCompilerState *cs, SZrObject *array, const SZrTypeValue *value) {
    SZrTypeValue key;
    ZrExternCompilerTempRoot arrayRoot;
    ZrExternCompilerTempRoot valueRoot;
    TZrBool arrayRootActive = ZR_FALSE;
    TZrBool valueRootActive = ZR_FALSE;

    if (cs == ZR_NULL || array == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (extern_compiler_temp_root_begin(cs, &arrayRoot)) {
        arrayRootActive = extern_compiler_temp_root_set_object(&arrayRoot, array, ZR_VALUE_TYPE_ARRAY);
    }
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY || value->type == ZR_VALUE_TYPE_STRING) &&
        value->value.object != ZR_NULL &&
        extern_compiler_temp_root_begin(cs, &valueRoot)) {
        valueRootActive = extern_compiler_temp_root_set_value(&valueRoot, value);
    }

    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)array->nodeMap.elementCount);
    ZrCore_Object_SetValue(cs->state, array, &key, value);
    if (valueRootActive) {
        extern_compiler_temp_root_end(&valueRoot);
    }
    if (arrayRootActive) {
        extern_compiler_temp_root_end(&arrayRoot);
    }
    return ZR_TRUE;
}

static SZrObject *extern_compiler_new_object_constant(SZrCompilerState *cs) {
    SZrObject *object;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(cs->state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(cs->state, object);
    }
    return object;
}

static SZrObject *extern_compiler_new_array_constant(SZrCompilerState *cs) {
    SZrObject *array;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(cs->state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(cs->state, array);
    }
    return array;
}

static TZrBool extern_compiler_match_decorator_path(SZrAstNode *decoratorNode,
                                                    const TZrChar *leafName,
                                                    TZrBool requireCall,
                                                    SZrFunctionCall **outCall) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary;
    SZrAstNode *ffiMember;
    SZrAstNode *leafMember;
    SZrAstNode *callMember = ZR_NULL;

    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION || leafName == ZR_NULL) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL || expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (!extern_compiler_identifier_equals(primary->property, "zr") ||
        primary->members == ZR_NULL ||
        primary->members->count < (requireCall ? 3 : 2)) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (ffiMember == ZR_NULL || ffiMember->type != ZR_AST_MEMBER_EXPRESSION ||
        leafMember == ZR_NULL || leafMember->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_FALSE;
    }

    if (!extern_compiler_identifier_equals(ffiMember->data.memberExpression.property, "ffi") ||
        !extern_compiler_identifier_equals(leafMember->data.memberExpression.property, leafName)) {
        return ZR_FALSE;
    }

    if (requireCall) {
        callMember = primary->members->nodes[2];
        if (callMember == ZR_NULL || callMember->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
        if (outCall != ZR_NULL) {
            *outCall = &callMember->data.functionCall;
        }
        return primary->members->count == 3;
    }

    return primary->members->count == 2;
}

static SZrAstNode *extern_compiler_decorators_find_call(SZrAstNodeArray *decorators,
                                                        const TZrChar *leafName,
                                                        SZrFunctionCall **outCall) {
    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (decorators == ZR_NULL || leafName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decorator = decorators->nodes[index];
        if (extern_compiler_match_decorator_path(decorator, leafName, ZR_TRUE, &call)) {
            if (outCall != ZR_NULL) {
                *outCall = call;
            }
            return decorator;
        }
    }

    return ZR_NULL;
}

static TZrBool extern_compiler_decorators_has_flag(SZrAstNodeArray *decorators, const TZrChar *leafName) {
    if (decorators == ZR_NULL || leafName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        if (extern_compiler_match_decorator_path(decorators->nodes[index], leafName, ZR_FALSE, ZR_NULL)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool extern_compiler_extract_string_argument(SZrFunctionCall *call, SZrString **outString) {
    if (outString != ZR_NULL) {
        *outString = ZR_NULL;
    }
    if (call == ZR_NULL || outString == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }
    if (call->args->nodes[0] == ZR_NULL || call->args->nodes[0]->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    *outString = call->args->nodes[0]->data.stringLiteral.value;
    return *outString != ZR_NULL;
}

static TZrBool extern_compiler_extract_int_argument(SZrFunctionCall *call, TZrInt64 *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (call == ZR_NULL || outValue == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }
    if (call->args->nodes[0] == ZR_NULL || call->args->nodes[0]->type != ZR_AST_INTEGER_LITERAL) {
        return ZR_FALSE;
    }

    *outValue = call->args->nodes[0]->data.integerLiteral.value;
    return ZR_TRUE;
}

static SZrString *extern_compiler_decorators_get_string_arg(SZrAstNodeArray *decorators, const TZrChar *leafName) {
    SZrFunctionCall *call = ZR_NULL;
    SZrString *result = ZR_NULL;

    if (extern_compiler_decorators_find_call(decorators, leafName, &call) == ZR_NULL) {
        return ZR_NULL;
    }

    return extern_compiler_extract_string_argument(call, &result) ? result : ZR_NULL;
}

static TZrBool extern_compiler_decorators_get_int_arg(SZrAstNodeArray *decorators,
                                                      const TZrChar *leafName,
                                                      TZrInt64 *outValue) {
    SZrFunctionCall *call = ZR_NULL;
    if (extern_compiler_decorators_find_call(decorators, leafName, &call) == ZR_NULL) {
        return ZR_FALSE;
    }

    return extern_compiler_extract_int_argument(call, outValue);
}

static SZrAstNode *extern_compiler_find_named_declaration(SZrExternBlock *externBlock, SZrString *name) {
    if (externBlock == ZR_NULL || name == ZR_NULL || externBlock->declarations == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL &&
                    declaration->data.externDelegateDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.externDelegateDeclaration.name->name, name)) {
                    return declaration;
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL &&
                    declaration->data.structDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.structDeclaration.name->name, name)) {
                    return declaration;
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL &&
                    declaration->data.enumDeclaration.name->name != ZR_NULL &&
                    ZrCore_String_Equal(declaration->data.enumDeclaration.name->name, name)) {
                    return declaration;
                }
                break;
            default:
                break;
        }
    }

    return ZR_NULL;
}

static TZrBool extern_compiler_is_precise_ffi_primitive_name(SZrString *name) {
    static const TZrChar *kSupported[] = {
            "void", "bool", "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "f32", "f64"
    };

    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kSupported); index++) {
        if (extern_compiler_string_equals(name, kSupported[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool extern_compiler_wrap_pointer_descriptor(SZrCompilerState *cs,
                                                       SZrTypeValue *baseValue,
                                                       const TZrChar *directionText) {
    SZrObject *pointerObject;
    SZrTypeValue objectValue;
    ZrExternCompilerTempRoot pointerRoot;

    if (cs == ZR_NULL || baseValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &pointerRoot)) {
        return ZR_FALSE;
    }

    pointerObject = extern_compiler_new_object_constant(cs);
    if (pointerObject == ZR_NULL) {
        extern_compiler_temp_root_end(&pointerRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&pointerRoot, pointerObject, ZR_VALUE_TYPE_OBJECT);

    if (!extern_compiler_make_string_value(cs, "pointer", &objectValue) ||
        !extern_compiler_set_object_field(cs, pointerObject, "kind", &objectValue) ||
        !extern_compiler_set_object_field(cs, pointerObject, "to", baseValue)) {
        extern_compiler_temp_root_end(&pointerRoot);
        return ZR_FALSE;
    }

    if (directionText != ZR_NULL && directionText[0] != '\0') {
        if (!extern_compiler_make_string_value(cs, directionText, &objectValue) ||
            !extern_compiler_set_object_field(cs, pointerObject, "direction", &objectValue)) {
            extern_compiler_temp_root_end(&pointerRoot);
            return ZR_FALSE;
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, baseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(pointerObject));
    baseValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&pointerRoot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_descriptor_set_string_field(SZrCompilerState *cs,
                                                           SZrObject *object,
                                                           const TZrChar *fieldName,
                                                           const TZrChar *text) {
    SZrTypeValue stringValue;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!extern_compiler_make_string_value(cs, text, &stringValue)) {
        return ZR_FALSE;
    }

    return extern_compiler_set_object_field(cs, object, fieldName, &stringValue);
}

static TZrBool extern_compiler_descriptor_set_string_object_field(SZrCompilerState *cs,
                                                                  SZrObject *object,
                                                                  const TZrChar *fieldName,
                                                                  SZrString *text) {
    SZrTypeValue stringValue;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(text));
    stringValue.type = ZR_VALUE_TYPE_STRING;
    return extern_compiler_set_object_field(cs, object, fieldName, &stringValue);
}

static TZrBool extern_compiler_descriptor_set_int_field(SZrCompilerState *cs,
                                                        SZrObject *object,
                                                        const TZrChar *fieldName,
                                                        TZrInt64 value) {
    SZrTypeValue intValue;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsInt(cs->state, &intValue, value);
    return extern_compiler_set_object_field(cs, object, fieldName, &intValue);
}

static const TZrChar *extern_compiler_direction_from_decorators(SZrAstNodeArray *decorators) {
    if (extern_compiler_decorators_has_flag(decorators, "out")) {
        return "out";
    }
    if (extern_compiler_decorators_has_flag(decorators, "inout")) {
        return "inout";
    }
    if (extern_compiler_decorators_has_flag(decorators, "in")) {
        return "in";
    }
    return ZR_NULL;
}

static TZrBool extern_compiler_apply_string_charset(SZrCompilerState *cs,
                                                    SZrTypeValue *descriptorValue,
                                                    SZrString *charsetName) {
    SZrObject *stringObject;
    SZrTypeValue kindValue;
    const TZrChar *kindText = ZR_NULL;
    ZrExternCompilerTempRoot stringRoot;

    if (cs == ZR_NULL || descriptorValue == ZR_NULL || charsetName == ZR_NULL) {
        return ZR_TRUE;
    }

    if (descriptorValue->type == ZR_VALUE_TYPE_STRING && descriptorValue->value.object != ZR_NULL) {
        kindText = ZrCore_String_GetNativeString(ZR_CAST_STRING(cs->state, descriptorValue->value.object));
        if (kindText != ZR_NULL && strcmp(kindText, "string") == 0) {
            if (!extern_compiler_temp_root_begin(cs, &stringRoot)) {
                return ZR_FALSE;
            }
            stringObject = extern_compiler_new_object_constant(cs);
            if (stringObject == ZR_NULL) {
                extern_compiler_temp_root_end(&stringRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&stringRoot, stringObject, ZR_VALUE_TYPE_OBJECT);
            if (!extern_compiler_make_string_value(cs, "string", &kindValue) ||
                !extern_compiler_set_object_field(cs, stringObject, "kind", &kindValue) ||
                !extern_compiler_descriptor_set_string_object_field(cs, stringObject, "encoding", charsetName)) {
                extern_compiler_temp_root_end(&stringRoot);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(cs->state, descriptorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
            descriptorValue->type = ZR_VALUE_TYPE_OBJECT;
            extern_compiler_temp_root_end(&stringRoot);
        }
        return ZR_TRUE;
    }

    if (descriptorValue->type == ZR_VALUE_TYPE_OBJECT && descriptorValue->value.object != ZR_NULL) {
        stringObject = ZR_CAST_OBJECT(cs->state, descriptorValue->value.object);
        if (stringObject == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!extern_compiler_descriptor_set_string_object_field(cs, stringObject, "encoding", charsetName)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool extern_compiler_build_type_descriptor_value(SZrCompilerState *cs,
                                                           SZrExternBlock *externBlock,
                                                           SZrType *type,
                                                           SZrAstNodeArray *decorators,
                                                           SZrFileRange location,
                                                           SZrTypeValue *outValue);

static TZrBool extern_compiler_build_signature_descriptor_value(SZrCompilerState *cs,
                                                                SZrExternBlock *externBlock,
                                                                SZrAstNodeArray *params,
                                                                SZrParameter *args,
                                                                SZrType *returnType,
                                                                SZrAstNodeArray *decorators,
                                                                TZrBool includeKind,
                                                                SZrFileRange location,
                                                                SZrTypeValue *outValue);

static TZrBool extern_compiler_build_struct_descriptor_value(SZrCompilerState *cs,
                                                             SZrExternBlock *externBlock,
                                                             SZrAstNode *declarationNode,
                                                             SZrTypeValue *outValue);

static TZrBool extern_compiler_build_enum_descriptor_value(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeValue *outValue);

static TZrBool extern_compiler_build_delegate_descriptor_value(SZrCompilerState *cs,
                                                               SZrExternBlock *externBlock,
                                                               SZrAstNode *declarationNode,
                                                               TZrBool includeKind,
                                                               SZrTypeValue *outValue) {
    SZrExternDelegateDeclaration *delegateDecl;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_EXTERN_DELEGATE_DECLARATION ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    delegateDecl = &declarationNode->data.externDelegateDeclaration;
    return extern_compiler_build_signature_descriptor_value(cs,
                                                            externBlock,
                                                            delegateDecl->params,
                                                            delegateDecl->args,
                                                            delegateDecl->returnType,
                                                            delegateDecl->decorators,
                                                            includeKind,
                                                            declarationNode->location,
                                                            outValue);
}

static TZrBool extern_compiler_build_type_descriptor_value(SZrCompilerState *cs,
                                                           SZrExternBlock *externBlock,
                                                           SZrType *type,
                                                           SZrAstNodeArray *decorators,
                                                           SZrFileRange location,
                                                           SZrTypeValue *outValue) {
    SZrString *charsetName;
    const TZrChar *directionText;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (cs == ZR_NULL || type == ZR_NULL || outValue == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "extern ffi type descriptor is missing a type annotation", location);
        return ZR_FALSE;
    }

    charsetName = extern_compiler_decorators_get_string_arg(decorators, "charset");
    directionText = extern_compiler_direction_from_decorators(decorators);

    if (type->name != ZR_NULL && type->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &type->name->data.genericType;
        if (genericType->name != ZR_NULL &&
            extern_compiler_string_equals(genericType->name->name, "pointer") &&
            genericType->params != ZR_NULL &&
            genericType->params->count == 1 &&
            genericType->params->nodes[0] != ZR_NULL &&
            genericType->params->nodes[0]->type == ZR_AST_TYPE) {
            if (!extern_compiler_build_type_descriptor_value(cs,
                                                             externBlock,
                                                             &genericType->params->nodes[0]->data.type,
                                                             ZR_NULL,
                                                             genericType->params->nodes[0]->location,
                                                             outValue) ||
                !extern_compiler_wrap_pointer_descriptor(cs,
                                                         outValue,
                                                         directionText != ZR_NULL ? directionText : "in")) {
                return ZR_FALSE;
            }
            return ZR_TRUE;
        }
    }

    if (type->name == ZR_NULL || type->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        type->name->data.identifier.name == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "extern ffi only accepts precise identifier or pointer<T> type syntax in v1",
                                location);
        return ZR_FALSE;
    }

    if (extern_compiler_is_precise_ffi_primitive_name(type->name->data.identifier.name)) {
        ZrCore_Value_InitAsRawObject(cs->state,
                                     outValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(type->name->data.identifier.name));
        outValue->type = ZR_VALUE_TYPE_STRING;
    } else if (extern_compiler_string_equals(type->name->data.identifier.name, "string")) {
        if (!extern_compiler_make_string_value(cs, "string", outValue)) {
            ZrParser_Compiler_Error(cs, "failed to build extern ffi string descriptor", location);
            return ZR_FALSE;
        }
    } else {
        SZrAstNode *namedDeclaration =
                extern_compiler_find_named_declaration(externBlock, type->name->data.identifier.name);
        if (namedDeclaration == ZR_NULL) {
            ZrParser_Compiler_Error(cs,
                                    "extern ffi type must resolve to a precise primitive or extern struct/enum/delegate",
                                    location);
            return ZR_FALSE;
        }

        switch (namedDeclaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (!extern_compiler_build_delegate_descriptor_value(cs, externBlock, namedDeclaration, ZR_TRUE, outValue)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (!extern_compiler_build_struct_descriptor_value(cs, externBlock, namedDeclaration, outValue)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (!extern_compiler_build_enum_descriptor_value(cs, namedDeclaration, outValue)) {
                    return ZR_FALSE;
                }
                break;
            default:
                ZrParser_Compiler_Error(cs, "unsupported extern ffi referenced type declaration", location);
                return ZR_FALSE;
        }
    }

    if (!extern_compiler_apply_string_charset(cs, outValue, charsetName)) {
        ZrParser_Compiler_Error(cs, "failed to apply extern ffi charset decorator", location);
        return ZR_FALSE;
    }

    if (directionText != ZR_NULL) {
        if (!extern_compiler_wrap_pointer_descriptor(cs, outValue, directionText)) {
            ZrParser_Compiler_Error(cs, "failed to apply extern ffi parameter direction", location);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool extern_compiler_build_signature_descriptor_value(SZrCompilerState *cs,
                                                                SZrExternBlock *externBlock,
                                                                SZrAstNodeArray *params,
                                                                SZrParameter *args,
                                                                SZrType *returnType,
                                                                SZrAstNodeArray *decorators,
                                                                TZrBool includeKind,
                                                                SZrFileRange location,
                                                                SZrTypeValue *outValue) {
    SZrObject *signatureObject;
    SZrObject *parametersArray;
    SZrTypeValue signatureValue;
    SZrTypeValue returnTypeValue;
    SZrString *callconvName;
    ZrExternCompilerTempRoot signatureRoot;
    ZrExternCompilerTempRoot parametersRoot;

    if (cs == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &signatureRoot) ||
        !extern_compiler_temp_root_begin(cs, &parametersRoot)) {
        if (signatureRoot.active) {
            extern_compiler_temp_root_end(&signatureRoot);
        }
        if (parametersRoot.active) {
            extern_compiler_temp_root_end(&parametersRoot);
        }
        return ZR_FALSE;
    }

    signatureObject = extern_compiler_new_object_constant(cs);
    parametersArray = extern_compiler_new_array_constant(cs);
    if (signatureObject == ZR_NULL || parametersArray == ZR_NULL) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to allocate extern ffi signature descriptor", location);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&signatureRoot, signatureObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&parametersRoot, parametersArray, ZR_VALUE_TYPE_ARRAY);
    if (includeKind &&
        !extern_compiler_descriptor_set_string_field(cs, signatureObject, "kind", "function")) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to build extern ffi function kind", location);
        return ZR_FALSE;
    }

    if (returnType != ZR_NULL) {
        if (!extern_compiler_build_type_descriptor_value(cs,
                                                         externBlock,
                                                         returnType,
                                                         decorators,
                                                         location,
                                                         &returnTypeValue)) {
            extern_compiler_temp_root_end(&parametersRoot);
            extern_compiler_temp_root_end(&signatureRoot);
            return ZR_FALSE;
        }
    } else if (!extern_compiler_make_string_value(cs, "void", &returnTypeValue)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to build extern ffi void return type", location);
        return ZR_FALSE;
    }

    if (!extern_compiler_set_object_field(cs, signatureObject, "returnType", &returnTypeValue)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to set extern ffi return type", location);
        return ZR_FALSE;
    }

    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrAstNode *paramNode = params->nodes[index];
            SZrObject *parameterObject;
            SZrTypeValue parameterObjectValue;
            SZrTypeValue parameterTypeValue;
            ZrExternCompilerTempRoot parameterRoot;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (!extern_compiler_build_type_descriptor_value(cs,
                                                             externBlock,
                                                             paramNode->data.parameter.typeInfo,
                                                             paramNode->data.parameter.decorators,
                                                             paramNode->location,
                                                             &parameterTypeValue)) {
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                return ZR_FALSE;
            }

            if (!extern_compiler_temp_root_begin(cs, &parameterRoot)) {
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                ZrParser_Compiler_Error(cs, "failed to root extern ffi parameter descriptor", paramNode->location);
                return ZR_FALSE;
            }
            parameterObject = extern_compiler_new_object_constant(cs);
            if (parameterObject == ZR_NULL ||
                !extern_compiler_set_object_field(cs, parameterObject, "type", &parameterTypeValue)) {
                extern_compiler_temp_root_end(&parameterRoot);
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                ZrParser_Compiler_Error(cs, "failed to build extern ffi parameter descriptor", paramNode->location);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&parameterRoot, parameterObject, ZR_VALUE_TYPE_OBJECT);

            ZrCore_Value_InitAsRawObject(cs->state, &parameterObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(parameterObject));
            parameterObjectValue.type = ZR_VALUE_TYPE_OBJECT;
            if (!extern_compiler_push_array_value(cs, parametersArray, &parameterObjectValue)) {
                extern_compiler_temp_root_end(&parameterRoot);
                extern_compiler_temp_root_end(&parametersRoot);
                extern_compiler_temp_root_end(&signatureRoot);
                ZrParser_Compiler_Error(cs, "failed to append extern ffi parameter descriptor", paramNode->location);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_end(&parameterRoot);
        }
    }

    if (args != ZR_NULL) {
        SZrTypeValue varargsValue;
        ZrCore_Value_ResetAsNull(&varargsValue);
        varargsValue.type = ZR_VALUE_TYPE_BOOL;
        varargsValue.value.nativeObject.nativeBool = ZR_TRUE;
        if (!extern_compiler_set_object_field(cs, signatureObject, "varargs", &varargsValue)) {
            extern_compiler_temp_root_end(&parametersRoot);
            extern_compiler_temp_root_end(&signatureRoot);
            ZrParser_Compiler_Error(cs, "failed to mark extern ffi varargs signature", location);
            return ZR_FALSE;
        }
    }

    callconvName = extern_compiler_decorators_get_string_arg(decorators, "callconv");
    if (callconvName != ZR_NULL &&
        !extern_compiler_descriptor_set_string_object_field(cs, signatureObject, "abi", callconvName)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to set extern ffi calling convention", location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &signatureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(parametersArray));
    signatureValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, signatureObject, "parameters", &signatureValue)) {
        extern_compiler_temp_root_end(&parametersRoot);
        extern_compiler_temp_root_end(&signatureRoot);
        ZrParser_Compiler_Error(cs, "failed to set extern ffi parameters array", location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(signatureObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&parametersRoot);
    extern_compiler_temp_root_end(&signatureRoot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_build_struct_descriptor_value(SZrCompilerState *cs,
                                                             SZrExternBlock *externBlock,
                                                             SZrAstNode *declarationNode,
                                                             SZrTypeValue *outValue) {
    SZrStructDeclaration *structDecl;
    SZrObject *structObject;
    SZrObject *fieldsArray;
    SZrTypeValue fieldsValue;
    TZrInt64 packValue = 0;
    TZrInt64 alignValue = 0;
    ZrExternCompilerTempRoot structRoot;
    ZrExternCompilerTempRoot fieldsRoot;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_STRUCT_DECLARATION ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &structRoot) ||
        !extern_compiler_temp_root_begin(cs, &fieldsRoot)) {
        if (structRoot.active) {
            extern_compiler_temp_root_end(&structRoot);
        }
        if (fieldsRoot.active) {
            extern_compiler_temp_root_end(&fieldsRoot);
        }
        return ZR_FALSE;
    }

    structDecl = &declarationNode->data.structDeclaration;
    structObject = extern_compiler_new_object_constant(cs);
    fieldsArray = extern_compiler_new_array_constant(cs);
    if (structObject == ZR_NULL || fieldsArray == ZR_NULL ||
        structDecl->name == ZR_NULL || structDecl->name->name == ZR_NULL) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&structRoot, structObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&fieldsRoot, fieldsArray, ZR_VALUE_TYPE_ARRAY);

    if (!extern_compiler_descriptor_set_string_field(cs, structObject, "kind", "struct") ||
        !extern_compiler_descriptor_set_string_object_field(cs, structObject, "name", structDecl->name->name)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }

    if (extern_compiler_decorators_get_int_arg(structDecl->decorators, "pack", &packValue) &&
        !extern_compiler_descriptor_set_int_field(cs, structObject, "pack", packValue)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }
    if (extern_compiler_decorators_get_int_arg(structDecl->decorators, "align", &alignValue) &&
        !extern_compiler_descriptor_set_int_field(cs, structObject, "align", alignValue)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            SZrAstNode *member = structDecl->members->nodes[index];
            SZrObject *fieldObject;
            SZrTypeValue fieldObjectValue;
            SZrTypeValue fieldTypeValue;
            TZrInt64 offsetValue = 0;
            ZrExternCompilerTempRoot fieldRoot;

            if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD) {
                continue;
            }

            if (!extern_compiler_build_type_descriptor_value(cs,
                                                             externBlock,
                                                             member->data.structField.typeInfo,
                                                             member->data.structField.decorators,
                                                             member->location,
                                                             &fieldTypeValue)) {
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }

            if (!extern_compiler_temp_root_begin(cs, &fieldRoot)) {
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }
            fieldObject = extern_compiler_new_object_constant(cs);
            if (fieldObject == ZR_NULL ||
                member->data.structField.name == ZR_NULL ||
                member->data.structField.name->name == ZR_NULL ||
                !extern_compiler_descriptor_set_string_object_field(cs,
                                                                    fieldObject,
                                                                    "name",
                                                                    member->data.structField.name->name) ||
                !extern_compiler_set_object_field(cs, fieldObject, "type", &fieldTypeValue)) {
                extern_compiler_temp_root_end(&fieldRoot);
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&fieldRoot, fieldObject, ZR_VALUE_TYPE_OBJECT);

            if (extern_compiler_decorators_get_int_arg(member->data.structField.decorators, "offset", &offsetValue) &&
                !extern_compiler_descriptor_set_int_field(cs, fieldObject, "offset", offsetValue)) {
                extern_compiler_temp_root_end(&fieldRoot);
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(cs->state, &fieldObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
            fieldObjectValue.type = ZR_VALUE_TYPE_OBJECT;
            if (!extern_compiler_push_array_value(cs, fieldsArray, &fieldObjectValue)) {
                extern_compiler_temp_root_end(&fieldRoot);
                extern_compiler_temp_root_end(&fieldsRoot);
                extern_compiler_temp_root_end(&structRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_end(&fieldRoot);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, &fieldsValue, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldsArray));
    fieldsValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, structObject, "fields", &fieldsValue)) {
        extern_compiler_temp_root_end(&fieldsRoot);
        extern_compiler_temp_root_end(&structRoot);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(structObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&fieldsRoot);
    extern_compiler_temp_root_end(&structRoot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_build_enum_descriptor_value(SZrCompilerState *cs,
                                                           SZrAstNode *declarationNode,
                                                           SZrTypeValue *outValue) {
    SZrEnumDeclaration *enumDecl;
    SZrObject *enumObject;
    SZrObject *membersArray;
    SZrTypeValue membersValue;
    SZrTypeValue underlyingValue;
    TZrInt64 nextAutoValue = 0;
    ZrExternCompilerTempRoot enumRoot;
    ZrExternCompilerTempRoot membersRoot;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_ENUM_DECLARATION ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!extern_compiler_temp_root_begin(cs, &enumRoot) ||
        !extern_compiler_temp_root_begin(cs, &membersRoot)) {
        if (enumRoot.active) {
            extern_compiler_temp_root_end(&enumRoot);
        }
        if (membersRoot.active) {
            extern_compiler_temp_root_end(&membersRoot);
        }
        return ZR_FALSE;
    }

    enumDecl = &declarationNode->data.enumDeclaration;
    enumObject = extern_compiler_new_object_constant(cs);
    membersArray = extern_compiler_new_array_constant(cs);
    if (enumObject == ZR_NULL || membersArray == ZR_NULL ||
        enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }
    extern_compiler_temp_root_set_object(&enumRoot, enumObject, ZR_VALUE_TYPE_OBJECT);
    extern_compiler_temp_root_set_object(&membersRoot, membersArray, ZR_VALUE_TYPE_ARRAY);

    if (!extern_compiler_descriptor_set_string_field(cs, enumObject, "kind", "enum") ||
        !extern_compiler_descriptor_set_string_object_field(cs, enumObject, "name", enumDecl->name->name)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }

    if (enumDecl->baseType != ZR_NULL) {
        if (!extern_compiler_build_type_descriptor_value(cs,
                                                         ZR_NULL,
                                                         enumDecl->baseType,
                                                         enumDecl->decorators,
                                                         declarationNode->location,
                                                         &underlyingValue)) {
            extern_compiler_temp_root_end(&membersRoot);
            extern_compiler_temp_root_end(&enumRoot);
            return ZR_FALSE;
        }
    } else {
        SZrString *underlyingName = extern_compiler_decorators_get_string_arg(enumDecl->decorators, "underlying");
        if (underlyingName != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(cs->state, &underlyingValue, ZR_CAST_RAW_OBJECT_AS_SUPER(underlyingName));
            underlyingValue.type = ZR_VALUE_TYPE_STRING;
        } else if (!extern_compiler_make_string_value(cs, "i32", &underlyingValue)) {
            extern_compiler_temp_root_end(&membersRoot);
            extern_compiler_temp_root_end(&enumRoot);
            return ZR_FALSE;
        }
    }

    if (!extern_compiler_set_object_field(cs, enumObject, "underlyingType", &underlyingValue)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }

    if (enumDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < enumDecl->members->count; index++) {
            SZrAstNode *memberNode = enumDecl->members->nodes[index];
            SZrObject *memberObject;
            SZrTypeValue memberObjectValue;
            SZrTypeValue memberValue;
            TZrInt64 explicitValue = 0;
            TZrBool hasExplicitValue = ZR_FALSE;
            const TZrChar *memberNameText;
            ZrExternCompilerTempRoot memberRoot;

            if (memberNode == ZR_NULL || memberNode->type != ZR_AST_ENUM_MEMBER ||
                memberNode->data.enumMember.name == ZR_NULL || memberNode->data.enumMember.name->name == ZR_NULL) {
                continue;
            }

            if (!extern_compiler_temp_root_begin(cs, &memberRoot)) {
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }
            memberObject = extern_compiler_new_object_constant(cs);
            if (memberObject == ZR_NULL ||
                !extern_compiler_descriptor_set_string_object_field(cs,
                                                                    memberObject,
                                                                    "name",
                                                                    memberNode->data.enumMember.name->name)) {
                extern_compiler_temp_root_end(&memberRoot);
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_set_object(&memberRoot, memberObject, ZR_VALUE_TYPE_OBJECT);

            if (memberNode->data.enumMember.value != ZR_NULL &&
                memberNode->data.enumMember.value->type == ZR_AST_INTEGER_LITERAL) {
                explicitValue = memberNode->data.enumMember.value->data.integerLiteral.value;
                hasExplicitValue = ZR_TRUE;
            } else if (extern_compiler_decorators_get_int_arg(memberNode->data.enumMember.decorators,
                                                              "value",
                                                              &explicitValue)) {
                hasExplicitValue = ZR_TRUE;
            }

            if (!hasExplicitValue) {
                explicitValue = nextAutoValue;
            }
            nextAutoValue = explicitValue + 1;

            ZrCore_Value_InitAsInt(cs->state, &memberValue, explicitValue);
            memberNameText = ZrCore_String_GetNativeString(memberNode->data.enumMember.name->name);
            if (memberNameText == ZR_NULL ||
                !extern_compiler_set_object_field(cs, memberObject, "value", &memberValue) ||
                !extern_compiler_set_object_field(cs, enumObject, memberNameText, &memberValue)) {
                extern_compiler_temp_root_end(&memberRoot);
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }

            ZrCore_Value_InitAsRawObject(cs->state, &memberObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberObject));
            memberObjectValue.type = ZR_VALUE_TYPE_OBJECT;
            if (!extern_compiler_push_array_value(cs, membersArray, &memberObjectValue)) {
                extern_compiler_temp_root_end(&memberRoot);
                extern_compiler_temp_root_end(&membersRoot);
                extern_compiler_temp_root_end(&enumRoot);
                return ZR_FALSE;
            }
            extern_compiler_temp_root_end(&memberRoot);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, &membersValue, ZR_CAST_RAW_OBJECT_AS_SUPER(membersArray));
    membersValue.type = ZR_VALUE_TYPE_ARRAY;
    if (!extern_compiler_set_object_field(cs, enumObject, "members", &membersValue)) {
        extern_compiler_temp_root_end(&membersRoot);
        extern_compiler_temp_root_end(&enumRoot);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(enumObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    extern_compiler_temp_root_end(&membersRoot);
    extern_compiler_temp_root_end(&enumRoot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_emit_get_member_to_slot(SZrCompilerState *cs,
                                                       TZrUInt32 destSlot,
                                                       TZrUInt32 objectSlot,
                                                       SZrString *memberName) {
    TZrUInt32 keySlot;

    if (cs == ZR_NULL || memberName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    keySlot = allocate_stack_slot(cs);
    emit_string_constant_to_slot(cs, keySlot, memberName);
    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE),
                                          (TZrUInt16)destSlot,
                                          (TZrUInt16)objectSlot,
                                          (TZrUInt16)keySlot));
    ZrParser_Compiler_TrimStackToSlot(cs, destSlot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_emit_import_module_to_local(SZrCompilerState *cs,
                                                           SZrString *moduleName,
                                                           TZrUInt32 localSlot,
                                                           SZrFileRange location) {
    TZrUInt32 importSlot;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    importSlot = ZrParser_Compiler_EmitImportModuleExpression(cs, moduleName, location);
    if (importSlot == (TZrUInt32)-1) {
        return ZR_FALSE;
    }

    if (importSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)importSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_emit_module_function_call_to_local(SZrCompilerState *cs,
                                                                  TZrUInt32 moduleSlot,
                                                                  SZrString *functionName,
                                                                  const SZrTypeValue *argumentValues,
                                                                  TZrUInt32 argumentCount,
                                                                  TZrUInt32 localSlot,
                                                                  SZrFileRange location) {
    TZrUInt32 functionSlot;

    if (cs == ZR_NULL || functionName == ZR_NULL || (argumentCount > 0 && argumentValues == ZR_NULL) || cs->hasError) {
        return ZR_FALSE;
    }

    functionSlot = allocate_stack_slot(cs);
    if (!extern_compiler_emit_get_member_to_slot(cs, functionSlot, moduleSlot, functionName)) {
        ZrParser_Compiler_Error(cs, "failed to resolve extern ffi module function", location);
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 argumentSlot = allocate_stack_slot(cs);
        emit_constant_to_slot(cs, argumentSlot, &argumentValues[index]);
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)argumentCount));
    if (functionSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)functionSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

static TZrBool extern_compiler_emit_method_call_to_local(SZrCompilerState *cs,
                                                         TZrUInt32 receiverSlot,
                                                         SZrString *methodName,
                                                         const SZrTypeValue *argumentValues,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt32 localSlot,
                                                         SZrFileRange location) {
    TZrUInt32 functionSlot;
    TZrUInt32 selfSlot;

    if (cs == ZR_NULL || methodName == ZR_NULL || (argumentCount > 0 && argumentValues == ZR_NULL) || cs->hasError) {
        return ZR_FALSE;
    }

    functionSlot = allocate_stack_slot(cs);
    if (!extern_compiler_emit_get_member_to_slot(cs, functionSlot, receiverSlot, methodName)) {
        ZrParser_Compiler_Error(cs, "failed to resolve extern ffi receiver method", location);
        return ZR_FALSE;
    }

    selfSlot = allocate_stack_slot(cs);
    emit_instruction(cs,
                     create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                          (TZrUInt16)selfSlot,
                                          (TZrInt32)receiverSlot));
    for (TZrUInt32 index = 0; index < argumentCount; index++) {
        TZrUInt32 argumentSlot = allocate_stack_slot(cs);
        emit_constant_to_slot(cs, argumentSlot, &argumentValues[index]);
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)functionSlot,
                                          (TZrUInt16)(argumentCount + 1)));
    if (functionSlot != localSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)localSlot,
                                              (TZrInt32)functionSlot));
    }
    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
    return ZR_TRUE;
}

// 进入新作用域
void enter_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    SZrScope scope;
    scope.startVarIndex = cs->localVarCount;
    scope.varCount = 0;
    scope.cleanupRegistrationCount = 0;
    // 如果当前编译器有父编译器，则新作用域的父编译器就是当前编译器
    // 否则，如果当前编译器是顶层编译器，其父编译器为NULL
    scope.parentCompiler = cs->currentFunction != ZR_NULL ? cs : ZR_NULL;

    ZrCore_Array_Push(cs->state, &cs->scopeStack, &scope);
}

// 退出作用域
void exit_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    if (cs->scopeStack.length == 0) {
        return;
    }

    SZrScope *scope = (SZrScope *) ZrCore_Array_Pop(&cs->scopeStack);
    if (scope != ZR_NULL) {
        if (scope->cleanupRegistrationCount > 0) {
            TZrInstruction cleanupInst = create_instruction_0(
                ZR_INSTRUCTION_ENUM(CLOSE_SCOPE),
                (TZrUInt16)scope->cleanupRegistrationCount);
            emit_instruction(cs, cleanupInst);
        }

        // 标记作用域内变量的结束位置
        TZrMemoryOffset endOffset = (TZrMemoryOffset) cs->instructionCount;
        // 使用 localVars.length 而不是 localVarCount，确保同步
        TZrSize varCount = cs->localVars.length;
        for (TZrSize i = scope->startVarIndex; i < scope->startVarIndex + scope->varCount; i++) {
            if (i < varCount) {
                SZrFunctionLocalVariable *var = (SZrFunctionLocalVariable *) ZrCore_Array_Get(&cs->localVars, i);
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
    SZrTypeEnvironment *newEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (newEnv == ZR_NULL) {
        return;
    }
    
    // 设置父环境为当前环境
    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = (cs->typeEnv != ZR_NULL)
                                  ? cs->typeEnv->semanticContext
                                  : cs->semanticContext;
    
    // 将当前环境推入栈
    if (cs->typeEnv != ZR_NULL) {
        ZrCore_Array_Push(cs->state, &cs->typeEnvStack, &cs->typeEnv);
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
            ZrParser_TypeEnvironment_Free(cs->state, cs->typeEnv);
            cs->typeEnv = ZR_NULL;
        }
        return;
    }
    
    // 释放当前环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(cs->state, cs->typeEnv);
    }
    
    // 从栈中弹出父环境
    SZrTypeEnvironment **parentEnvPtr = (SZrTypeEnvironment **)ZrCore_Array_Pop(&cs->typeEnvStack);
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

    ZrCore_Array_Push(cs->state, &cs->labels, &label);
    return cs->labels.length - 1;
}

// 解析标签
void resolve_label(SZrCompilerState *cs, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError || labelId >= cs->labels.length) {
        return;
    }

    SZrLabel *label = (SZrLabel *) ZrCore_Array_Get(&cs->labels, labelId);
    if (label != ZR_NULL) {
        label->instructionIndex = cs->instructionCount;
        label->isResolved = ZR_TRUE;

        // 填充所有指向该标签的跳转指令的偏移量
        for (TZrSize i = 0; i < cs->pendingJumps.length; i++) {
            SZrPendingJump *pendingJump = (SZrPendingJump *) ZrCore_Array_Get(&cs->pendingJumps, i);
            if (pendingJump != ZR_NULL && pendingJump->labelId == labelId &&
                pendingJump->instructionIndex < cs->instructions.length) {
                TZrInstruction *jumpInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, pendingJump->instructionIndex);
                if (jumpInst != ZR_NULL) {
                    // 计算相对偏移：目标指令索引 - (当前指令索引 + 1)
                    // 因为 ZR_INSTRUCTION_FETCH 已经将 PC 指向下一条指令，所以需要 -1
                    TZrInt32 offset = (TZrInt32) label->instructionIndex - (TZrInt32) pendingJump->instructionIndex - 1;
                    jumpInst->instruction.operand.operand2[0] = offset;
                }
            }
        }

        for (TZrSize i = 0; i < cs->pendingAbsolutePatches.length; i++) {
            SZrPendingAbsolutePatch *pendingPatch =
                    (SZrPendingAbsolutePatch *)ZrCore_Array_Get(&cs->pendingAbsolutePatches, i);
            if (pendingPatch != ZR_NULL && pendingPatch->labelId == labelId &&
                pendingPatch->instructionIndex < cs->instructions.length) {
                TZrInstruction *inst =
                        (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, pendingPatch->instructionIndex);
                if (inst != ZR_NULL) {
                    inst->instruction.operand.operand2[0] = (TZrInt32)label->instructionIndex;
                }
            }
        }
    }
}

// 添加待解析的跳转（在 compiler.c 中定义，在 ZrParser_Statement_Compile.c 和 ZrParser_Expression_Compile.c 中使用）
void add_pending_jump(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) {
    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    SZrPendingJump pendingJump;
    pendingJump.instructionIndex = instructionIndex;
    pendingJump.labelId = labelId;

    ZrCore_Array_Push(cs->state, &cs->pendingJumps, &pendingJump);
}

void add_pending_absolute_patch(SZrCompilerState *cs, TZrSize instructionIndex, TZrSize labelId) {
    SZrPendingAbsolutePatch pendingPatch;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    pendingPatch.instructionIndex = instructionIndex;
    pendingPatch.labelId = labelId;
    ZrCore_Array_Push(cs->state, &cs->pendingAbsolutePatches, &pendingPatch);
}

// 外部变量分析辅助函数（用于闭包捕获）

// TODO: 记录引用的外部变量（简化实现：直接添加到列表）
static void record_external_var_reference(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 检查是否已存在
    for (TZrSize i = 0; i < cs->referencedExternalVars.length; i++) {
        SZrString **varName = (SZrString **)ZrCore_Array_Get(&cs->referencedExternalVars, i);
        if (varName != ZR_NULL && *varName == name) {
            return; // 已存在
        }
    }
    
    // 添加到列表
    ZrCore_Array_Push(cs->state, &cs->referencedExternalVars, &name);
}

static void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames);

static void collect_identifiers_from_array(SZrCompilerState *cs, SZrAstNodeArray *nodes, SZrArray *identifierNames) {
    if (cs == ZR_NULL || nodes == ZR_NULL || identifierNames == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < nodes->count; i++) {
        SZrAstNode *child = nodes->nodes[i];
        if (child != ZR_NULL) {
            collect_identifiers_from_node(cs, child, identifierNames);
        }
    }
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
            TZrBool exists = ZR_FALSE;
            for (TZrSize i = 0; i < identifierNames->length; i++) {
                SZrString **existingName = (SZrString **)ZrCore_Array_Get(identifierNames, i);
                if (existingName != ZR_NULL && *existingName == name) {
                    exists = ZR_TRUE;
                    break;
                }
            }
            if (!exists) {
                ZrCore_Array_Push(cs->state, identifierNames, &name);
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
            collect_identifiers_from_array(cs, funcCall->args, identifierNames);
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *memberExpr = &node->data.memberExpression;
            if (memberExpr->computed && memberExpr->property != ZR_NULL) {
                collect_identifiers_from_node(cs, memberExpr->property, identifierNames);
            }
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->property != ZR_NULL) {
                collect_identifiers_from_node(cs, primary->property, identifierNames);
            }
            collect_identifiers_from_array(cs, primary->members, identifierNames);
            break;
        }
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION: {
            SZrPrototypeReferenceExpression *prototypeRef = &node->data.prototypeReferenceExpression;
            if (prototypeRef->target != ZR_NULL) {
                collect_identifiers_from_node(cs, prototypeRef->target, identifierNames);
            }
            break;
        }
        case ZR_AST_CONSTRUCT_EXPRESSION: {
            SZrConstructExpression *construct = &node->data.constructExpression;
            if (construct->target != ZR_NULL) {
                collect_identifiers_from_node(cs, construct->target, identifierNames);
            }
            collect_identifiers_from_array(cs, construct->args, identifierNames);
            break;
        }
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arrayLit = &node->data.arrayLiteral;
            collect_identifiers_from_array(cs, arrayLit->elements, identifierNames);
            break;
        }
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *objLit = &node->data.objectLiteral;
            collect_identifiers_from_array(cs, objLit->properties, identifierNames);
            break;
        }
        case ZR_AST_KEY_VALUE_PAIR: {
            SZrKeyValuePair *kv = &node->data.keyValuePair;
            if (kv->key != ZR_NULL &&
                kv->key->type != ZR_AST_IDENTIFIER_LITERAL &&
                kv->key->type != ZR_AST_STRING_LITERAL) {
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
            collect_identifiers_from_array(cs, block->body, identifierNames);
            break;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->value != ZR_NULL) {
                collect_identifiers_from_node(cs, varDecl->value, identifierNames);
            }
            break;
        }
        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            if (exprStmt->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, exprStmt->expr, identifierNames);
            }
            break;
        }
        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->resource != ZR_NULL) {
                collect_identifiers_from_node(cs, usingStmt->resource, identifierNames);
            }
            if (usingStmt->body != ZR_NULL) {
                collect_identifiers_from_node(cs, usingStmt->body, identifierNames);
            }
            break;
        }
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, returnStmt->expr, identifierNames);
            }
            break;
        }
        case ZR_AST_BREAK_CONTINUE_STATEMENT: {
            SZrBreakContinueStatement *breakContinueStmt = &node->data.breakContinueStatement;
            if (breakContinueStmt->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, breakContinueStmt->expr, identifierNames);
            }
            break;
        }
        case ZR_AST_THROW_STATEMENT: {
            SZrThrowStatement *throwStmt = &node->data.throwStatement;
            if (throwStmt->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, throwStmt->expr, identifierNames);
            }
            break;
        }
        case ZR_AST_OUT_STATEMENT: {
            SZrOutStatement *outStmt = &node->data.outStatement;
            if (outStmt->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, outStmt->expr, identifierNames);
            }
            break;
        }
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *tryStmt = &node->data.tryCatchFinallyStatement;
            if (tryStmt->block != ZR_NULL) {
                collect_identifiers_from_node(cs, tryStmt->block, identifierNames);
            }
            if (tryStmt->catchClauses != ZR_NULL) {
                for (TZrSize i = 0; i < tryStmt->catchClauses->count; i++) {
                    SZrAstNode *catchClauseNode = tryStmt->catchClauses->nodes[i];
                    if (catchClauseNode != ZR_NULL && catchClauseNode->type == ZR_AST_CATCH_CLAUSE &&
                        catchClauseNode->data.catchClause.block != ZR_NULL) {
                        collect_identifiers_from_node(cs, catchClauseNode->data.catchClause.block, identifierNames);
                    }
                }
            }
            if (tryStmt->finallyBlock != ZR_NULL) {
                collect_identifiers_from_node(cs, tryStmt->finallyBlock, identifierNames);
            }
            break;
        }
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            if (whileLoop->cond != ZR_NULL) {
                collect_identifiers_from_node(cs, whileLoop->cond, identifierNames);
            }
            if (whileLoop->block != ZR_NULL) {
                collect_identifiers_from_node(cs, whileLoop->block, identifierNames);
            }
            break;
        }
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            if (forLoop->init != ZR_NULL) {
                collect_identifiers_from_node(cs, forLoop->init, identifierNames);
            }
            if (forLoop->cond != ZR_NULL) {
                collect_identifiers_from_node(cs, forLoop->cond, identifierNames);
            }
            if (forLoop->step != ZR_NULL) {
                collect_identifiers_from_node(cs, forLoop->step, identifierNames);
            }
            if (forLoop->block != ZR_NULL) {
                collect_identifiers_from_node(cs, forLoop->block, identifierNames);
            }
            break;
        }
        case ZR_AST_FOREACH_LOOP: {
            SZrForeachLoop *foreachLoop = &node->data.foreachLoop;
            if (foreachLoop->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, foreachLoop->expr, identifierNames);
            }
            if (foreachLoop->block != ZR_NULL) {
                collect_identifiers_from_node(cs, foreachLoop->block, identifierNames);
            }
            break;
        }
        case ZR_AST_SWITCH_EXPRESSION: {
            SZrSwitchExpression *switchExpr = &node->data.switchExpression;
            if (switchExpr->expr != ZR_NULL) {
                collect_identifiers_from_node(cs, switchExpr->expr, identifierNames);
            }
            collect_identifiers_from_array(cs, switchExpr->cases, identifierNames);
            if (switchExpr->defaultCase != ZR_NULL) {
                collect_identifiers_from_node(cs, switchExpr->defaultCase, identifierNames);
            }
            break;
        }
        case ZR_AST_SWITCH_CASE: {
            SZrSwitchCase *switchCase = &node->data.switchCase;
            if (switchCase->value != ZR_NULL) {
                collect_identifiers_from_node(cs, switchCase->value, identifierNames);
            }
            if (switchCase->block != ZR_NULL) {
                collect_identifiers_from_node(cs, switchCase->block, identifierNames);
            }
            break;
        }
        case ZR_AST_SWITCH_DEFAULT: {
            SZrSwitchDefault *switchDefault = &node->data.switchDefault;
            if (switchDefault->block != ZR_NULL) {
                collect_identifiers_from_node(cs, switchDefault->block, identifierNames);
            }
            break;
        }
        default:
            // TODO: 其他节点类型暂时不处理，可以根据需要扩展
            break;
    }
}

// 分析AST节点中的外部变量引用（完整实现）
void ZrParser_ExternalVariables_Analyze(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler) {
    if (cs == ZR_NULL || node == ZR_NULL || parentCompiler == ZR_NULL || cs->hasError) {
        return;
    }
    
    // 1. 收集所有标识符引用
    SZrArray identifierNames;
    ZrCore_Array_Init(cs->state, &identifierNames, sizeof(SZrString *), 16);
    collect_identifiers_from_node(cs, node, &identifierNames);
    
    // 2. 检查每个标识符是否是外部变量
    for (TZrSize i = 0; i < identifierNames.length; i++) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get(&identifierNames, i);
        if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
            continue;
        }
        SZrString *name = *namePtr;
        
        // 在当前编译器中查找（局部变量和闭包变量）
        TZrUInt32 localIndex = find_local_var(cs, name);
        TZrUInt32 closureIndex = find_closure_var(cs, name);
        
        // 如果既不是局部变量也不是闭包变量，可能是外部变量
        if (localIndex == (TZrUInt32)-1 && closureIndex == (TZrUInt32)-1) {
            // 在父编译器中查找（外部作用域的变量）
            TZrUInt32 parentLocalIndex = find_local_var(parentCompiler, name);
            TZrUInt32 parentClosureIndex = find_closure_var(parentCompiler, name);
            if (parentLocalIndex != (TZrUInt32)-1 || parentClosureIndex != (TZrUInt32)-1) {
                // 这是外部变量，需要捕获到闭包中
                // 注意：index 必须指向父作用域中的真实槽位/上值索引，而不是当前闭包数组长度。
                if (find_closure_var(cs, name) == (TZrUInt32)-1) {
                    SZrFunctionClosureVariable closureVar;
                    closureVar.name = name;
                    closureVar.inStack = (parentLocalIndex != (TZrUInt32)-1) ? ZR_TRUE : ZR_FALSE;
                    closureVar.index = (parentLocalIndex != (TZrUInt32)-1) ? parentLocalIndex : parentClosureIndex;
                    closureVar.valueType = ZR_VALUE_TYPE_NULL;
                    ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVar);
                    cs->closureVarCount++;
                }
            }
        }
    }
    
    // 3. 清理临时数组
    ZrCore_Array_Free(cs->state, &identifierNames);
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
    // TODO: 注意：这是一个简化实现，更复杂的死代码消除需要数据流分析
    
    // 遍历指令序列，查找冗余模式
    TZrSize i = 0;
    while (i < cs->instructions.length - 1) {
        TZrInstruction *inst1 = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, i);
        TZrInstruction *inst2 = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, i + 1);
        
        if (inst1 != ZR_NULL && inst2 != ZR_NULL) {
            EZrInstructionCode opcode1 = (EZrInstructionCode)inst1->instruction.operationCode;
            EZrInstructionCode opcode2 = (EZrInstructionCode)inst2->instruction.operationCode;
            
            // 检查 SET_STACK 后立即 GET_STACK 到同一个槽位（可以消除）
            if (opcode1 == ZR_INSTRUCTION_ENUM(SET_STACK) && opcode2 == ZR_INSTRUCTION_ENUM(GET_STACK)) {
                TZrUInt16 destSlot1 = inst1->instruction.operandExtra;
                TZrUInt16 destSlot2 = inst2->instruction.operandExtra;
                TZrInt32 srcSlot2 = inst2->instruction.operand.operand1[0];
                
                // 如果 SET_STACK 的目标槽位和 GET_STACK 的源槽位相同，且目标槽位也相同
                if (destSlot1 == (TZrUInt16)srcSlot2 && destSlot1 == destSlot2) {
                    // 这是冗余操作，可以消除 GET_STACK
                    // 移除 inst2
                    for (TZrSize j = i + 1; j < cs->instructions.length - 1; j++) {
                        TZrInstruction *src = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, j + 1);
                        TZrInstruction *dst = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, j);
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
    // TODO: 注意：这需要分析跳转指令，暂时跳过
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
        TZrInstruction *inst1 = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, i);
        
        if (inst1 != ZR_NULL) {
            EZrInstructionCode opcode1 = (EZrInstructionCode)inst1->instruction.operationCode;
            
            // 检查是否是跳转指令
            if (opcode1 == ZR_INSTRUCTION_ENUM(JUMP) || opcode1 == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
                // 查找跳转目标
                // TODO: 注意：这需要标签解析后才能进行，暂时跳过详细实现
                // 这里只做基本检查
            }
        }
        
        i++;
    }
    
    // 合并相同目标的跳转
    // 如果多个跳转指令跳转到同一个标签，可以考虑优化
    // TODO: 注意：这需要更复杂的分析，暂时跳过
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
    
    // 添加更多优化步骤
    // 可以添加的优化包括：
    // 1. 常量折叠：在编译期计算常量表达式
    // 2. 死代码消除：移除永远不会执行的代码
    // 3. 循环优化：展开小循环、循环不变式外提等
    // 4. 内联优化：内联小函数
    // 5. 寄存器分配优化：优化栈槽使用
    // TODO: 这些优化需要更复杂的分析，暂时作为占位符
    // 未来可以逐步实现这些优化
}

// 编译表达式（在 ZrParser_Expression_Compile.c 中实现）
// 这里只声明，不实现

// 编译语句（在 ZrParser_Statement_Compile.c 中实现）
// 这里只声明，不实现

// 编译函数声明
void compile_function_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_FUNCTION_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected function declaration", node->location);
        return;
    }

    SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
    
    // 保存当前函数节点（用于访问参数信息）
    cs->currentFunctionNode = node;
    
    // 检查是否是构造函数，设置 isInConstructor 标志
    // 普通函数不是构造函数
    cs->isInConstructor = ZR_FALSE;
    
    // 清空 const 参数列表（为新函数）
    cs->constParameters.length = 0;

    // 注册函数类型到类型环境（在编译函数体之前注册，以便递归调用）
    compiler_register_function_type_binding(cs, funcDecl);

    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldMaxStackSlotCount = cs->maxStackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    TZrSize oldInstructionLength = cs->instructions.length;
    TZrSize oldLocalVarLength = cs->localVars.length;
    TZrSize oldConstantLength = cs->constants.length;
    TZrSize oldClosureVarLength = cs->closureVars.length;
    TZrSize oldExecutionLocationLength = cs->executionLocations.length;
    TZrSize oldCatchClauseInfoLength = cs->catchClauseInfos.length;
    TZrSize oldExceptionHandlerInfoLength = cs->exceptionHandlerInfos.length;
    TZrSize oldTryContextLength = cs->tryContextStack.length;
    TZrBool oldIsInConstructor = cs->isInConstructor;
    SZrAstNode *oldFunctionNode = cs->currentFunctionNode;
    TZrSize oldConstLocalVarLength = cs->constLocalVars.length;
    TZrSize oldConstParameterLength = cs->constParameters.length;
    TZrInstruction *savedParentInstructions = ZR_NULL;
    SZrFunctionLocalVariable *savedParentLocalVars = ZR_NULL;
    SZrTypeValue *savedParentConstants = ZR_NULL;
    SZrFunctionClosureVariable *savedParentClosureVars = ZR_NULL;
    TZrSize savedParentInstructionsSize = oldInstructionLength * sizeof(TZrInstruction);
    TZrSize savedParentLocalVarsSize = oldLocalVarLength * sizeof(SZrFunctionLocalVariable);
    TZrSize savedParentConstantsSize = oldConstantLength * sizeof(SZrTypeValue);
    TZrSize savedParentClosureVarsSize = oldClosureVarLength * sizeof(SZrFunctionClosureVariable);

    if (savedParentInstructionsSize > 0) {
        savedParentInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentInstructionsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentInstructions == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for function declaration", node->location);
            return;
        }
        memcpy(savedParentInstructions, cs->instructions.head, savedParentInstructionsSize);
    }

    if (savedParentLocalVarsSize > 0) {
        savedParentLocalVars = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentLocalVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentLocalVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for function declaration", node->location);
            return;
        }
        memcpy(savedParentLocalVars, cs->localVars.head, savedParentLocalVarsSize);
    }

    if (savedParentConstantsSize > 0) {
        savedParentConstants = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentConstantsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentConstants == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for function declaration", node->location);
            return;
        }
        memcpy(savedParentConstants, cs->constants.head, savedParentConstantsSize);
    }

    if (savedParentClosureVarsSize > 0) {
        savedParentClosureVars = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentClosureVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentClosureVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentConstants != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for function declaration", node->location);
            return;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }

    // 创建新的函数对象
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create function object", node->location);
        if (savedParentInstructions != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }

    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;
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
        SZrScope *currentScope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
        if (currentScope != ZR_NULL) {
            currentScope->parentCompiler = parentCompiler;
        }
    }

    // 1. 编译参数列表
    TZrUInt32 parameterCount = 0;
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < funcDecl->params->count; i++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        // 如果是 const 参数，记录到 constParameters 数组
                        if (param->isConst) {
                            ZrCore_Array_Push(cs->state, &cs->constParameters, &paramName);
                        }
                        
                        // 注册参数类型到类型环境（用于类型推断）
                        if (cs->typeEnv != ZR_NULL) {
                            SZrInferredType paramType;
                            if (param->typeInfo != ZR_NULL) {
                                // 从类型注解推断类型
                                if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                    ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                    ZrParser_InferredType_Free(cs->state, &paramType);
                                }
                            } else {
                                // 没有类型注解，注册为对象类型（默认）
                                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                ZrParser_InferredType_Free(cs->state, &paramType);
                            }
                        }
                    }
                }
            }
        }
    }

    if (oldFunction != ZR_NULL && funcDecl->body != ZR_NULL) {
        SZrCompilerState parentCompilerSnapshot = {0};
        parentCompilerSnapshot.localVars.head = (TZrByte *)savedParentLocalVars;
        parentCompilerSnapshot.localVars.elementSize = sizeof(SZrFunctionLocalVariable);
        parentCompilerSnapshot.localVars.length = oldLocalVarLength;
        parentCompilerSnapshot.localVars.capacity = oldLocalVarLength;
        parentCompilerSnapshot.localVars.isValid = ZR_TRUE;
        parentCompilerSnapshot.closureVars.head = (TZrByte *)savedParentClosureVars;
        parentCompilerSnapshot.closureVars.elementSize = sizeof(SZrFunctionClosureVariable);
        parentCompilerSnapshot.closureVars.length = oldClosureVarLength;
        parentCompilerSnapshot.closureVars.capacity = oldClosureVarLength;
        parentCompilerSnapshot.closureVars.isValid = ZR_TRUE;
        ZrParser_ExternalVariables_Analyze(cs, funcDecl->body, &parentCompilerSnapshot);
    }

    // 检查是否有可变参数
    TZrBool hasVariableArguments = (funcDecl->args != ZR_NULL);

    // 2. 编译函数体
    if (funcDecl->body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, funcDecl->body);
    }

    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
            // 确保 length > 0 才访问数组
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TZrUInt16) resultSlot, (TZrInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                           (TZrInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    // 退出函数作用域
    exit_scope(cs);
    
    // 清空 const 变量跟踪（函数编译完成）
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunctionNode = ZR_NULL;

    if (cs->hasError) {
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
        }
        if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
            memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
            memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
            memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
            memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->currentFunction = oldFunction;
        cs->instructionCount = oldInstructionCount;
        cs->stackSlotCount = oldStackSlotCount;
        cs->maxStackSlotCount = oldMaxStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->instructions.length = oldInstructionLength;
        cs->localVars.length = oldLocalVarLength;
        cs->constants.length = oldConstantLength;
        cs->closureVars.length = oldClosureVarLength;
        cs->executionLocations.length = oldExecutionLocationLength;
        cs->catchClauseInfos.length = oldCatchClauseInfoLength;
        cs->exceptionHandlerInfos.length = oldExceptionHandlerInfoLength;
        cs->tryContextStack.length = oldTryContextLength;
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }

    // 3. 将编译结果复制到函数对象
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;

    // 复制指令列表
    // 使用 instructions.length 而不是 instructionCount，确保同步
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32) cs->instructions.length;
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
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32) cs->constants.length;
            // 同步 constantCount
            cs->constantCount = cs->constants.length;
        }
    }

    // 复制局部变量列表
    // 使用 localVars.length 而不是 localVarCount，确保同步
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32) cs->localVars.length;
            // 同步 localVarCount
            cs->localVarCount = cs->localVars.length;
        }
    }

    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *) ZrCore_Memory_RawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32) cs->closureVarCount;
        }
    }

    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32) cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16) parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32) node->location.end.line : 0;
    if (!compiler_copy_function_exception_metadata_slice(cs,
                                                         newFunc,
                                                         oldExecutionLocationLength,
                                                         oldCatchClauseInfoLength,
                                                         oldExceptionHandlerInfoLength,
                                                         node)) {
        ZrParser_Compiler_Error(cs, "Failed to copy function exception metadata", node->location);
    }
    
    // 设置函数名（函数名由函数自身持有）
    // 如果有名称，存储函数名；匿名函数（lambda）为 ZR_NULL
    if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
        newFunc->functionName = funcDecl->name->name;
    } else {
        newFunc->functionName = ZR_NULL;  // 匿名函数
    }

    if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
        memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
        memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
        memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
        memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;
    cs->executionLocations.length = oldExecutionLocationLength;
    cs->catchClauseInfos.length = oldCatchClauseInfoLength;
    cs->exceptionHandlerInfos.length = oldExceptionHandlerInfoLength;
    cs->tryContextStack.length = oldTryContextLength;
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = oldConstParameterLength;

    // 将新函数添加到子函数列表
    // 注意：这里需要在父编译器的上下文中操作
    // 函数名已经存储在函数对象中（newFunc->functionName），父函数只需要维护子函数列表的索引
    // 检查是否是顶层函数声明（脚本级别的函数声明，而不是嵌套函数）
    // 如果当前编译器是脚本级别（isScriptLevel == ZR_TRUE），则这是顶层函数声明
    if (oldFunction != ZR_NULL) {
        // 无论是嵌套函数还是顶层函数，都添加到父函数的 childFunctions 中
        // 这样它们都可以通过 GET_SUB_FUNCTION 访问
        // 子函数在 childFunctions 中的索引就是添加时的位置（cs->childFunctions.length）
        ZrCore_Array_Push(cs->state, &cs->childFunctions, &newFunc);
        
        // 更新函数名到索引的映射（用于编译时查找）
        SZrChildFunctionNameMap nameMap;
        nameMap.name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
        nameMap.childFunctionIndex = (TZrUInt32)(cs->childFunctions.length - 1);
        ZrCore_Array_Push(cs->state, &cs->childFunctionNameMap, &nameMap);
    } else {
        // 如果是顶层函数声明且没有父函数（不应该发生），将其保存到编译器状态
        // 这样 ZrParser_Compiler_Compile 可以返回它
        cs->topLevelFunction = newFunc;
    }

    if (oldFunction != ZR_NULL && funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
        SZrString *functionName = funcDecl->name->name;
        TZrUInt32 functionVarIndex = find_local_var(cs, functionName);
        SZrTypeValue funcValue;
        TZrUInt32 functionConstantIndex;
        TZrUInt32 closureSlot;
        TZrUInt32 closureVarCount = (TZrUInt32)newFunc->closureValueLength;

        if (functionVarIndex == (TZrUInt32)-1) {
            functionVarIndex = allocate_local_var(cs, functionName);
        }

        ZrCore_Value_InitAsRawObject(cs->state, &funcValue, ZR_CAST_RAW_OBJECT_AS_SUPER(newFunc));
        funcValue.type = ZR_VALUE_TYPE_FUNCTION;
        functionConstantIndex = add_constant(cs, &funcValue);
        closureSlot = allocate_stack_slot(cs);

        emit_instruction(
                cs,
                create_instruction_2(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE),
                                     (TZrUInt16)closureSlot,
                                     (TZrUInt16)functionConstantIndex,
                                     (TZrUInt16)closureVarCount));
        emit_instruction(
                cs,
                create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                     (TZrUInt16)functionVarIndex,
                                     (TZrInt32)closureSlot));

        if (cs->isScriptLevel) {
            SZrExportedVariable exportedVar;
            exportedVar.name = functionName;
            exportedVar.stackSlot = functionVarIndex;
            exportedVar.accessModifier = ZR_ACCESS_PUBLIC;
            ZrCore_Array_Push(cs->state, &cs->pubVariables, &exportedVar);
            ZrCore_Array_Push(cs->state, &cs->proVariables, &exportedVar);
        }
    }
}

// 编译测试声明
// 语法：%test("test_name") { ... }
// 测试体按真实脚本语义编译；通过/失败由宿主边界决定。
static void compile_test_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TEST_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected test declaration", node->location);
        return;
    }

    SZrTestDeclaration *testDecl = &node->data.testDeclaration;

    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldMaxStackSlotCount = cs->maxStackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    TZrSize oldInstructionLength = cs->instructions.length;
    TZrSize oldLocalVarLength = cs->localVars.length;
    TZrSize oldConstantLength = cs->constants.length;
    TZrSize oldClosureVarLength = cs->closureVars.length;
    TZrSize oldExecutionLocationLength = cs->executionLocations.length;
    TZrSize oldCatchClauseInfoLength = cs->catchClauseInfos.length;
    TZrSize oldExceptionHandlerInfoLength = cs->exceptionHandlerInfos.length;
    TZrSize oldTryContextLength = cs->tryContextStack.length;
    TZrBool oldIsInConstructor = cs->isInConstructor;
    SZrAstNode *oldFunctionNode = cs->currentFunctionNode;
    TZrSize oldConstLocalVarLength = cs->constLocalVars.length;
    TZrSize oldConstParameterLength = cs->constParameters.length;
    TZrInstruction *savedParentInstructions = ZR_NULL;
    SZrFunctionLocalVariable *savedParentLocalVars = ZR_NULL;
    SZrTypeValue *savedParentConstants = ZR_NULL;
    SZrFunctionClosureVariable *savedParentClosureVars = ZR_NULL;
    TZrSize savedParentInstructionsSize = oldInstructionLength * sizeof(TZrInstruction);
    TZrSize savedParentLocalVarsSize = oldLocalVarLength * sizeof(SZrFunctionLocalVariable);
    TZrSize savedParentConstantsSize = oldConstantLength * sizeof(SZrTypeValue);
    TZrSize savedParentClosureVarsSize = oldClosureVarLength * sizeof(SZrFunctionClosureVariable);

    if (savedParentInstructionsSize > 0) {
        savedParentInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentInstructionsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentInstructions == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for test declaration", node->location);
            return;
        }
        memcpy(savedParentInstructions, cs->instructions.head, savedParentInstructionsSize);
    }

    if (savedParentLocalVarsSize > 0) {
        savedParentLocalVars = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentLocalVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentLocalVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for test declaration", node->location);
            return;
        }
        memcpy(savedParentLocalVars, cs->localVars.head, savedParentLocalVarsSize);
    }

    if (savedParentConstantsSize > 0) {
        savedParentConstants = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentConstantsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentConstants == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for test declaration", node->location);
            return;
        }
        memcpy(savedParentConstants, cs->constants.head, savedParentConstantsSize);
    }

    if (savedParentClosureVarsSize > 0) {
        savedParentClosureVars = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentClosureVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentClosureVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentConstants != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for test declaration", node->location);
            return;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }

    // 创建新的测试函数对象
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create test function object", node->location);
        if (savedParentInstructions != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }

    // 测试函数应继承当前脚本已经编译出的脚本级初始化与局部变量布局。
    // 这样直接执行测试函数时，会先运行前置脚本初始化，再进入测试体。
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;

    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;

    // 进入函数作用域
    enter_scope(cs);

    // 测试函数没有参数
    // 计算参数数量和可变参数标志
    TZrUInt32 parameterCount = 0;
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
    TZrBool hasVariableArguments = (testDecl->args != ZR_NULL);

    // 直接编译测试体，未捕获异常由运行时和测试宿主按真实语义处理。
    if (testDecl->body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, testDecl->body);
        if (cs->hasError) {
            // 错误已在 ZrParser_Statement_Compile 中报告
        }
    }

    // 如果没有显式返回，按真实语义补一个 `return null`。
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式 null 返回。
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式 `return null`。
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction loadNullInst =
                                create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                     (TZrInt32) constantIndex);
                        emit_instruction(cs, loadNullInst);
                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式 null 返回。
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                           (TZrInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    // 退出函数作用域
    exit_scope(cs);

    if (cs->hasError) {
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
        }
        if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
            memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
            memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
            memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
            memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->currentFunction = oldFunction;
        cs->instructionCount = oldInstructionCount;
        cs->stackSlotCount = oldStackSlotCount;
        cs->maxStackSlotCount = oldMaxStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->instructions.length = oldInstructionLength;
        cs->localVars.length = oldLocalVarLength;
        cs->constants.length = oldConstantLength;
        cs->closureVars.length = oldClosureVarLength;
        cs->executionLocations.length = oldExecutionLocationLength;
        cs->catchClauseInfos.length = oldCatchClauseInfoLength;
        cs->exceptionHandlerInfos.length = oldExceptionHandlerInfoLength;
        cs->tryContextStack.length = oldTryContextLength;
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = oldConstLocalVarLength;
        cs->constParameters.length = oldConstParameterLength;
        return;
    }

    // 将编译结果复制到函数对象（参考 compile_function_declaration）
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;

    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32) cs->instructions.length;
            cs->instructionCount = cs->instructions.length;
        }
    }

    // 复制常量列表
    // 使用 constants.length 而不是 constantCount，确保同步
    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32) cs->constants.length;
            // 同步 constantCount
            cs->constantCount = cs->constants.length;
        }
    }

    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32) cs->localVars.length;
            cs->localVarCount = cs->localVars.length;
        }
    }

    // 复制闭包变量列表
    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *) ZrCore_Memory_RawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32) cs->closureVarCount;
        }
    }

    // 复制脚本级（主函数）的 childFunctions 到测试函数，使 GET_SUB_FUNCTION 能解析顶层函数
    if (cs->childFunctions.length > 0) {
        TZrSize childFuncSize = cs->childFunctions.length * sizeof(SZrFunction);
        newFunc->childFunctionList =
                (struct SZrFunction *) ZrCore_Memory_RawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->childFunctionList != ZR_NULL) {
            SZrFunction **srcArray = (SZrFunction **) cs->childFunctions.head;
            for (TZrSize i = 0; i < cs->childFunctions.length; i++) {
                if (srcArray[i] != ZR_NULL) {
                    newFunc->childFunctionList[i] = *srcArray[i];
                }
            }
            newFunc->childFunctionLength = (TZrUInt32) cs->childFunctions.length;
        }
    }

    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32) cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16) parameterCount;
    newFunc->hasVariableArguments = hasVariableArguments;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32) node->location.end.line : 0;
    if (!compiler_copy_function_exception_metadata_slice(cs,
                                                         newFunc,
                                                         oldExecutionLocationLength,
                                                         oldCatchClauseInfoLength,
                                                         oldExceptionHandlerInfoLength,
                                                         node)) {
        ZrParser_Compiler_Error(cs, "Failed to copy test exception metadata", node->location);
    }

    if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
        memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
        memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
        memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
        memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;
    cs->executionLocations.length = oldExecutionLocationLength;
    cs->catchClauseInfos.length = oldCatchClauseInfoLength;
    cs->exceptionHandlerInfos.length = oldExceptionHandlerInfoLength;
    cs->tryContextStack.length = oldTryContextLength;
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = oldConstLocalVarLength;
    cs->constParameters.length = oldConstParameterLength;

    // 将测试函数添加到测试函数列表
    ZrCore_Array_Push(cs->state, &cs->testFunctions, &newFunc);
}

// 辅助函数：从推断类型获取类型名称字符串
static SZrString *get_type_name_from_inferred_type(SZrCompilerState *cs, const SZrInferredType *inferredType) {
    if (cs == ZR_NULL || inferredType == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果有用户定义类型名，直接返回
    if (inferredType->typeName != ZR_NULL) {
        return inferredType->typeName;
    }
    
    // 根据基础类型返回对应的类型名称字符串
    const char *typeNameStr = ZR_NULL;
    switch (inferredType->baseType) {
        case ZR_VALUE_TYPE_INT8: typeNameStr = "i8"; break;
        case ZR_VALUE_TYPE_INT16: typeNameStr = "i16"; break;
        case ZR_VALUE_TYPE_INT32: typeNameStr = "i32"; break;
        case ZR_VALUE_TYPE_INT64: typeNameStr = "int"; break;
        case ZR_VALUE_TYPE_UINT8: typeNameStr = "u8"; break;
        case ZR_VALUE_TYPE_UINT16: typeNameStr = "u16"; break;
        case ZR_VALUE_TYPE_UINT32: typeNameStr = "u32"; break;
        case ZR_VALUE_TYPE_UINT64: typeNameStr = "uint"; break;
        case ZR_VALUE_TYPE_FLOAT: typeNameStr = "float"; break;
        case ZR_VALUE_TYPE_DOUBLE: typeNameStr = "double"; break;
        case ZR_VALUE_TYPE_BOOL: typeNameStr = "bool"; break;
        case ZR_VALUE_TYPE_STRING: typeNameStr = "string"; break;
        case ZR_VALUE_TYPE_OBJECT: typeNameStr = "object"; break;
        default: typeNameStr = "object"; break;
    }
    
    if (typeNameStr != ZR_NULL) {
        return ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)typeNameStr);
    }
    
    return ZR_NULL;
}

// 辅助函数：从类型节点提取类型名称字符串
static SZrString *extract_type_name_string(SZrCompilerState *cs, SZrType *type) {
    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *baseName = type->name->data.identifier.name;
        // 处理数组维度
        if (type->dimensions > 0) {
            // 构建数组类型名称，例如 "int[]" 或 "int[][]"
            TZrNativeString baseNameStr = ZrCore_String_GetNativeStringShort(baseName);
            if (baseNameStr == ZR_NULL) {
                baseNameStr = *ZrCore_String_GetNativeStringLong(baseName);
            }
            if (baseNameStr != ZR_NULL) {
                TZrSize baseLen = strlen(baseNameStr);
                TZrSize totalLen = baseLen + type->dimensions * 2; // 每个维度需要 "[]"
                char *arrayTypeName = (char *)ZrCore_Memory_RawMalloc(cs->state->global, totalLen + 1);
                if (arrayTypeName != ZR_NULL) {
                    strcpy(arrayTypeName, baseNameStr);
                    for (TZrInt32 i = 0; i < type->dimensions; i++) {
                        strcat(arrayTypeName, "[]");
                    }
                    SZrString *result = ZrCore_String_CreateFromNative(cs->state, arrayTypeName);
                    ZrCore_Memory_RawFree(cs->state->global, arrayTypeName, totalLen + 1);
                    return result;
                }
            }
        }
        return baseName;
    }
    
    // 处理泛型类型（如 Array<int>）
    if (type->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &type->name->data.genericType;
        if (genericType->name != ZR_NULL) {
            TZrNativeString genericNameStr = ZrCore_String_GetNativeStringShort(genericType->name->name);
            if (genericNameStr == ZR_NULL) {
                genericNameStr = *ZrCore_String_GetNativeStringLong(genericType->name->name);
            }
            if (genericNameStr != ZR_NULL) {
                // 构建泛型类型名称，例如 "Array<int>"
                TZrSize nameLen = strlen(genericNameStr);
                TZrSize totalLen = nameLen + 2; // "<" 和 ">"
                
                // 计算参数类型名称的总长度
                if (genericType->params != ZR_NULL && genericType->params->count > 0) {
                    for (TZrSize i = 0; i < genericType->params->count; i++) {
                        SZrAstNode *paramNode = genericType->params->nodes[i];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                            SZrString *paramTypeName = extract_type_name_string(cs, &paramNode->data.type);
                            if (paramTypeName != ZR_NULL) {
                                TZrNativeString paramStr = ZrCore_String_GetNativeStringShort(paramTypeName);
                                if (paramStr == ZR_NULL) {
                                    paramStr = *ZrCore_String_GetNativeStringLong(paramTypeName);
                                }
                                if (paramStr != ZR_NULL) {
                                    totalLen += strlen(paramStr);
                                    if (i > 0) {
                                        totalLen += 2; // ", "
                                    }
                                }
                            }
                        }
                    }
                }
                
                char *genericTypeName = (char *)ZrCore_Memory_RawMalloc(cs->state->global, totalLen + 1);
                if (genericTypeName != ZR_NULL) {
                    strcpy(genericTypeName, genericNameStr);
                    strcat(genericTypeName, "<");
                    if (genericType->params != ZR_NULL && genericType->params->count > 0) {
                        for (TZrSize i = 0; i < genericType->params->count; i++) {
                            if (i > 0) {
                                strcat(genericTypeName, ", ");
                            }
                            SZrAstNode *paramNode = genericType->params->nodes[i];
                            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                                SZrString *paramTypeName = extract_type_name_string(cs, &paramNode->data.type);
                                if (paramTypeName != ZR_NULL) {
                                    TZrNativeString paramStr = ZrCore_String_GetNativeStringShort(paramTypeName);
                                    if (paramStr == ZR_NULL) {
                                        paramStr = *ZrCore_String_GetNativeStringLong(paramTypeName);
                                    }
                                    if (paramStr != ZR_NULL) {
                                        strcat(genericTypeName, paramStr);
                                    }
                                }
                            }
                        }
                    }
                    strcat(genericTypeName, ">");
                    SZrString *result = ZrCore_String_CreateFromNative(cs->state, genericTypeName);
                    ZrCore_Memory_RawFree(cs->state->global, genericTypeName, totalLen + 1);
                    return result;
                }
            }
        }
        return ZR_NULL;
    }
    
    // 处理元组类型（如 (int, string)）
    if (type->name->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = &type->name->data.tupleType;
        if (tupleType->elements != ZR_NULL && tupleType->elements->count > 0) {
            TZrSize totalLen = 2; // "(" 和 ")"
            // 计算元素类型名称的总长度
            for (TZrSize i = 0; i < tupleType->elements->count; i++) {
                SZrAstNode *elemNode = tupleType->elements->nodes[i];
                if (elemNode != ZR_NULL && elemNode->type == ZR_AST_TYPE) {
                    SZrString *elemTypeName = extract_type_name_string(cs, &elemNode->data.type);
                    if (elemTypeName != ZR_NULL) {
                        TZrNativeString elemStr = ZrCore_String_GetNativeStringShort(elemTypeName);
                        if (elemStr == ZR_NULL) {
                            elemStr = *ZrCore_String_GetNativeStringLong(elemTypeName);
                        }
                        if (elemStr != ZR_NULL) {
                            totalLen += strlen(elemStr);
                            if (i > 0) {
                                totalLen += 2; // ", "
                            }
                        }
                    }
                }
            }
            
            char *tupleTypeName = (char *)ZrCore_Memory_RawMalloc(cs->state->global, totalLen + 1);
            if (tupleTypeName != ZR_NULL) {
                strcpy(tupleTypeName, "(");
                for (TZrSize i = 0; i < tupleType->elements->count; i++) {
                    if (i > 0) {
                        strcat(tupleTypeName, ", ");
                    }
                    SZrAstNode *elemNode = tupleType->elements->nodes[i];
                    if (elemNode != ZR_NULL && elemNode->type == ZR_AST_TYPE) {
                        SZrString *elemTypeName = extract_type_name_string(cs, &elemNode->data.type);
                        if (elemTypeName != ZR_NULL) {
                            TZrNativeString elemStr = ZrCore_String_GetNativeStringShort(elemTypeName);
                            if (elemStr == ZR_NULL) {
                                elemStr = *ZrCore_String_GetNativeStringLong(elemTypeName);
                            }
                            if (elemStr != ZR_NULL) {
                                strcat(tupleTypeName, elemStr);
                            }
                        }
                    }
                }
                strcat(tupleTypeName, ")");
                SZrString *result = ZrCore_String_CreateFromNative(cs->state, tupleTypeName);
                ZrCore_Memory_RawFree(cs->state->global, tupleTypeName, totalLen + 1);
                return result;
            }
        }
        return ZR_NULL;
    }
    
    return ZR_NULL;
}

// 辅助函数：计算类型的大小（字节数）
// 返回0表示未知类型，需要在运行时确定
static TZrUInt32 calculate_type_size(SZrCompilerState *cs, SZrType *type) {
    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL) {
        return 0;
    }
    
    // 处理数组类型
    if (type->dimensions > 0) {
        // 数组本身是指针类型，固定8字节
        // 注意：数组元素的实际大小在运行时确定，这里只返回指针大小
        return sizeof(TZrPtr); // 8 bytes
    }
    
    // 处理基本类型（通过类型名称字符串匹配）
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = type->name->data.identifier.name;
        if (typeName == ZR_NULL) {
            return 0;
        }
        
        TZrNativeString typeNameStr = ZrCore_String_GetNativeStringShort(typeName);
        if (typeNameStr == ZR_NULL) {
            return 0;
        }
        
        // 匹配基本类型名称
        if (strcmp(typeNameStr, "int") == 0 || strcmp(typeNameStr, "i64") == 0) {
            return sizeof(TZrInt64); // 8 bytes
        }
        if (strcmp(typeNameStr, "i8") == 0) {
            return sizeof(TZrInt8); // 1 byte
        }
        if (strcmp(typeNameStr, "i16") == 0) {
            return sizeof(TZrInt16); // 2 bytes
        }
        if (strcmp(typeNameStr, "i32") == 0) {
            return sizeof(TZrInt32); // 4 bytes
        }
        if (strcmp(typeNameStr, "uint") == 0 || strcmp(typeNameStr, "u64") == 0) {
            return sizeof(TZrUInt64); // 8 bytes
        }
        if (strcmp(typeNameStr, "u8") == 0) {
            return sizeof(TZrUInt8); // 1 byte
        }
        if (strcmp(typeNameStr, "u16") == 0) {
            return sizeof(TZrUInt16); // 2 bytes
        }
        if (strcmp(typeNameStr, "u32") == 0) {
            return sizeof(TZrUInt32); // 4 bytes
        }
        if (strcmp(typeNameStr, "float") == 0 || strcmp(typeNameStr, "f32") == 0) {
            return sizeof(TZrFloat32); // 4 bytes
        }
        if (strcmp(typeNameStr, "double") == 0 || strcmp(typeNameStr, "f64") == 0 || strcmp(typeNameStr, "f") == 0) {
            return sizeof(TZrDouble); // 8 bytes
        }
        if (strcmp(typeNameStr, "bool") == 0) {
            return sizeof(TZrBool); // 1 byte
        }
        if (strcmp(typeNameStr, "string") == 0 || strcmp(typeNameStr, "str") == 0) {
            return sizeof(TZrPtr); // 指针大小 8 bytes
        }
        if (strcmp(typeNameStr, "object") == 0 || strcmp(typeNameStr, "obj") == 0) {
            return sizeof(TZrPtr); // 指针大小 8 bytes
        }
        
        // 如果是自定义类型（struct/class），大小未知，需要在运行时确定
        // 返回0表示需要运行时计算
        return 0;
    }
    
    return 0;
}

// 辅助函数：应用对齐规则计算偏移量
static TZrUInt32 align_offset(TZrUInt32 offset, TZrUInt32 align) {
    // 对齐到align字节边界
    return ((offset + align - 1) / align) * align;
}

// 辅助函数：确定类型的对齐要求（字节）
static TZrUInt32 get_type_alignment(SZrCompilerState *cs, SZrType *type) {
    if (cs == ZR_NULL || type == ZR_NULL) {
        return ZR_ALIGN_SIZE; // 默认对齐
    }
    
    TZrUInt32 size = calculate_type_size(cs, type);
    if (size == 0) {
        return ZR_ALIGN_SIZE; // 未知类型，默认对齐
    }
    
    // 对齐要求通常是类型大小的幂（但不超过默认对齐）
    if (size <= 1) return 1;
    if (size <= 2) return 2;
    if (size <= 4) return 4;
    return ZR_ALIGN_SIZE; // 默认对齐
}

// 编译 struct 声明
static void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_STRUCT_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected struct declaration node", node->location);
        return;
    }
    
    SZrStructDeclaration *structDecl = &node->data.structDeclaration;
    
    // 获取类型名称
    if (structDecl->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Struct declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = structDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Struct name is null", node->location);
        return;
    }
    
    // 设置当前类型名称（用于成员字段 const 检查）
    SZrString *oldTypeName = cs->currentTypeName;
    SZrTypePrototypeInfo *oldTypePrototypeInfo = cs->currentTypePrototypeInfo;
    cs->currentTypeName = typeName;
    
    // 创建 prototype 信息结构
    SZrTypePrototypeInfo info;
    memset(&info, 0, sizeof(info));
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    info.accessModifier = structDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    
    // 初始化继承数组
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 2);
    
    // 处理继承关系
    if (structDecl->inherits != ZR_NULL && structDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < structDecl->inherits->count; i++) {
            SZrAstNode *inheritType = structDecl->inherits->nodes[i];
            if (inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE) {
                SZrType *type = &inheritType->data.type;
                // TODO: 提取类型名称（简化处理，只处理简单类型名）
                if (type->name != ZR_NULL && type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *inheritTypeName = type->name->data.identifier.name;
                    if (inheritTypeName != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
                    }
                }
            }
        }
    }
    
    // 初始化成员数组
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 16);
    cs->currentTypePrototypeInfo = &info;
    
    // 处理成员信息
    if (structDecl->members != ZR_NULL && structDecl->members->count > 0) {
        for (TZrSize i = 0; i < structDecl->members->count; i++) {
            SZrAstNode *member = structDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            SZrTypeMemberInfo memberInfo;
            // 初始化所有字段
            memberInfo.memberType = member->type;
            memberInfo.isStatic = ZR_FALSE;
            memberInfo.isConst = ZR_FALSE;
            memberInfo.isUsingManaged = ZR_FALSE;
            memberInfo.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.callsClose = ZR_FALSE;
            memberInfo.callsDestructor = ZR_FALSE;
            memberInfo.declarationOrder = (TZrUInt32)i;
            memberInfo.accessModifier = ZR_ACCESS_PRIVATE;
            memberInfo.name = ZR_NULL;
            memberInfo.fieldType = ZR_NULL;
            memberInfo.fieldTypeName = ZR_NULL;
            memberInfo.fieldOffset = 0;
            memberInfo.fieldSize = 0;
            memberInfo.compiledFunction = ZR_NULL;
            memberInfo.functionConstantIndex = 0;
            memberInfo.parameterCount = 0;
            memberInfo.metaType = 0; // ZR_META_ENUM_MAX表示非元方法
            memberInfo.isMetaMethod = ZR_FALSE;
            memberInfo.returnTypeName = ZR_NULL;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_STRUCT_FIELD: {
                    SZrStructField *field = &member->data.structField;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isStatic = field->isStatic;
                    memberInfo.isConst = field->isConst;
                    memberInfo.isUsingManaged = field->isUsingManaged;
                    if (field->name != ZR_NULL) {
                        memberInfo.name = field->name->name;
                    }

                    if (field->isStatic && field->isUsingManaged) {
                        ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "static using fields are not supported",
                                           member->location);
                        cs->currentTypeName = oldTypeName;
                        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                        return;
                    }
                    
                    // 处理字段类型信息
                    if (field->typeInfo != ZR_NULL) {
                        memberInfo.fieldType = field->typeInfo;
                        memberInfo.fieldTypeName = extract_type_name_string(cs, field->typeInfo);
                        memberInfo.ownershipQualifier = field->typeInfo->ownershipQualifier;
                        // 计算字段大小（用于偏移量计算）
                        memberInfo.fieldSize = calculate_type_size(cs, field->typeInfo);
                    } else if (field->init != ZR_NULL) {
                        // 没有类型注解，从初始值推断类型
                        SZrInferredType inferredType;
                        if (ZrParser_ExpressionType_Infer(cs, field->init, &inferredType)) {
                            memberInfo.fieldTypeName = get_type_name_from_inferred_type(cs, &inferredType);
                            memberInfo.ownershipQualifier = inferredType.ownershipQualifier;
                            // 根据推断类型计算字段大小
                            switch (inferredType.baseType) {
                                case ZR_VALUE_TYPE_INT8: memberInfo.fieldSize = sizeof(TZrInt8); break;
                                case ZR_VALUE_TYPE_INT16: memberInfo.fieldSize = sizeof(TZrInt16); break;
                                case ZR_VALUE_TYPE_INT32: memberInfo.fieldSize = sizeof(TZrInt32); break;
                                case ZR_VALUE_TYPE_INT64: memberInfo.fieldSize = sizeof(TZrInt64); break;
                                case ZR_VALUE_TYPE_UINT8: memberInfo.fieldSize = sizeof(TZrUInt8); break;
                                case ZR_VALUE_TYPE_UINT16: memberInfo.fieldSize = sizeof(TZrUInt16); break;
                                case ZR_VALUE_TYPE_UINT32: memberInfo.fieldSize = sizeof(TZrUInt32); break;
                                case ZR_VALUE_TYPE_UINT64: memberInfo.fieldSize = sizeof(TZrUInt64); break;
                                case ZR_VALUE_TYPE_FLOAT: memberInfo.fieldSize = sizeof(TZrFloat32); break;
                                case ZR_VALUE_TYPE_DOUBLE: memberInfo.fieldSize = sizeof(TZrDouble); break;
                                case ZR_VALUE_TYPE_BOOL: memberInfo.fieldSize = sizeof(TZrBool); break;
                                case ZR_VALUE_TYPE_STRING:
                                case ZR_VALUE_TYPE_OBJECT:
                                default:
                                    memberInfo.fieldSize = sizeof(TZrPtr); // 指针大小
                                    break;
                            }
                            ZrParser_InferredType_Free(cs->state, &inferredType);
                        } else {
                            // 类型推断失败，默认为object类型
                            memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                            memberInfo.fieldSize = sizeof(TZrPtr);
                        }
                    } else {
                        // 没有类型注解和初始值，默认为object类型（8字节指针）
                        memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                        memberInfo.fieldSize = sizeof(TZrPtr);
                    }

                    if (memberInfo.isUsingManaged &&
                        memberInfo.ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_WEAK) {
                        memberInfo.callsClose = ZR_TRUE;
                        memberInfo.callsDestructor = ZR_TRUE;
                    }
                    
                    // 字段偏移量将在所有字段收集后统一计算
                    // 这里先设置为0，后续会根据字段顺序和对齐规则计算
                    break;
                }
                case ZR_AST_STRUCT_METHOD: {
                    SZrStructMethod *method = &member->data.structMethod;
                    memberInfo.accessModifier = method->access;
                    memberInfo.isStatic = method->isStatic;
                    memberInfo.receiverQualifier = method->receiverQualifier;
                    if (method->name != ZR_NULL) {
                        memberInfo.name = method->name->name;
                    }
                    // 处理返回类型信息
                    if (method->returnType != ZR_NULL) {
                        memberInfo.returnTypeName = extract_type_name_string(cs, method->returnType);
                    } else {
                        memberInfo.returnTypeName = ZR_NULL; // 无返回类型（void）
                    }
                    // 方法信息（函数引用等）将在方法编译后设置
                    // 需要将方法编译为函数并存储函数引用索引
                    // 注意：方法编译应该在prototype创建时进行，这里只记录方法信息
                    // 实际的函数编译和引用索引设置需要在方法编译完成后进行
                    if (method->params != ZR_NULL) {
                        memberInfo.parameterCount = (TZrUInt32)method->params->count;
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_STRUCT_META_FUNCTION: {
                    SZrStructMetaFunction *metaFunc = &member->data.structMetaFunction;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                        
                        // 提取元方法类型（如@constructor -> ZR_META_CONSTRUCTOR）
                        // 通过元方法名称匹配
                        TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
                        if (metaName != ZR_NULL) {
                            if (strcmp(metaName, "constructor") == 0) {
                                memberInfo.metaType = ZR_META_CONSTRUCTOR;
                            } else if (strcmp(metaName, "destructor") == 0) {
                                memberInfo.metaType = ZR_META_DESTRUCTOR;
                            } else if (strcmp(metaName, "add") == 0) {
                                memberInfo.metaType = ZR_META_ADD;
                            } else if (strcmp(metaName, "toString") == 0) {
                                memberInfo.metaType = ZR_META_TO_STRING;
                            }
                            // TODO: 添加更多元方法类型匹配
                            memberInfo.isMetaMethod = ZR_TRUE;
                        }
                    }
                    // 处理返回类型信息
                    if (metaFunc->returnType != ZR_NULL) {
                        memberInfo.returnTypeName = extract_type_name_string(cs, metaFunc->returnType);
                    } else {
                        memberInfo.returnTypeName = ZR_NULL; // 无返回类型（void）
                    }
                    // 元方法的函数引用索引将在编译后设置
                    // TODO: 需要将元方法编译为函数并存储函数引用索引
                    if (metaFunc->params != ZR_NULL) {
                        memberInfo.parameterCount = (TZrUInt32)metaFunc->params->count;
                    }
                    break;
                }
                default:
                    // 忽略未知成员类型
                    continue;
            }
            
            if (memberInfo.name != ZR_NULL) {
                ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
            }
        }
    }
    
    // 计算struct字段偏移量（仅对非静态字段）
    TZrUInt32 currentOffset = 0;
    for (TZrSize i = 0; i < info.members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info.members, i);
        if (memberInfo == ZR_NULL) {
            continue;
        }
        
        // 只处理非静态字段
        if (memberInfo->memberType == ZR_AST_STRUCT_FIELD && !memberInfo->isStatic) {
            // 获取字段对齐要求
            TZrUInt32 align = ZR_ALIGN_SIZE; // 默认对齐
            if (memberInfo->fieldType != ZR_NULL) {
                align = get_type_alignment(cs, memberInfo->fieldType);
            }
            
            // 应用对齐
            currentOffset = align_offset(currentOffset, align);
            
            // 设置字段偏移量
            memberInfo->fieldOffset = currentOffset;
            
            // 增加偏移量（如果字段大小为0，表示未知类型，使用默认大小）
            TZrUInt32 fieldSize = memberInfo->fieldSize;
            if (fieldSize == 0) {
                fieldSize = ZR_ALIGN_SIZE; // 默认大小
            }
            currentOffset += fieldSize;
        }
    }
    
    // 将 prototype 信息添加到数组
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    
    // 注册类型名称到类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }
    
    // 编译元函数（特别是构造函数）
    if (structDecl->members != ZR_NULL && structDecl->members->count > 0) {
        for (TZrSize i = 0; i < structDecl->members->count; i++) {
            SZrAstNode *member = structDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            if (member->type == ZR_AST_STRUCT_META_FUNCTION) {
                SZrStructMetaFunction *metaFunc = &member->data.structMetaFunction;
                if (metaFunc->meta != ZR_NULL) {
                    TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
                    if (metaName != ZR_NULL) {
                        EZrMetaType metaType = 0;
                        if (strcmp(metaName, "constructor") == 0) {
                            metaType = ZR_META_CONSTRUCTOR;
                        } else if (strcmp(metaName, "destructor") == 0) {
                            metaType = ZR_META_DESTRUCTOR;
                        } else if (strcmp(metaName, "add") == 0) {
                            metaType = ZR_META_ADD;
                        } else if (strcmp(metaName, "toString") == 0) {
                            metaType = ZR_META_TO_STRING;
                        }
                        
                        if (metaType != 0) {
                            compile_meta_function(cs, member, metaType);
                        }
                    }
                }
            }
        }
    }
    
    // 恢复当前类型名称
    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
}

static TZrBool extern_compiler_has_registered_type(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void extern_compiler_register_struct_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) {
    SZrStructDeclaration *structDecl;
    SZrTypePrototypeInfo info;
    TZrUInt32 currentOffset = 0;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_STRUCT_DECLARATION || cs->hasError) {
        return;
    }

    structDecl = &declarationNode->data.structDeclaration;
    if (structDecl->name == ZR_NULL || structDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, structDecl->name->name)) {
        return;
    }

    memset(&info, 0, sizeof(info));
    info.name = structDecl->name->name;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    info.accessModifier = structDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 2);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 2);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 8);

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            SZrAstNode *member = structDecl->members->nodes[index];
            SZrTypeMemberInfo memberInfo;
            TZrInt64 explicitOffset = 0;

            if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD ||
                member->data.structField.name == ZR_NULL || member->data.structField.name->name == ZR_NULL) {
                continue;
            }

            memset(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.memberType = ZR_AST_STRUCT_FIELD;
            memberInfo.name = member->data.structField.name->name;
            memberInfo.accessModifier = member->data.structField.access;
            memberInfo.isStatic = member->data.structField.isStatic;
            memberInfo.isConst = member->data.structField.isConst;
            memberInfo.fieldType = member->data.structField.typeInfo;
            memberInfo.fieldTypeName = extract_type_name_string(cs, member->data.structField.typeInfo);
            memberInfo.fieldSize = calculate_type_size(cs, member->data.structField.typeInfo);
            if (memberInfo.fieldSize == 0) {
                memberInfo.fieldSize = ZR_ALIGN_SIZE;
            }

            if (extern_compiler_decorators_get_int_arg(member->data.structField.decorators, "offset", &explicitOffset)) {
                memberInfo.fieldOffset = (TZrUInt32)explicitOffset;
                currentOffset = memberInfo.fieldOffset + memberInfo.fieldSize;
            } else {
                currentOffset = align_offset(currentOffset, get_type_alignment(cs, member->data.structField.typeInfo));
                memberInfo.fieldOffset = currentOffset;
                currentOffset += memberInfo.fieldSize;
            }

            ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
        }
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

static void extern_compiler_register_enum_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) {
    SZrEnumDeclaration *enumDecl;
    SZrTypePrototypeInfo info;
    SZrString *underlyingName = ZR_NULL;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_ENUM_DECLARATION || cs->hasError) {
        return;
    }

    enumDecl = &declarationNode->data.enumDeclaration;
    if (enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, enumDecl->name->name)) {
        return;
    }

    memset(&info, 0, sizeof(info));
    info.name = enumDecl->name->name;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
    info.accessModifier = enumDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 4);

    if (enumDecl->baseType != ZR_NULL) {
        underlyingName = extract_type_name_string(cs, enumDecl->baseType);
    }
    if (underlyingName == ZR_NULL) {
        underlyingName = extern_compiler_decorators_get_string_arg(enumDecl->decorators, "underlying");
    }
    if (underlyingName == ZR_NULL) {
        underlyingName = ZrCore_String_CreateFromNative(cs->state, "i32");
    }
    info.enumValueTypeName = underlyingName;

    if (enumDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < enumDecl->members->count; index++) {
            SZrAstNode *memberNode = enumDecl->members->nodes[index];
            SZrTypeMemberInfo memberInfo;

            if (memberNode == ZR_NULL || memberNode->type != ZR_AST_ENUM_MEMBER ||
                memberNode->data.enumMember.name == ZR_NULL || memberNode->data.enumMember.name->name == ZR_NULL) {
                continue;
            }

            memset(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.memberType = ZR_AST_CLASS_FIELD;
            memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
            memberInfo.isStatic = ZR_TRUE;
            memberInfo.name = memberNode->data.enumMember.name->name;
            memberInfo.fieldTypeName = info.name;
            ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
        }
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

static void compiler_register_extern_block_bindings(SZrCompilerState *cs, SZrExternBlock *externBlock) {
    if (cs == ZR_NULL || externBlock == ZR_NULL || externBlock->declarations == ZR_NULL || cs->hasError) {
        return;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        if (declaration->type == ZR_AST_STRUCT_DECLARATION) {
            extern_compiler_register_struct_prototype(cs, declaration);
        } else if (declaration->type == ZR_AST_ENUM_DECLARATION) {
            extern_compiler_register_enum_prototype(cs, declaration);
        }
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
                compiler_register_extern_function_type_binding_to_env(
                        cs,
                        cs->typeEnv,
                        &declaration->data.externFunctionDeclaration);
                compiler_register_extern_function_type_binding_to_env(
                        cs,
                        cs->compileTimeTypeEnv,
                        &declaration->data.externFunctionDeclaration);
                break;
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL) {
                    SZrString *delegateName = declaration->data.externDelegateDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, delegateName, delegateName);
                    compiler_register_named_value_binding_to_env(cs,
                                                                 cs->compileTimeTypeEnv,
                                                                 delegateName,
                                                                 delegateName);
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL) {
                    SZrString *structName = declaration->data.structDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, structName, structName);
                    compiler_register_named_value_binding_to_env(cs, cs->compileTimeTypeEnv, structName, structName);
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL) {
                    SZrString *enumName = declaration->data.enumDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, enumName, enumName);
                    compiler_register_named_value_binding_to_env(cs, cs->compileTimeTypeEnv, enumName, enumName);
                }
                break;
            default:
                break;
        }
    }
}

void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError || cs->externBindingsPredeclared) {
        return;
    }

    for (TZrSize index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];
        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK) {
            continue;
        }
        compiler_register_extern_block_bindings(cs, &statement->data.externBlock);
        if (cs->hasError) {
            return;
        }
    }

    cs->externBindingsPredeclared = ZR_TRUE;
}

static void compile_extern_block_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrExternBlock *externBlock;
    SZrString *libraryName;
    SZrString *ffiModuleName;
    SZrString *loadLibraryName;
    SZrString *getSymbolName;
    TZrUInt32 ffiModuleSlot = (TZrUInt32)-1;
    TZrUInt32 librarySlot = (TZrUInt32)-1;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_EXTERN_BLOCK) {
        ZrParser_Compiler_Error(cs, "Expected extern block declaration", node->location);
        return;
    }

    externBlock = &node->data.externBlock;
    if (externBlock->libraryName == ZR_NULL || externBlock->libraryName->type != ZR_AST_STRING_LITERAL) {
        ZrParser_Compiler_Error(cs, "extern block requires a string library specifier", node->location);
        return;
    }

    libraryName = externBlock->libraryName->data.stringLiteral.value;
    ffiModuleName = ZrCore_String_CreateFromNative(cs->state, "zr.ffi");
    loadLibraryName = ZrCore_String_CreateFromNative(cs->state, "loadLibrary");
    getSymbolName = ZrCore_String_CreateFromNative(cs->state, "getSymbol");
    if (libraryName == ZR_NULL || ffiModuleName == ZR_NULL || loadLibraryName == ZR_NULL || getSymbolName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to allocate extern ffi helper strings", node->location);
        return;
    }

    if (!cs->externBindingsPredeclared) {
        compiler_register_extern_block_bindings(cs, externBlock);
        if (cs->hasError) {
            return;
        }
    }

    if (externBlock->declarations == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        SZrString *bindingName = ZR_NULL;
        SZrTypeValue descriptorValue;

        if (declaration == ZR_NULL) {
            continue;
        }

        ZrCore_Value_ResetAsNull(&descriptorValue);
        switch (declaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.externDelegateDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_build_delegate_descriptor_value(cs, externBlock, declaration, ZR_TRUE, &descriptorValue)) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, &descriptorValue);
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.structDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_build_struct_descriptor_value(cs, externBlock, declaration, &descriptorValue)) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, &descriptorValue);
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.enumDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_build_enum_descriptor_value(cs, declaration, &descriptorValue)) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, &descriptorValue);
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                break;
            case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
                SZrExternFunctionDeclaration *functionDecl = &declaration->data.externFunctionDeclaration;
                SZrString *entryName = functionDecl->name != ZR_NULL ? functionDecl->name->name : ZR_NULL;
                SZrString *entryOverride = extern_compiler_decorators_get_string_arg(functionDecl->decorators, "entry");
                TZrUInt32 localSlot;
                SZrTypeValue symbolArguments[2];

                if (functionDecl->name == ZR_NULL || functionDecl->name->name == ZR_NULL) {
                    ZrParser_Compiler_Error(cs, "extern function declaration is missing a name", declaration->location);
                    return;
                }
                if (entryOverride != ZR_NULL) {
                    entryName = entryOverride;
                }

                if (!extern_compiler_build_signature_descriptor_value(cs,
                                                                     externBlock,
                                                                     functionDecl->params,
                                                                     functionDecl->args,
                                                                     functionDecl->returnType,
                                                                     functionDecl->decorators,
                                                                     ZR_FALSE,
                                                                     declaration->location,
                                                                     &symbolArguments[1])) {
                    return;
                }
                ZrCore_Value_InitAsRawObject(cs->state, &symbolArguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(entryName));
                symbolArguments[0].type = ZR_VALUE_TYPE_STRING;

                if (ffiModuleSlot == (TZrUInt32)-1) {
                    SZrString *hiddenFfiName = create_hidden_extern_local_name(cs, "ffi");
                    SZrString *hiddenLibraryName = create_hidden_extern_local_name(cs, "library");
                    SZrTypeValue loadArguments[1];
                    if (hiddenFfiName == ZR_NULL || hiddenLibraryName == ZR_NULL) {
                        ZrParser_Compiler_Error(cs, "failed to allocate hidden extern locals", declaration->location);
                        return;
                    }

                    ffiModuleSlot = allocate_local_var(cs, hiddenFfiName);
                    if (!extern_compiler_emit_import_module_to_local(cs, ffiModuleName, ffiModuleSlot, declaration->location)) {
                        return;
                    }

                    librarySlot = allocate_local_var(cs, hiddenLibraryName);
                    ZrCore_Value_InitAsRawObject(cs->state, &loadArguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(libraryName));
                    loadArguments[0].type = ZR_VALUE_TYPE_STRING;
                    if (!extern_compiler_emit_module_function_call_to_local(cs,
                                                                           ffiModuleSlot,
                                                                           loadLibraryName,
                                                                           loadArguments,
                                                                           1,
                                                                           librarySlot,
                                                                           declaration->location)) {
                        return;
                    }
                }

                localSlot = allocate_local_var(cs, functionDecl->name->name);
                if (!extern_compiler_emit_method_call_to_local(cs,
                                                               librarySlot,
                                                               getSymbolName,
                                                               symbolArguments,
                                                               2,
                                                               localSlot,
                                                               declaration->location)) {
                    return;
                }
                break;
            }
            default:
                break;
        }

        if (cs->hasError) {
            return;
        }
    }
}

void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node) {
    compile_extern_block_declaration(cs, node);
}

// 编译元函数（@constructor, @destructor 等）
static void compile_meta_function(SZrCompilerState *cs, SZrAstNode *node, EZrMetaType metaType) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrStructMetaFunction *metaFunc = ZR_NULL;
    SZrClassMetaFunction *classMetaFunc = ZR_NULL;
    
    if (node->type == ZR_AST_STRUCT_META_FUNCTION) {
        metaFunc = &node->data.structMetaFunction;
    } else if (node->type == ZR_AST_CLASS_META_FUNCTION) {
        classMetaFunc = &node->data.classMetaFunction;
    } else {
        ZrParser_Compiler_Error(cs, "Expected meta function node", node->location);
        return;
    }
    
    // 保存当前编译器状态
    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldMaxStackSlotCount = cs->maxStackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    TZrBool oldIsInConstructor = cs->isInConstructor;
    
    // 设置 isInConstructor 标志（如果是构造函数）
    cs->isInConstructor = (metaType == ZR_META_CONSTRUCTOR) ? ZR_TRUE : ZR_FALSE;
    cs->currentFunctionNode = node;
    
    // 清空 const 变量跟踪（为新函数）
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    
    // 创建新的函数对象
    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create meta function object", node->location);
        cs->isInConstructor = oldIsInConstructor;
        return;
    }
    
    // 重置编译器状态（为新函数）
    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;
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
    
    // 编译参数列表
    TZrUInt32 parameterCount = 0;
    SZrAstNodeArray *params = ZR_NULL;
    if (metaFunc != ZR_NULL) {
        params = metaFunc->params;
    } else if (classMetaFunc != ZR_NULL) {
        params = classMetaFunc->params;
    }
    
    if (params != ZR_NULL) {
        for (TZrSize i = 0; i < params->count; i++) {
            SZrAstNode *paramNode = params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    if (paramName != ZR_NULL) {
                        // 分配参数槽位
                        allocate_local_var(cs, paramName);
                        parameterCount++;
                        
                        // 如果是 const 参数，记录到 constParameters 数组
                        if (param->isConst) {
                            ZrCore_Array_Push(cs->state, &cs->constParameters, &paramName);
                        }
                        
                        // 注册参数类型到类型环境
                        if (cs->typeEnv != ZR_NULL) {
                            SZrInferredType paramType;
                            if (param->typeInfo != ZR_NULL) {
                                if (ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                                    ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                    ZrParser_InferredType_Free(cs->state, &paramType);
                                }
                            } else {
                                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                                ZrParser_InferredType_Free(cs->state, &paramType);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 编译函数体
    SZrAstNode *body = ZR_NULL;
    if (metaFunc != ZR_NULL) {
        body = metaFunc->body;
    } else if (classMetaFunc != ZR_NULL) {
        body = classMetaFunc->body;
    }
    
    if (body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, body);
    }
    
    // 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);
            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            if (cs->instructions.length > 0) {
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TZrUInt16) resultSlot, (TZrInt32) constantIndex);
                        emit_instruction(cs, inst);
                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            }
        }
    }
    
    // 退出函数作用域
    exit_scope(cs);
    
    // 检查是否有编译错误
    if (cs->hasError) {
        // 如果有错误，释放函数对象并恢复状态
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
            cs->currentFunction = ZR_NULL;
        }
        cs->currentFunction = oldFunction;
        cs->instructionCount = oldInstructionCount;
        cs->stackSlotCount = oldStackSlotCount;
        cs->maxStackSlotCount = oldMaxStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = ZR_NULL;
        cs->constLocalVars.length = 0;
        cs->constParameters.length = 0;
        cs->instructions.length = 0;
        cs->localVars.length = 0;
        cs->constants.length = 0;
        cs->closureVars.length = 0;
        return;
    }
    
    // 清空 const 变量跟踪
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;
    cs->currentFunctionNode = ZR_NULL;
    
    // 将编译结果复制到函数对象（类似 compile_function_declaration）
    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;
    
    // 复制指令列表
    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32) cs->instructions.length;
        }
    }
    
    // 复制常量列表
    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32) cs->constants.length;
        }
    }
    
    // 复制局部变量列表
    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32) cs->localVars.length;
        }
    }
    
    // 设置函数元数据
    newFunc->stackSize = (TZrUInt32) cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16) parameterCount;
    newFunc->hasVariableArguments = ZR_FALSE;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32) node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32) node->location.end.line : 0;
    
    // 将新函数添加到子函数列表
    // 注意：对于元函数，即使 oldFunction 为 ZR_NULL（在类型声明编译时），
    // 也应该将函数添加到当前函数的 childFunctions 中（如果存在）
    // 但如果没有父函数，元函数可能无法正确管理，这里暂时不添加到列表
    // TODO: 需要更完善的元函数管理机制
    if (oldFunction != ZR_NULL) {
        ZrCore_Array_Push(cs->state, &cs->childFunctions, &newFunc);
    } else {
        // 如果没有父函数，元函数暂时不添加到列表
        // 注意：这可能导致元函数无法被正确访问，需要后续完善
        // 但是，如果没有父函数，我们需要释放这个函数对象，避免内存泄漏
        // 因为元函数在类型声明编译时不应该被保留（它们会在运行时通过类型原型创建）
        if (newFunc != ZR_NULL) {
            ZrCore_Function_Free(cs->state, newFunc);
            newFunc = ZR_NULL;
        }
    }
    
    // 恢复编译器状态
    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->isInConstructor = oldIsInConstructor;
    
    // 清空数组（但保留已分配的内存）
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;
}

static SZrFunction *compile_class_member_function(SZrCompilerState *cs, SZrAstNode *node,
                                                  SZrString *superTypeName,
                                                  TZrBool injectThis, TZrUInt32 *outParameterCount) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return ZR_NULL;
    }

    SZrAstNodeArray *params = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    SZrString *functionName = ZR_NULL;
    TZrBool isConstructor = ZR_FALSE;
    SZrString *manualParamName = ZR_NULL;
    SZrType *manualParamType = ZR_NULL;

    if (node->type == ZR_AST_CLASS_METHOD) {
        SZrClassMethod *method = &node->data.classMethod;
        params = method->params;
        body = method->body;
        functionName = method->name != ZR_NULL ? method->name->name : ZR_NULL;
    } else if (node->type == ZR_AST_CLASS_META_FUNCTION) {
        SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
        params = metaFunc->params;
        body = metaFunc->body;
        functionName = metaFunc->meta != ZR_NULL ? metaFunc->meta->name : ZR_NULL;
        if (metaFunc->meta != ZR_NULL) {
            TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
            if (metaName != ZR_NULL && strcmp(metaName, "constructor") == 0) {
                isConstructor = ZR_TRUE;
            }
        }
    } else if (node->type == ZR_AST_CLASS_PROPERTY) {
        SZrClassProperty *property = &node->data.classProperty;
        if (property->modifier == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Class property modifier is null", node->location);
            return ZR_NULL;
        }

        if (property->modifier->type == ZR_AST_PROPERTY_GET) {
            SZrPropertyGet *getter = &property->modifier->data.propertyGet;
            body = getter->body;
            if (getter->name != ZR_NULL) {
                functionName = create_hidden_property_accessor_name(cs, getter->name->name, ZR_FALSE);
            }
        } else if (property->modifier->type == ZR_AST_PROPERTY_SET) {
            SZrPropertySet *setter = &property->modifier->data.propertySet;
            body = setter->body;
            if (setter->name != ZR_NULL) {
                functionName = create_hidden_property_accessor_name(cs, setter->name->name, ZR_TRUE);
            }
            if (setter->param != ZR_NULL) {
                manualParamName = setter->param->name;
            }
            manualParamType = setter->targetType;
        } else {
            ZrParser_Compiler_Error(cs, "Unsupported class property modifier", node->location);
            return ZR_NULL;
        }
    } else {
        ZrParser_Compiler_Error(cs, "Expected class method, class property or class meta function", node->location);
        return ZR_NULL;
    }

    SZrFunction *oldFunction = cs->currentFunction;
    TZrSize oldInstructionCount = cs->instructionCount;
    TZrSize oldStackSlotCount = cs->stackSlotCount;
    TZrSize oldMaxStackSlotCount = cs->maxStackSlotCount;
    TZrSize oldLocalVarCount = cs->localVarCount;
    TZrSize oldConstantCount = cs->constantCount;
    TZrSize oldClosureVarCount = cs->closureVarCount;
    TZrSize oldInstructionLength = cs->instructions.length;
    TZrSize oldLocalVarLength = cs->localVars.length;
    TZrSize oldConstantLength = cs->constants.length;
    TZrSize oldClosureVarLength = cs->closureVars.length;
    TZrBool oldIsInConstructor = cs->isInConstructor;
    SZrAstNode *oldFunctionNode = cs->currentFunctionNode;
    TZrInstruction *savedParentInstructions = ZR_NULL;
    SZrFunctionLocalVariable *savedParentLocalVars = ZR_NULL;
    SZrTypeValue *savedParentConstants = ZR_NULL;
    SZrFunctionClosureVariable *savedParentClosureVars = ZR_NULL;
    TZrSize savedParentInstructionsSize = oldInstructionLength * sizeof(TZrInstruction);
    TZrSize savedParentLocalVarsSize = oldLocalVarLength * sizeof(SZrFunctionLocalVariable);
    TZrSize savedParentConstantsSize = oldConstantLength * sizeof(SZrTypeValue);
    TZrSize savedParentClosureVarsSize = oldClosureVarLength * sizeof(SZrFunctionClosureVariable);

    if (savedParentInstructionsSize > 0) {
        savedParentInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentInstructionsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentInstructions == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Failed to backup parent instructions for class member compilation", node->location);
            return ZR_NULL;
        }
        memcpy(savedParentInstructions, cs->instructions.head, savedParentInstructionsSize);
    }

    if (savedParentLocalVarsSize > 0) {
        savedParentLocalVars = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentLocalVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentLocalVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent locals for class member compilation", node->location);
            return ZR_NULL;
        }
        memcpy(savedParentLocalVars, cs->localVars.head, savedParentLocalVarsSize);
    }

    if (savedParentConstantsSize > 0) {
        savedParentConstants = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentConstantsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentConstants == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent constants for class member compilation", node->location);
            return ZR_NULL;
        }
        memcpy(savedParentConstants, cs->constants.head, savedParentConstantsSize);
    }

    if (savedParentClosureVarsSize > 0) {
        savedParentClosureVars = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                cs->state->global, savedParentClosureVarsSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (savedParentClosureVars == ZR_NULL) {
            if (savedParentInstructions != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentLocalVars != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            if (savedParentConstants != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrParser_Compiler_Error(cs, "Failed to backup parent closures for class member compilation", node->location);
            return ZR_NULL;
        }
        memcpy(savedParentClosureVars, cs->closureVars.head, savedParentClosureVarsSize);
    }

    cs->isInConstructor = isConstructor ? ZR_TRUE : ZR_FALSE;
    cs->currentFunctionNode = node;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;

    cs->currentFunction = ZrCore_Function_New(cs->state);
    if (cs->currentFunction == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create class member function object", node->location);
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        return ZR_NULL;
    }

    cs->instructionCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;
    cs->localVarCount = 0;
    cs->constantCount = 0;
    cs->closureVarCount = 0;
    cs->instructions.length = 0;
    cs->localVars.length = 0;
    cs->constants.length = 0;
    cs->closureVars.length = 0;

    enter_scope(cs);

    TZrUInt32 parameterCount = 0;
    if (injectThis) {
        EZrOwnershipQualifier thisOwnershipQualifier =
                get_implicit_this_ownership_qualifier(get_member_receiver_qualifier(node));
        SZrString *thisName = ZrCore_String_CreateFromNative(cs->state, "this");
        if (thisName != ZR_NULL) {
            allocate_local_var(cs, thisName);
            parameterCount++;
            if (cs->typeEnv != ZR_NULL) {
                SZrInferredType thisType;
                if (cs->currentTypeName != ZR_NULL) {
                    ZrParser_InferredType_InitFull(cs->state, &thisType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE,
                                           cs->currentTypeName);
                } else {
                    ZrParser_InferredType_Init(cs->state, &thisType, ZR_VALUE_TYPE_OBJECT);
                }
                thisType.ownershipQualifier = thisOwnershipQualifier;
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, thisName, &thisType);
                ZrParser_InferredType_Free(cs->state, &thisType);
            }
        }
    }

    if (params != ZR_NULL) {
        for (TZrSize i = 0; i < params->count; i++) {
            SZrAstNode *paramNode = params->nodes[i];
            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                SZrParameter *param = &paramNode->data.parameter;
                if (param->name != ZR_NULL && param->name->name != ZR_NULL) {
                    SZrString *paramName = param->name->name;
                    allocate_local_var(cs, paramName);
                    parameterCount++;

                    if (param->isConst) {
                        ZrCore_Array_Push(cs->state, &cs->constParameters, &paramName);
                    }

                    if (cs->typeEnv != ZR_NULL) {
                        SZrInferredType paramType;
                        if (param->typeInfo != ZR_NULL &&
                            ZrParser_AstTypeToInferredType_Convert(cs, param->typeInfo, &paramType)) {
                            ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                            ZrParser_InferredType_Free(cs->state, &paramType);
                        } else {
                            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                            ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, paramName, &paramType);
                            ZrParser_InferredType_Free(cs->state, &paramType);
                        }
                    }
                }
            }
        }
    }

    if (manualParamName != ZR_NULL) {
        allocate_local_var(cs, manualParamName);
        parameterCount++;

        if (cs->typeEnv != ZR_NULL) {
            SZrInferredType paramType;
            if (manualParamType != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, manualParamType, &paramType)) {
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, manualParamName, &paramType);
                ZrParser_InferredType_Free(cs->state, &paramType);
            } else {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, manualParamName, &paramType);
                ZrParser_InferredType_Free(cs->state, &paramType);
            }
        }
    }

    if (!cs->hasError && isConstructor && injectThis && node->type == ZR_AST_CLASS_META_FUNCTION &&
        superTypeName != ZR_NULL) {
        SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
        if (metaFunc->hasSuperCall && compiler_type_has_constructor(cs, superTypeName)) {
            emit_super_constructor_call(cs, superTypeName, metaFunc->superArgs);
        }
    }

    if (body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, body);
    }

    if (!cs->hasError) {
        if (cs->instructions.length == 0) {
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot,
                                                       (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            TZrInstruction *lastInst =
                    (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
            if (lastInst != ZR_NULL &&
                (EZrInstructionCode)lastInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)resultSlot,
                                                           (TZrInt32)constantIndex);
                emit_instruction(cs, inst);
                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16)resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }

    exit_scope(cs);

    if (cs->hasError) {
        if (cs->currentFunction != ZR_NULL) {
            ZrCore_Function_Free(cs->state, cs->currentFunction);
        }
        if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
            memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
            memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentConstants != ZR_NULL) {
            memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
            memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
            ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        cs->currentFunction = oldFunction;
        cs->instructionCount = oldInstructionCount;
        cs->stackSlotCount = oldStackSlotCount;
        cs->maxStackSlotCount = oldMaxStackSlotCount;
        cs->localVarCount = oldLocalVarCount;
        cs->constantCount = oldConstantCount;
        cs->closureVarCount = oldClosureVarCount;
        cs->instructions.length = oldInstructionLength;
        cs->localVars.length = oldLocalVarLength;
        cs->constants.length = oldConstantLength;
        cs->closureVars.length = oldClosureVarLength;
        cs->isInConstructor = oldIsInConstructor;
        cs->currentFunctionNode = oldFunctionNode;
        cs->constLocalVars.length = 0;
        cs->constParameters.length = 0;
        return ZR_NULL;
    }

    SZrFunction *newFunc = cs->currentFunction;
    SZrGlobalState *global = cs->state->global;

    if (cs->instructions.length > 0) {
        TZrSize instSize = cs->instructions.length * sizeof(TZrInstruction);
        newFunc->instructionsList =
                (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->instructionsList != ZR_NULL) {
            memcpy(newFunc->instructionsList, cs->instructions.head, instSize);
            newFunc->instructionsLength = (TZrUInt32)cs->instructions.length;
        }
    }

    if (cs->constants.length > 0) {
        TZrSize constSize = cs->constants.length * sizeof(SZrTypeValue);
        newFunc->constantValueList =
                (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->constantValueList != ZR_NULL) {
            memcpy(newFunc->constantValueList, cs->constants.head, constSize);
            newFunc->constantValueLength = (TZrUInt32)cs->constants.length;
        }
    }

    if (cs->localVars.length > 0) {
        TZrSize localVarSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
        newFunc->localVariableList = (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->localVariableList != ZR_NULL) {
            memcpy(newFunc->localVariableList, cs->localVars.head, localVarSize);
            newFunc->localVariableLength = (TZrUInt32)cs->localVars.length;
        }
    }

    if (cs->closureVarCount > 0) {
        TZrSize closureVarSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
        newFunc->closureValueList = (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newFunc->closureValueList != ZR_NULL) {
            memcpy(newFunc->closureValueList, cs->closureVars.head, closureVarSize);
            newFunc->closureValueLength = (TZrUInt32)cs->closureVarCount;
        }
    }

    newFunc->stackSize = (TZrUInt32)cs->maxStackSlotCount;
    newFunc->parameterCount = (TZrUInt16)parameterCount;
    newFunc->hasVariableArguments = ZR_FALSE;
    newFunc->lineInSourceStart = (node->location.start.line > 0) ? (TZrUInt32)node->location.start.line : 0;
    newFunc->lineInSourceEnd = (node->location.end.line > 0) ? (TZrUInt32)node->location.end.line : 0;
    newFunc->functionName = functionName;

    if (savedParentInstructions != ZR_NULL && savedParentInstructionsSize > 0) {
        memcpy(cs->instructions.head, savedParentInstructions, savedParentInstructionsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentInstructions, savedParentInstructionsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (savedParentLocalVars != ZR_NULL && savedParentLocalVarsSize > 0) {
        memcpy(cs->localVars.head, savedParentLocalVars, savedParentLocalVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentLocalVars, savedParentLocalVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (savedParentConstants != ZR_NULL && savedParentConstantsSize > 0) {
        memcpy(cs->constants.head, savedParentConstants, savedParentConstantsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentConstants, savedParentConstantsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    if (savedParentClosureVars != ZR_NULL && savedParentClosureVarsSize > 0) {
        memcpy(cs->closureVars.head, savedParentClosureVars, savedParentClosureVarsSize);
        ZrCore_Memory_RawFreeWithType(cs->state->global, savedParentClosureVars, savedParentClosureVarsSize,
                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    cs->currentFunction = oldFunction;
    cs->instructionCount = oldInstructionCount;
    cs->stackSlotCount = oldStackSlotCount;
    cs->maxStackSlotCount = oldMaxStackSlotCount;
    cs->localVarCount = oldLocalVarCount;
    cs->constantCount = oldConstantCount;
    cs->closureVarCount = oldClosureVarCount;
    cs->instructions.length = oldInstructionLength;
    cs->localVars.length = oldLocalVarLength;
    cs->constants.length = oldConstantLength;
    cs->closureVars.length = oldClosureVarLength;
    cs->isInConstructor = oldIsInConstructor;
    cs->currentFunctionNode = oldFunctionNode;
    cs->constLocalVars.length = 0;
    cs->constParameters.length = 0;

    if (outParameterCount != ZR_NULL) {
        *outParameterCount = parameterCount;
    }

    return newFunc;
}

// 编译 class 声明
static void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CLASS_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected class declaration node", node->location);
        return;
    }
    
    SZrClassDeclaration *classDecl = &node->data.classDeclaration;
    
    // 获取类型名称
    if (classDecl->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Class declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = classDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Class name is null", node->location);
        return;
    }
    
    // 设置当前类型名称（用于成员字段 const 检查）
    SZrString *oldTypeName = cs->currentTypeName;
    SZrTypePrototypeInfo *oldTypePrototypeInfo = cs->currentTypePrototypeInfo;
    cs->currentTypeName = typeName;
    
    // 创建 prototype 信息结构
    SZrTypePrototypeInfo info;
    memset(&info, 0, sizeof(info));
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    info.accessModifier = classDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    
    // 初始化继承数组
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 2);
    SZrString *primarySuperTypeName = ZR_NULL;
    // 处理继承关系
    if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
            SZrAstNode *inheritType = classDecl->inherits->nodes[i];
            SZrString *inheritTypeName = extract_simple_type_name_from_type_node(inheritType);
            if (inheritTypeName != ZR_NULL) {
                if (primarySuperTypeName == ZR_NULL) {
                    primarySuperTypeName = inheritTypeName;
                }
                ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
            }
        }
    }
    
    // 初始化成员数组
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 16);
    cs->currentTypePrototypeInfo = &info;
    
    // 处理成员信息
    if (classDecl->members != ZR_NULL && classDecl->members->count > 0) {
        for (TZrSize i = 0; i < classDecl->members->count; i++) {
            SZrAstNode *member = classDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            SZrTypeMemberInfo memberInfo;
            // 初始化所有字段
            memberInfo.memberType = member->type;
            memberInfo.isStatic = ZR_FALSE;
            memberInfo.isConst = ZR_FALSE;
            memberInfo.isUsingManaged = ZR_FALSE;
            memberInfo.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.callsClose = ZR_FALSE;
            memberInfo.callsDestructor = ZR_FALSE;
            memberInfo.declarationOrder = (TZrUInt32)i;
            memberInfo.accessModifier = ZR_ACCESS_PRIVATE;
            memberInfo.name = ZR_NULL;
            memberInfo.fieldType = ZR_NULL;
            memberInfo.fieldTypeName = ZR_NULL;
            memberInfo.fieldOffset = 0;
            memberInfo.fieldSize = 0;
            memberInfo.compiledFunction = ZR_NULL;
            memberInfo.functionConstantIndex = 0;
            memberInfo.parameterCount = 0;
            memberInfo.metaType = 0;
            memberInfo.isMetaMethod = ZR_FALSE;
            memberInfo.returnTypeName = ZR_NULL;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_CLASS_FIELD: {
                    SZrClassField *field = &member->data.classField;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isStatic = field->isStatic; // class字段也有isStatic
                    memberInfo.isConst = field->isConst;
                    memberInfo.isUsingManaged = field->isUsingManaged;
                    if (field->name != ZR_NULL) {
                        memberInfo.name = field->name->name;
                    }

                    if (field->isStatic && field->isUsingManaged) {
                        ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "static using fields are not supported",
                                           member->location);
                        cs->currentTypeName = oldTypeName;
                        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                        return;
                    }
                    // 处理字段类型信息
                    if (field->typeInfo != ZR_NULL) {
                        memberInfo.fieldType = field->typeInfo;
                        memberInfo.fieldTypeName = extract_type_name_string(cs, field->typeInfo);
                        memberInfo.ownershipQualifier = field->typeInfo->ownershipQualifier;
                        memberInfo.fieldSize = calculate_type_size(cs, field->typeInfo);
                    } else if (field->init != ZR_NULL) {
                        // 没有类型注解，从初始值推断类型
                        SZrInferredType inferredType;
                        if (ZrParser_ExpressionType_Infer(cs, field->init, &inferredType)) {
                            memberInfo.fieldTypeName = get_type_name_from_inferred_type(cs, &inferredType);
                            memberInfo.ownershipQualifier = inferredType.ownershipQualifier;
                            // 根据推断类型计算字段大小
                            switch (inferredType.baseType) {
                                case ZR_VALUE_TYPE_INT8: memberInfo.fieldSize = sizeof(TZrInt8); break;
                                case ZR_VALUE_TYPE_INT16: memberInfo.fieldSize = sizeof(TZrInt16); break;
                                case ZR_VALUE_TYPE_INT32: memberInfo.fieldSize = sizeof(TZrInt32); break;
                                case ZR_VALUE_TYPE_INT64: memberInfo.fieldSize = sizeof(TZrInt64); break;
                                case ZR_VALUE_TYPE_UINT8: memberInfo.fieldSize = sizeof(TZrUInt8); break;
                                case ZR_VALUE_TYPE_UINT16: memberInfo.fieldSize = sizeof(TZrUInt16); break;
                                case ZR_VALUE_TYPE_UINT32: memberInfo.fieldSize = sizeof(TZrUInt32); break;
                                case ZR_VALUE_TYPE_UINT64: memberInfo.fieldSize = sizeof(TZrUInt64); break;
                                case ZR_VALUE_TYPE_FLOAT: memberInfo.fieldSize = sizeof(TZrFloat32); break;
                                case ZR_VALUE_TYPE_DOUBLE: memberInfo.fieldSize = sizeof(TZrDouble); break;
                                case ZR_VALUE_TYPE_BOOL: memberInfo.fieldSize = sizeof(TZrBool); break;
                                case ZR_VALUE_TYPE_STRING:
                                case ZR_VALUE_TYPE_OBJECT:
                                default:
                                    memberInfo.fieldSize = sizeof(TZrPtr); // 指针大小
                                    break;
                            }
                            ZrParser_InferredType_Free(cs->state, &inferredType);
                        } else {
                            // 类型推断失败，默认为object类型
                            memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                            memberInfo.fieldSize = sizeof(TZrPtr);
                        }
                    } else {
                        // 没有类型注解和初始值，默认为object类型（8字节指针）
                        memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                        memberInfo.fieldSize = sizeof(TZrPtr);
                    }

                    if (memberInfo.isUsingManaged &&
                        memberInfo.ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_WEAK) {
                        memberInfo.callsClose = ZR_TRUE;
                        memberInfo.callsDestructor = ZR_TRUE;
                    }
                    break;
                }
                case ZR_AST_CLASS_METHOD: {
                    SZrClassMethod *method = &member->data.classMethod;
                    memberInfo.accessModifier = method->access;
                    memberInfo.isStatic = method->isStatic;
                    memberInfo.receiverQualifier = method->receiverQualifier;
                    if (method->name != ZR_NULL) {
                        memberInfo.name = method->name->name;
                    }
                    // 处理返回类型信息
                    if (method->returnType != ZR_NULL) {
                        memberInfo.returnTypeName = extract_type_name_string(cs, method->returnType);
                    } else {
                        memberInfo.returnTypeName = ZR_NULL; // 无返回类型（void）
                    }
                    {
                        TZrUInt32 compiledParameterCount = 0;
                        SZrFunction *compiledMethod =
                                compile_class_member_function(cs, member, primarySuperTypeName,
                                                              !method->isStatic, &compiledParameterCount);
                        if (compiledMethod == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMethod));
                        memberInfo.compiledFunction = compiledMethod;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        memberInfo.parameterCount = compiledParameterCount;
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_CLASS_PROPERTY: {
                    SZrClassProperty *property = &member->data.classProperty;
                    memberInfo.accessModifier = property->access;
                    memberInfo.isStatic = property->isStatic;
                    memberInfo.memberType = ZR_AST_CLASS_METHOD;
                    if (property->modifier != ZR_NULL) {
                        TZrUInt32 compiledParameterCount = 0;
                        if (property->modifier->type == ZR_AST_PROPERTY_GET) {
                            SZrPropertyGet *getter = &property->modifier->data.propertyGet;
                            if (getter->name != ZR_NULL) {
                                memberInfo.name =
                                        create_hidden_property_accessor_name(cs, getter->name->name, ZR_FALSE);
                            }
                            if (getter->targetType != ZR_NULL) {
                                memberInfo.returnTypeName = extract_type_name_string(cs, getter->targetType);
                            }
                        } else if (property->modifier->type == ZR_AST_PROPERTY_SET) {
                            SZrPropertySet *setter = &property->modifier->data.propertySet;
                            if (setter->name != ZR_NULL) {
                                memberInfo.name =
                                        create_hidden_property_accessor_name(cs, setter->name->name, ZR_TRUE);
                            }
                            memberInfo.returnTypeName = ZR_NULL;
                        }

                        SZrFunction *compiledProperty =
                                compile_class_member_function(cs, member, primarySuperTypeName, !property->isStatic,
                                                              &compiledParameterCount);
                        if (compiledProperty == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledProperty));
                        memberInfo.compiledFunction = compiledProperty;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        memberInfo.parameterCount = compiledParameterCount;
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_CLASS_META_FUNCTION: {
                    SZrClassMetaFunction *metaFunc = &member->data.classMetaFunction;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                        
                        // 提取元方法类型
                        TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
                        if (metaName != ZR_NULL) {
                            if (strcmp(metaName, "constructor") == 0) {
                                memberInfo.metaType = ZR_META_CONSTRUCTOR;
                            } else if (strcmp(metaName, "destructor") == 0) {
                                memberInfo.metaType = ZR_META_DESTRUCTOR;
                            } else if (strcmp(metaName, "add") == 0) {
                                memberInfo.metaType = ZR_META_ADD;
                            } else if (strcmp(metaName, "toString") == 0) {
                                memberInfo.metaType = ZR_META_TO_STRING;
                            }
                            // TODO: 添加更多元方法类型匹配
                            memberInfo.isMetaMethod = ZR_TRUE;
                        }
                    }
                    // 处理返回类型信息
                    if (metaFunc->returnType != ZR_NULL) {
                        memberInfo.returnTypeName = extract_type_name_string(cs, metaFunc->returnType);
                    } else {
                        memberInfo.returnTypeName = ZR_NULL; // 无返回类型（void）
                    }
                    if (memberInfo.isMetaMethod) {
                        TZrUInt32 compiledParameterCount = 0;
                        SZrFunction *compiledMeta =
                                compile_class_member_function(cs, member, primarySuperTypeName,
                                                              !metaFunc->isStatic, &compiledParameterCount);
                        if (compiledMeta == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMeta));
                        memberInfo.compiledFunction = compiledMeta;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        memberInfo.parameterCount = compiledParameterCount;
                    }
                    break;
                }
                default:
                    // 忽略未知成员类型
                    continue;
            }
            
            if (memberInfo.name != ZR_NULL) {
                ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
            }
        }
    }
    
    // 将 prototype 信息添加到数组
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    
    // 注册类型名称到类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }

    emit_class_static_field_initializers(cs, node);
    if (cs->hasError) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        return;
    }
    
    // 恢复当前类型名称
    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
}

// 序列化的prototype信息结构（紧凑二进制格式）
typedef struct SZrSerializedPrototypeInfo {
    // 基本信息
    TZrUInt32 nameStringIndex;              // 类型名称字符串在常量池中的索引
    TZrUInt32 type;                         // EZrObjectPrototypeType
    TZrUInt32 accessModifier;               // EZrAccessModifier
    
    // 继承关系
    TZrUInt32 inheritsCount;                // 继承类型数量
    TZrUInt32 *inheritStringIndices;        // 继承类型名称字符串索引数组（动态分配）
    
    // 成员信息
    TZrUInt32 membersCount;                 // 成员数量
    // 成员数据紧随其后（动态数组）
} SZrSerializedPrototypeInfo;

// 序列化的成员信息结构（紧凑二进制格式）
typedef struct SZrSerializedMemberInfo {
    TZrUInt32 memberType;                   // EZrAstNodeType
    TZrUInt32 nameStringIndex;              // 成员名称字符串在常量池中的索引（如果为0表示无名）
    TZrUInt32 accessModifier;               // EZrAccessModifier
    TZrUInt32 isStatic;                     // TZrBool (0或1)
    
    // 字段特定信息（仅当memberType为STRUCT_FIELD或CLASS_FIELD时有效）
    TZrUInt32 fieldTypeNameStringIndex;     // 字段类型名称字符串索引（如果为0表示无类型名）
    TZrUInt32 fieldOffset;                  // 字段偏移量
    TZrUInt32 fieldSize;                    // 字段大小
    
    // 方法特定信息（仅当memberType为METHOD或META_FUNCTION时有效）
    TZrUInt32 isMetaMethod;                 // TZrBool (0或1)
    TZrUInt32 metaType;                     // EZrMetaType
    TZrUInt32 functionConstantIndex;        // 函数在常量池中的索引
    TZrUInt32 parameterCount;               // 参数数量
} SZrSerializedMemberInfo;

// 编译时使用的prototype二进制格式。
// 显式 pack(1) 以确保与 runtime 解析协议一致。
#pragma pack(push, 1)
typedef struct SZrCompiledPrototypeInfo {
    TZrUInt32 nameStringIndex;
    TZrUInt32 type;
    TZrUInt32 accessModifier;
    TZrUInt32 inheritsCount;
    TZrUInt32 membersCount;
} SZrCompiledPrototypeInfo;

typedef struct SZrCompiledMemberInfo {
    TZrUInt32 memberType;
    TZrUInt32 nameStringIndex;
    TZrUInt32 accessModifier;
    TZrUInt32 isStatic;
    TZrUInt32 isConst;
    TZrUInt32 fieldTypeNameStringIndex;
    TZrUInt32 fieldOffset;
    TZrUInt32 fieldSize;
    TZrUInt32 isMetaMethod;
    TZrUInt32 metaType;
    TZrUInt32 functionConstantIndex;
    TZrUInt32 parameterCount;
    TZrUInt32 returnTypeNameStringIndex;
    TZrUInt32 isUsingManaged;
    TZrUInt32 ownershipQualifier;
    TZrUInt32 callsClose;
    TZrUInt32 callsDestructor;
    TZrUInt32 declarationOrder;
} SZrCompiledMemberInfo;
#pragma pack(pop)

// 将prototype信息序列化为二进制数据（不存储到常量池）
// 编译时使用C原生结构，避免创建VM对象，提高编译速度
// 运行时（module.c）会从 function->prototypeData 读取并创建VM对象
// 返回：ZR_TRUE 表示成功，ZR_FALSE 表示失败
// 注意：outData 指向的内存需要调用者释放
static TZrBool serialize_prototype_info_to_binary(SZrCompilerState *cs, SZrTypePrototypeInfo *info, 
                                                 TZrByte **outData, TZrSize *outSize) {
    if (cs == ZR_NULL || info == ZR_NULL || info->name == ZR_NULL || outData == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 注意：为了保持格式兼容，我们仍然使用字符串索引
    // 但这些索引现在指向 prototype 数据内部的字符串表，而不是常量池
    // TODO: 为了简化实现，我们暂时仍然使用常量池索引，但后续会改为内部字符串表
    
    // 1. 使用C原生结构收集数据，避免创建VM对象
    // 先将所有字符串添加到常量池，获取索引（临时方案，后续改为内部字符串表）
    SZrTypeValue nameValue;
    ZrCore_Value_InitAsRawObject(cs->state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(info->name));
    nameValue.type = ZR_VALUE_TYPE_STRING;
    TZrUInt32 nameStringIndex = add_constant(cs, &nameValue);
    
    // 2. 添加继承类型名称字符串到常量池
    TZrUInt32 *inheritStringIndices = ZR_NULL;
    TZrUInt32 inheritsCount = (TZrUInt32)info->inherits.length;
    if (inheritsCount > 0) {
        inheritStringIndices = (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, inheritsCount * sizeof(TZrUInt32));
        if (inheritStringIndices == ZR_NULL) {
            return ZR_FALSE;
        }
        
        for (TZrSize i = 0; i < info->inherits.length; i++) {
            SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&info->inherits, i);
            if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                SZrTypeValue inheritValue;
                ZrCore_Value_InitAsRawObject(cs->state, &inheritValue, ZR_CAST_RAW_OBJECT_AS_SUPER(*inheritTypeNamePtr));
                inheritValue.type = ZR_VALUE_TYPE_STRING;
                inheritStringIndices[i] = add_constant(cs, &inheritValue);
            } else {
                inheritStringIndices[i] = 0;
            }
        }
    }
    
    // 3. 计算序列化数据大小（使用C原生结构）
    TZrUInt32 membersCount = (TZrUInt32)info->members.length;
    TZrSize serializedSize = sizeof(SZrCompiledPrototypeInfo) + 
                             (inheritsCount > 0 ? inheritsCount * sizeof(TZrUInt32) : 0) +
                             membersCount * sizeof(SZrCompiledMemberInfo);
    
    // 4. 分配序列化数据缓冲区（C原生内存，非VM对象）
    TZrByte *serializedData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, serializedSize);
    if (serializedData == ZR_NULL) {
        if (inheritStringIndices != ZR_NULL) {
            ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
        }
        return ZR_FALSE;
    }
    
    // 5. 填充序列化数据（使用C原生结构，避免指针，所有数据直接嵌入）
    SZrCompiledPrototypeInfo *protoInfo = (SZrCompiledPrototypeInfo *)serializedData;
    protoInfo->nameStringIndex = nameStringIndex;
    protoInfo->type = (TZrUInt32)info->type;
    protoInfo->accessModifier = (TZrUInt32)info->accessModifier;
    protoInfo->inheritsCount = inheritsCount;
    protoInfo->membersCount = membersCount;
    
    // 复制继承类型索引数组到序列化数据中（紧跟在结构体后面）
    TZrUInt32 *embeddedInheritIndices = (TZrUInt32 *)(serializedData + sizeof(SZrCompiledPrototypeInfo));
    if (inheritsCount > 0 && inheritStringIndices != ZR_NULL) {
        memcpy(embeddedInheritIndices, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
        ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
    }
    
    // 序列化成员信息（紧跟在继承数组后面）
    SZrCompiledMemberInfo *members = (SZrCompiledMemberInfo *)(serializedData + 
                                                                 sizeof(SZrCompiledPrototypeInfo) +
                                                                 inheritsCount * sizeof(TZrUInt32));
    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo == ZR_NULL) {
            continue;
        }
        
        SZrCompiledMemberInfo *compiledMember = &members[i];
        compiledMember->memberType = (TZrUInt32)memberInfo->memberType;
        compiledMember->accessModifier = (TZrUInt32)memberInfo->accessModifier;
        compiledMember->isStatic = memberInfo->isStatic ? ZR_TRUE : ZR_FALSE;
        compiledMember->isConst = memberInfo->isConst ? ZR_TRUE : ZR_FALSE;
        compiledMember->isUsingManaged = memberInfo->isUsingManaged ? ZR_TRUE : ZR_FALSE;
        compiledMember->ownershipQualifier = (TZrUInt32)memberInfo->ownershipQualifier;
        compiledMember->callsClose = memberInfo->callsClose ? ZR_TRUE : ZR_FALSE;
        compiledMember->callsDestructor = memberInfo->callsDestructor ? ZR_TRUE : ZR_FALSE;
        compiledMember->declarationOrder = memberInfo->declarationOrder;
        
        // 添加成员名称字符串到常量池（临时方案）
        if (memberInfo->name != ZR_NULL) {
            SZrTypeValue memberNameValue;
            ZrCore_Value_InitAsRawObject(cs->state, &memberNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->name));
            memberNameValue.type = ZR_VALUE_TYPE_STRING;
            compiledMember->nameStringIndex = add_constant(cs, &memberNameValue);
        } else {
            compiledMember->nameStringIndex = 0;
        }
        
        // 字段特定信息
        if (memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) {
            if (memberInfo->fieldTypeName != ZR_NULL) {
                SZrTypeValue fieldTypeNameValue;
                ZrCore_Value_InitAsRawObject(cs->state, &fieldTypeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->fieldTypeName));
                fieldTypeNameValue.type = ZR_VALUE_TYPE_STRING;
                compiledMember->fieldTypeNameStringIndex = add_constant(cs, &fieldTypeNameValue);
            } else {
                compiledMember->fieldTypeNameStringIndex = 0;
            }
            compiledMember->fieldOffset = memberInfo->fieldOffset;
            compiledMember->fieldSize = memberInfo->fieldSize;
            // 方法字段清零
            compiledMember->isMetaMethod = ZR_FALSE;
            compiledMember->metaType = 0;
            compiledMember->functionConstantIndex = 0;
            compiledMember->parameterCount = 0;
            compiledMember->returnTypeNameStringIndex = 0;
        }
        
        // 方法特定信息
        if (memberInfo->memberType == ZR_AST_STRUCT_METHOD || 
            memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
            memberInfo->memberType == ZR_AST_CLASS_METHOD ||
            memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION) {
            compiledMember->isMetaMethod = memberInfo->isMetaMethod ? ZR_TRUE : ZR_FALSE;
            compiledMember->metaType = (TZrUInt32)memberInfo->metaType;
            if (memberInfo->compiledFunction != ZR_NULL) {
                SZrTypeValue functionValue;
                ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                       ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->compiledFunction));
                compiledMember->functionConstantIndex = add_constant(cs, &functionValue);
            } else {
                compiledMember->functionConstantIndex = memberInfo->functionConstantIndex;
            }
            compiledMember->parameterCount = memberInfo->parameterCount;
            // 处理返回类型名称
            if (memberInfo->returnTypeName != ZR_NULL) {
                SZrTypeValue returnTypeNameValue;
                ZrCore_Value_InitAsRawObject(cs->state, &returnTypeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->returnTypeName));
                returnTypeNameValue.type = ZR_VALUE_TYPE_STRING;
                compiledMember->returnTypeNameStringIndex = add_constant(cs, &returnTypeNameValue);
            } else {
                compiledMember->returnTypeNameStringIndex = 0; // 无返回类型（void）
            }
            // 字段字段清零
            compiledMember->fieldTypeNameStringIndex = 0;
            compiledMember->fieldOffset = 0;
            compiledMember->fieldSize = 0;
        } else {
            // 非方法成员，返回类型字段清零
            compiledMember->returnTypeNameStringIndex = 0;
        }
    }
    
    // 6. 返回序列化数据（不存储到常量池）
    *outData = serializedData;
    *outSize = serializedSize;
    
    return ZR_TRUE;
}

// 编译脚本
static void compile_script(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_SCRIPT) {
        ZrParser_Compiler_Error(cs, "Expected script node", node->location);
        return;
    }

    SZrScript *script = &node->data.script;

    if (cs->semanticContext != ZR_NULL) {
        ZrParser_SemanticContext_Reset(cs->semanticContext);
        if (cs->hirModule != ZR_NULL) {
            ZrParser_HirModule_Free(cs->state, cs->hirModule);
            cs->hirModule = ZR_NULL;
        }
        cs->hirModule = ZrParser_HirModule_New(cs->state, cs->semanticContext, node);
    }

    // 设置脚本级别标志（用于区分脚本级变量和函数内变量）
    cs->isScriptLevel = ZR_TRUE;
    
    // 保存脚本 AST 引用（用于类型查找）
    cs->scriptAst = node;

    // 1. 编译模块声明（如果有）
    if (script->moduleName != ZR_NULL) {
        // 处理模块声明（注册模块到全局模块表）
        // 注意：模块注册在运行时进行，编译器只需要记录模块名称
        // 模块名称可以通过entry function的常量池或元数据存储
        // 运行时加载模块时会创建模块对象并注册到全局模块注册表
        // TODO: 这里暂时不生成特殊指令，模块注册在模块加载时自动进行
    }

    // 2. 首先收集并执行所有编译期声明
    if (script->statements != ZR_NULL) {
        enter_scope(cs);

        // 第一遍：收集并执行编译期声明
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL && stmt->type == ZR_AST_COMPILE_TIME_DECLARATION) {
                // 执行编译期声明
                ZrParser_CompileTimeDeclaration_Execute(cs, stmt);
                
                // 如果遇到致命错误，停止编译
                if (cs->hasFatalError) {
                    printf("  Fatal compile-time error encountered, stopping compilation\n");
                    return;
                }
            }
        }

        if (cs->hasCompileTimeError) {
            cs->hasError = ZR_TRUE;
            return;
        }

        ZrParser_Compiler_PredeclareExternBindings(cs, script->statements);
        if (cs->hasError) {
            return;
        }

        ZrParser_Compiler_PredeclareFunctionBindings(cs, script->statements);
        if (cs->hasError) {
            return;
        }

        // 第二遍：编译运行时代码
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL) {
                // 跳过编译期声明（已在第一遍执行）
                if (stmt->type == ZR_AST_COMPILE_TIME_DECLARATION) {
                    continue;
                }
                
                // 根据语句类型编译
                switch (stmt->type) {
                    case ZR_AST_FUNCTION_DECLARATION:
                        compile_function_declaration(cs, stmt);
                        break;
                    case ZR_AST_VARIABLE_DECLARATION:
                    case ZR_AST_EXPRESSION_STATEMENT:
                    case ZR_AST_USING_STATEMENT:
                    case ZR_AST_BLOCK:
                    case ZR_AST_RETURN_STATEMENT:
                    case ZR_AST_THROW_STATEMENT:
                    case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
                    case ZR_AST_IF_EXPRESSION:
                    case ZR_AST_WHILE_LOOP:
                    case ZR_AST_FOR_LOOP:
                    case ZR_AST_FOREACH_LOOP:
                        ZrParser_Statement_Compile(cs, stmt);
                        break;
                    case ZR_AST_TEST_DECLARATION:
                        compile_test_declaration(cs, stmt);
                        break;
                    case ZR_AST_STRUCT_DECLARATION:
                        compile_struct_declaration(cs, stmt);
                        break;
                    case ZR_AST_EXTERN_BLOCK:
                        compile_extern_block_declaration(cs, stmt);
                        break;
                    case ZR_AST_CLASS_DECLARATION:
                        compile_class_declaration(cs, stmt);
                        break;
                    case ZR_AST_INTERFACE_DECLARATION:
                        // 处理interface声明
                        // 注意：interface主要用于类型检查，不需要生成运行时代码
                        // 可以在这里进行interface的类型信息收集和验证
                        // TODO: 暂时跳过，后续可以实现interface的类型检查
                        break;
                    case ZR_AST_ENUM_DECLARATION:
                        // 处理enum声明
                        // TODO: enum可以编译为常量或对象，这里暂时跳过
                        // 后续可以实现enum的编译
                        break;
                    default:
                        // 其他顶层声明类型（intermediate等）
                        // TODO: 目前先跳过，后续实现
                        printf("    Skipping statement type %d (not implemented yet)\n", stmt->type);
                        break;
                }

                if (cs->hasCompileTimeError) {
                    cs->hasError = ZR_TRUE;
                    return;
                }

                // 即使有错误，也继续编译后续语句（除非是致命错误）
                // 这样可以尽可能多地编译成功的语句
                if (cs->hasError && !cs->hasFatalError) {
                    printf("    Compilation error at statement %zu, resetting error and continuing...\n", i);
                    // 重置错误状态，继续编译后续语句
                    cs->hasError = ZR_FALSE;
                    cs->errorMessage = ZR_NULL;
                } else if (cs->hasFatalError) {
                    printf("  Fatal error encountered, stopping compilation\n");
                    return;
                }
            }
        }
        printf("  Finished compiling statements, total instructions: %zu\n", cs->instructionCount);
        exit_scope(cs);
    }

    // 3. 在返回前添加导出收集代码（如果有导出的变量）
    // 导出收集在运行时进行（在内部模块导入 helper 执行完 __entry 后）
    // 这里只需要确保导出信息被正确记录到函数中
    // 导出的变量信息已存储在 cs->pubVariables 和 cs->proVariables 中
    // 这些信息将在编译完成后复制到函数的 exportedVariables 字段中
    
    // 4. 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        // 使用 instructions.length 而不是 instructionCount，确保同步
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TZrUInt16) resultSlot, (TZrInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                           (TZrInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }
    
    // 5. 将 prototype 信息序列化为二进制数据并存储到 function->prototypeData
    // 运行时创建逻辑将在内部模块导入 helper 中实现（在创建模块后）
    // 使用紧凑二进制格式存储，不再使用常量池
    
    if (cs->typePrototypes.length > 0) {
        // 计算所有 prototype 数据的总大小
        TZrSize totalPrototypeDataSize = 0;
        TZrSize serializablePrototypeCount = 0;
        TZrByte **prototypeDataArray = (TZrByte **)ZrCore_Memory_RawMalloc(cs->state->global, cs->typePrototypes.length * sizeof(TZrByte *));
        TZrSize *prototypeDataSizes = (TZrSize *)ZrCore_Memory_RawMalloc(cs->state->global, cs->typePrototypes.length * sizeof(TZrSize));
        
        if (prototypeDataArray == ZR_NULL || prototypeDataSizes == ZR_NULL) {
            if (prototypeDataArray != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray, cs->typePrototypes.length * sizeof(TZrByte *));
            }
            if (prototypeDataSizes != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, prototypeDataSizes, cs->typePrototypes.length * sizeof(TZrSize));
            }
        } else {
            // 序列化每个 prototype 信息
            for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
                if (info == ZR_NULL || info->name == ZR_NULL || info->isImportedNative) {
                    prototypeDataArray[i] = ZR_NULL;
                    prototypeDataSizes[i] = 0;
                    continue;
                }
                
                // 序列化prototype信息为二进制数据（不存储到常量池）
                TZrByte *prototypeData = ZR_NULL;
                TZrSize prototypeDataSize = 0;
                if (serialize_prototype_info_to_binary(cs, info, &prototypeData, &prototypeDataSize)) {
                    prototypeDataArray[i] = prototypeData;
                    prototypeDataSizes[i] = prototypeDataSize;
                    totalPrototypeDataSize += prototypeDataSize;
                    serializablePrototypeCount++;
                } else {
                    prototypeDataArray[i] = ZR_NULL;
                    prototypeDataSizes[i] = 0;
                }
            }
            
            // 将所有 prototype 数据合并到一个连续的缓冲区中
            if (totalPrototypeDataSize > 0) {
                // 在数据前添加一个头部：prototype 数量（TZrUInt32）
                TZrSize finalDataSize = sizeof(TZrUInt32) + totalPrototypeDataSize;
                TZrByte *finalPrototypeData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, finalDataSize);
                if (finalPrototypeData != ZR_NULL) {
                    // 写入 prototype 数量
                    TZrUInt32 *prototypeCountPtr = (TZrUInt32 *)finalPrototypeData;
                    *prototypeCountPtr = (TZrUInt32)serializablePrototypeCount;
                    
                    // 复制每个 prototype 的数据
                    TZrByte *currentPos = finalPrototypeData + sizeof(TZrUInt32);
                    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                        if (prototypeDataArray[i] != ZR_NULL && prototypeDataSizes[i] > 0) {
                            memcpy(currentPos, prototypeDataArray[i], prototypeDataSizes[i]);
                            currentPos += prototypeDataSizes[i];
                            // 释放单个 prototype 数据
                            ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray[i], prototypeDataSizes[i]);
                        }
                    }
                    
                    // 存储到 function
                    cs->currentFunction->prototypeData = finalPrototypeData;
                    cs->currentFunction->prototypeDataLength = (TZrUInt32)finalDataSize;
                    cs->currentFunction->prototypeCount = (TZrUInt32)serializablePrototypeCount;
                }
            } else {
                cs->currentFunction->prototypeData = ZR_NULL;
                cs->currentFunction->prototypeDataLength = 0;
                cs->currentFunction->prototypeCount = 0;
            }
            
            // 释放临时数组
            ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray, cs->typePrototypes.length * sizeof(TZrByte *));
            ZrCore_Memory_RawFree(cs->state->global, prototypeDataSizes, cs->typePrototypes.length * sizeof(TZrSize));
        }
    } else {
        cs->currentFunction->prototypeData = ZR_NULL;
        cs->currentFunction->prototypeDataLength = 0;
        cs->currentFunction->prototypeCount = 0;
    }
    
    // 重置脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
}

// 主编译入口（占位实现）
SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast) {
    if (state == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    SZrCompilerState cs;
    ZrParser_CompilerState_Init(&cs, state);

    // 创建新函数
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 编译脚本
    compile_script(&cs, ast);

    if (cs.hasError) {
        // 错误信息已在 ZrParser_Compiler_Error 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.currentFunction);
        }
        if (cs.topLevelFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.topLevelFunction);
        }
        ZrParser_CompilerState_Free(&cs);
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
                    (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->instructionsList == ZR_NULL) {
                ZrCore_Function_Free(state, func);
                ZrParser_CompilerState_Free(&cs);
                return ZR_NULL;
            }
            memcpy(func->instructionsList, cs.instructions.head, instSize);
            func->instructionsLength = (TZrUInt32) cs.instructions.length;
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
                    (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->constantValueList == ZR_NULL) {
                ZrCore_Function_Free(state, func);
                ZrParser_CompilerState_Free(&cs);
                return ZR_NULL;
            }
            memcpy(func->constantValueList, cs.constants.head, constSize);
            func->constantValueLength = (TZrUInt32) cs.constants.length;
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
            func->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                    global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->localVariableList == ZR_NULL) {
                ZrCore_Function_Free(state, func);
                ZrParser_CompilerState_Free(&cs);
                return ZR_NULL;
            }
            memcpy(func->localVariableList, cs.localVars.head, localVarSize);
            func->localVariableLength = (TZrUInt32) cs.localVars.length;
            // 同步 localVarCount
            cs.localVarCount = cs.localVars.length;
        } else {
            func->localVariableLength = 0;
            func->localVariableList = ZR_NULL;
        }

        // 4. 复制闭包变量列表（仅对脚本包装函数）
        if (cs.closureVarCount > 0) {
            TZrSize closureVarSize = cs.closureVarCount * sizeof(SZrFunctionClosureVariable);
            func->closureValueList = (SZrFunctionClosureVariable *) ZrCore_Memory_RawMallocWithType(
                    global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if (func->closureValueList == ZR_NULL) {
                ZrCore_Function_Free(state, func);
                ZrParser_CompilerState_Free(&cs);
                return ZR_NULL;
            }
            memcpy(func->closureValueList, cs.closureVars.head, closureVarSize);
            func->closureValueLength = (TZrUInt32) cs.closureVarCount;
        } else {
            func->closureValueLength = 0;
            func->closureValueList = ZR_NULL;
        }
    }

    // 5. 复制子函数列表
    if (cs.childFunctions.length > 0) {
        TZrSize childFuncSize = cs.childFunctions.length * sizeof(SZrFunction);
        func->childFunctionList =
                (struct SZrFunction *) ZrCore_Memory_RawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->childFunctionList == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_NULL;
        }
        // 从指针数组复制到对象数组
        SZrFunction **srcArray = (SZrFunction **) cs.childFunctions.head;
        for (TZrSize i = 0; i < cs.childFunctions.length; i++) {
            if (srcArray[i] != ZR_NULL) {
                func->childFunctionList[i] = *srcArray[i];
            }
        }
        func->childFunctionLength = (TZrUInt32) cs.childFunctions.length;
    }

    // 6. 复制导出变量信息（合并 pubVariables 和 proVariables）
    // proVariables 已经包含所有 pubVariables，所以只需要复制 proVariables
    if (cs.proVariables.length > 0) {
        TZrSize exportVarSize = cs.proVariables.length * sizeof(struct SZrFunctionExportedVariable);
        func->exportedVariables = (struct SZrFunctionExportedVariable *) ZrCore_Memory_RawMallocWithType(
                global, exportVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->exportedVariables == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_NULL;
        }
        // 从 SZrExportedVariable 复制到 SZrFunctionExportedVariable
        for (TZrSize i = 0; i < cs.proVariables.length; i++) {
            SZrExportedVariable *src = (SZrExportedVariable *) ZrCore_Array_Get(&cs.proVariables, i);
            if (src != ZR_NULL) {
                func->exportedVariables[i].name = src->name;
                func->exportedVariables[i].stackSlot = src->stackSlot;
                func->exportedVariables[i].accessModifier = (TZrUInt8) src->accessModifier;
            }
        }
        func->exportedVariableLength = (TZrUInt32) cs.proVariables.length;
    } else {
        func->exportedVariables = ZR_NULL;
        func->exportedVariableLength = 0;
    }

    // 7. 设置函数元数据
    // 注意：脚本入口函数没有参数（它是脚本的包装函数）
    // 脚本中的函数声明会被编译为子函数，它们的参数信息已通过 compile_function_declaration 正确设置
    // 但是，如果返回的是顶层函数声明，参数信息已经在 compile_function_declaration 中设置了，不应该覆盖
    func->stackSize = (TZrUInt32) cs.maxStackSlotCount;
    if (cs.topLevelFunction == ZR_NULL) {
        // 只有当返回的是脚本函数时，才设置参数数量为0
        func->parameterCount = 0;  // 脚本入口函数没有参数
        func->hasVariableArguments = ZR_FALSE;  // 脚本入口函数不支持可变参数
    }
    // 如果返回的是顶层函数，parameterCount 和 hasVariableArguments 已经在 compile_function_declaration 中设置了
    func->lineInSourceStart = (ast->location.start.line > 0) ? (TZrUInt32) ast->location.start.line : 0;
    func->lineInSourceEnd = (ast->location.end.line > 0) ? (TZrUInt32) ast->location.end.line : 0;
    if (func == cs.currentFunction &&
        !compiler_copy_function_exception_metadata_slice(&cs, func, 0, 0, 0, ast)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 确保所有字段都被正确初始化（避免 ZrCore_Function_Free 中的断言失败）
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

    ZrParser_CompilerState_Free(&cs);
    return func;
}

// 编译 AST 为函数和测试函数列表（新接口）
TZrBool ZrParser_Compiler_CompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result) {
    if (state == ZR_NULL || ast == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    // 初始化结果结构体
    result->mainFunction = ZR_NULL;
    result->testFunctions = ZR_NULL;
    result->testFunctionCount = 0;

    SZrCompilerState cs;
    ZrParser_CompilerState_Init(&cs, state);

    // 创建新函数
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    // 编译脚本
    compile_script(&cs, ast);

    if (cs.hasError) {
        // 错误信息已在 ZrParser_Compiler_Error 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.currentFunction);
        }
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    // 将编译结果复制到 SZrFunction（与ZrCompilerCompile相同的逻辑）
    SZrFunction *func = cs.currentFunction;
    SZrGlobalState *global = state->global;

    // 1. 复制指令列表
    if (cs.instructions.length > 0) {
        TZrSize instSize = cs.instructions.length * sizeof(TZrInstruction);
        func->instructionsList =
                (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->instructionsList == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        memcpy(func->instructionsList, cs.instructions.head, instSize);
        func->instructionsLength = (TZrUInt32) cs.instructions.length;
        cs.instructionCount = cs.instructions.length;
    }

    // 2. 复制常量列表
    // 使用 constants.length 而不是 constantCount，确保同步
    if (cs.constants.length > 0) {
        TZrSize constSize = cs.constants.length * sizeof(SZrTypeValue);
        func->constantValueList =
                (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->constantValueList == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        memcpy(func->constantValueList, cs.constants.head, constSize);
        func->constantValueLength = (TZrUInt32) cs.constants.length;
        // 同步 constantCount
        cs.constantCount = cs.constants.length;
    }

    // 3. 复制局部变量列表
    if (cs.localVars.length > 0) {
        TZrSize localVarSize = cs.localVars.length * sizeof(SZrFunctionLocalVariable);
        func->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
                global, localVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->localVariableList == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        memcpy(func->localVariableList, cs.localVars.head, localVarSize);
        func->localVariableLength = (TZrUInt32) cs.localVars.length;
        cs.localVarCount = cs.localVars.length;
    }

    // 4. 复制闭包变量列表
    if (cs.closureVarCount > 0) {
        TZrSize closureVarSize = cs.closureVarCount * sizeof(SZrFunctionClosureVariable);
        func->closureValueList = (SZrFunctionClosureVariable *) ZrCore_Memory_RawMallocWithType(
                global, closureVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->closureValueList == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        memcpy(func->closureValueList, cs.closureVars.head, closureVarSize);
        func->closureValueLength = (TZrUInt32) cs.closureVarCount;
    }

    // 5. 复制子函数列表
    if (cs.childFunctions.length > 0) {
        TZrSize childFuncSize = cs.childFunctions.length * sizeof(SZrFunction);
        func->childFunctionList =
                (struct SZrFunction *) ZrCore_Memory_RawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->childFunctionList == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        SZrFunction **srcArray = (SZrFunction **) cs.childFunctions.head;
        for (TZrSize i = 0; i < cs.childFunctions.length; i++) {
            if (srcArray[i] != ZR_NULL) {
                func->childFunctionList[i] = *srcArray[i];
            }
        }
        func->childFunctionLength = (TZrUInt32) cs.childFunctions.length;
    }

    // 6. 复制导出变量信息（合并 pubVariables 和 proVariables）
    // proVariables 已经包含所有 pubVariables，所以只需要复制 proVariables
    if (cs.proVariables.length > 0) {
        TZrSize exportVarSize = cs.proVariables.length * sizeof(struct SZrFunctionExportedVariable);
        func->exportedVariables = (struct SZrFunctionExportedVariable *) ZrCore_Memory_RawMallocWithType(
                global, exportVarSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (func->exportedVariables == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        // 从 SZrExportedVariable 复制到 SZrFunctionExportedVariable
        for (TZrSize i = 0; i < cs.proVariables.length; i++) {
            SZrExportedVariable *src = (SZrExportedVariable *) ZrCore_Array_Get(&cs.proVariables, i);
            if (src != ZR_NULL) {
                func->exportedVariables[i].name = src->name;
                func->exportedVariables[i].stackSlot = src->stackSlot;
                func->exportedVariables[i].accessModifier = (TZrUInt8) src->accessModifier;
            }
        }
        func->exportedVariableLength = (TZrUInt32) cs.proVariables.length;
    } else {
        func->exportedVariables = ZR_NULL;
        func->exportedVariableLength = 0;
    }

    // 7. 设置函数元数据
    func->stackSize = (TZrUInt32) cs.maxStackSlotCount;
    func->parameterCount = 0;
    func->hasVariableArguments = ZR_FALSE;
    func->lineInSourceStart = (ast->location.start.line > 0) ? (TZrUInt32) ast->location.start.line : 0;
    func->lineInSourceEnd = (ast->location.end.line > 0) ? (TZrUInt32) ast->location.end.line : 0;
    if (!compiler_copy_function_exception_metadata_slice(&cs, func, 0, 0, 0, ast)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

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
        result->testFunctions = (SZrFunction **) ZrCore_Memory_RawMallocWithType(
                global, testFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (result->testFunctions == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        // 复制测试函数指针
        SZrFunction **srcTestArray = (SZrFunction **) cs.testFunctions.head;
        for (TZrSize i = 0; i < cs.testFunctions.length; i++) {
            result->testFunctions[i] = srcTestArray[i];
        }
        result->testFunctionCount = cs.testFunctions.length;
    }

    ZrParser_CompilerState_Free(&cs);
    return ZR_TRUE;
}

// 释放编译结果
void ZrParser_CompileResult_Free(SZrState *state, SZrCompileResult *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    // 释放测试函数数组（函数对象本身由GC管理，不需要释放）
    if (result->testFunctions != ZR_NULL && result->testFunctionCount > 0) {
        SZrGlobalState *global = state->global;
        TZrSize testFuncSize = result->testFunctionCount * sizeof(SZrFunction *);
        ZrCore_Memory_RawFreeWithType(global, result->testFunctions, testFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        result->testFunctions = ZR_NULL;
        result->testFunctionCount = 0;
    }

    // 主函数由调用者负责释放（如果不需要可以调用ZrFunctionFree）
    // 这里不释放，因为调用者可能还需要使用
}

// 编译源代码为函数（封装了从解析到编译的全流程）
struct SZrFunction *ZrParser_Source_Compile(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName) {
    if (state == ZR_NULL || source == ZR_NULL || sourceLength == 0) {
        return ZR_NULL;
    }
    
    // 解析源代码为AST
    SZrAstNode *ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 编译AST为函数
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);
    
    // 释放AST
    ZrParser_Ast_Free(state, ast);
    
    return func;
}

// 注册 compileSource 函数到 globalState
void ZrParser_ToGlobalState_Register(struct SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }
    
    // 使用 API 设置 compileSource 函数指针，避免直接访问内部结构
    ZrCore_GlobalState_SetCompileSource(state->global, ZrParser_Source_Compile);
}
