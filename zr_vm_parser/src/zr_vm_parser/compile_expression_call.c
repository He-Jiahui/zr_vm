//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"

SZrAstNode *find_function_declaration(SZrCompilerState *cs, SZrString *funcName) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || funcName == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (cs->scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_NULL;
    }
    
    SZrScript *script = &cs->scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历顶层语句，查找函数声明
    for (TZrSize i = 0; i < script->statements->count; i++) {
        SZrAstNode *stmt = script->statements->nodes[i];
        if (stmt != ZR_NULL && stmt->type == ZR_AST_FUNCTION_DECLARATION) {
            SZrFunctionDeclaration *funcDecl = &stmt->data.functionDeclaration;
            if (funcDecl->name != ZR_NULL && funcDecl->name->name != ZR_NULL) {
                if (ZrCore_String_Equal(funcDecl->name->name, funcName)) {
                    return stmt;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 根据函数参数列表解析调用参数。
// 对已知签名的调用，统一处理命名参数重排和缺失默认值回填。
SZrAstNodeArray *match_named_arguments(SZrCompilerState *cs, 
                                               SZrFunctionCall *call,
                                               SZrAstNodeArray *paramList) {
    if (cs == ZR_NULL || call == ZR_NULL ||
        call->args == ZR_NULL || call->argNames == ZR_NULL || paramList == ZR_NULL) {
        return call->args;
    }
    
    // 创建参数映射表：参数名 -> 参数索引
    TZrSize paramCount = paramList->count;
    SZrString **paramNames = ZrCore_Memory_RawMallocWithType(cs->state->global, 
                                                       sizeof(SZrString*) * paramCount, 
                                                       ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (paramNames == ZR_NULL) {
        return call->args;  // 内存分配失败，返回原数组
    }
    
    // 提取参数名
    for (TZrSize i = 0; i < paramCount; i++) {
        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->name != ZR_NULL) {
                paramNames[i] = param->name->name;
            } else {
                paramNames[i] = ZR_NULL;
            }
        } else {
            paramNames[i] = ZR_NULL;
        }
    }
    
    // 创建重新排列的参数数组
    SZrAstNodeArray *reorderedArgs = ZrParser_AstNodeArray_New(cs->state, paramCount);
    if (reorderedArgs == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    
    // 初始化数组，所有位置设为 ZR_NULL（表示未提供）
    // 注意：不能使用 ZrParser_AstNodeArray_Add 因为当 node 为 ZR_NULL 时会直接返回
    // 所以直接设置数组元素并手动更新 count
    for (TZrSize i = 0; i < paramCount; i++) {
        reorderedArgs->nodes[i] = ZR_NULL;
    }
    reorderedArgs->count = paramCount;  // 手动设置 count
    
    // 标记哪些参数已被提供
    TZrBool *provided = ZrCore_Memory_RawMallocWithType(cs->state->global, 
                                                sizeof(TZrBool) * paramCount, 
                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provided == ZR_NULL) {
        ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
        ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return call->args;
    }
    for (TZrSize i = 0; i < paramCount; i++) {
        provided[i] = ZR_FALSE;
    }
    
    // 处理位置参数（在命名参数之前）
    TZrSize positionalCount = 0;
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrCore_Array_Get(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr == ZR_NULL) {
            // 位置参数
            if (positionalCount < paramCount) {
                reorderedArgs->nodes[positionalCount] = call->args->nodes[i];
                provided[positionalCount] = ZR_TRUE;
                positionalCount++;
            } else {
                // 位置参数过多
                ZrParser_Compiler_Error(cs, "Too many positional arguments", call->args->nodes[i]->location);
                ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                return call->args;
            }
        } else {
            // 遇到命名参数，停止处理位置参数
            break;
        }
    }
    
    // 处理命名参数
    for (TZrSize i = 0; i < call->args->count; i++) {
        SZrString **namePtr = (SZrString**)ZrCore_Array_Get(call->argNames, i);
        if (namePtr != ZR_NULL && *namePtr != ZR_NULL) {
            // 命名参数
            SZrString *argName = *namePtr;
            TZrBool found = ZR_FALSE;
            
            // 查找参数名对应的位置
            for (TZrSize j = 0; j < paramCount; j++) {
                if (paramNames[j] != ZR_NULL) {
                    if (ZrCore_String_Equal(argName, paramNames[j])) {
                        if (provided[j]) {
                            // 参数重复
                            ZrParser_Compiler_Error(cs, "Duplicate argument name", call->args->nodes[i]->location);
                            ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                            ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                            return call->args;
                        }
                        reorderedArgs->nodes[j] = call->args->nodes[i];
                        provided[j] = ZR_TRUE;
                        found = ZR_TRUE;
                        break;
                    }
                }
            }
            
            if (!found) {
                // 未找到匹配的参数名
                TZrNativeString nameStr = ZrCore_String_GetNativeString(argName);
                TZrChar errorMsg[256];
                snprintf(errorMsg, sizeof(errorMsg), "Unknown argument name: %s", nameStr ? nameStr : "<null>");
                ZrParser_Compiler_Error(cs, errorMsg, call->args->nodes[i]->location);
                ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
                ZrParser_AstNodeArray_Free(cs->state, reorderedArgs);
                return call->args;
            }
        }
    }
    
    // 回填缺失参数的默认值；命名参数留下的空洞不能直接交给运行时，
    // 否则运行时只会把尾部参数补 null，无法区分“省略且有默认值”和“显式传 null”。
    for (TZrSize i = 0; i < paramCount; i++) {
        if (provided[i]) {
            continue;
        }

        SZrAstNode *paramNode = paramList->nodes[i];
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            SZrParameter *param = &paramNode->data.parameter;
            if (param->defaultValue != ZR_NULL) {
                reorderedArgs->nodes[i] = param->defaultValue;
                provided[i] = ZR_TRUE;
            }
        }
    }
    
    ZrCore_Memory_RawFreeWithType(cs->state->global, provided, sizeof(TZrBool) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    ZrCore_Memory_RawFreeWithType(cs->state->global, paramNames, sizeof(SZrString*) * paramCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    
    return reorderedArgs;
}

// 编译函数调用表达式
void compile_function_call(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_FUNCTION_CALL) {
        ZrParser_Compiler_Error(cs, "Expected function call", node->location);
        return;
    }
    
    // 函数调用在 primary expression 中处理
    // 这里只处理参数列表
    SZrAstNodeArray *args = node->data.functionCall.args;
    if (args != ZR_NULL) {
        // 编译所有参数表达式（从右到左压栈，或从左到右，取决于调用约定）
        for (TZrSize i = 0; i < args->count; i++) {
            SZrAstNode *arg = args->nodes[i];
            if (arg != ZR_NULL) {
                ZrParser_Expression_Compile(cs, arg);
            }
        }
    }
    
    // 注意：实际的函数调用指令（FUNCTION_CALL）应该在 primary expression 中生成
    // 因为需要先编译 callee（被调用表达式）
}

// 编译成员访问表达式
void compile_member_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected member expression", node->location);
        return;
    }
    
    // 成员访问在 primary expression 中处理
    // 这里只处理属性访问
    // 注意：实际的 GETTABLE/SETTABLE 指令应该在 primary expression 中生成
    // 因为需要先编译对象表达式
}

void compile_import_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrImportExpression *importExpr;
    SZrAstNode *modulePathNode;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_IMPORT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected import expression", node->location);
        return;
    }

    importExpr = &node->data.importExpression;
    modulePathNode = importExpr->modulePath;
    if (modulePathNode == ZR_NULL || modulePathNode->type != ZR_AST_STRING_LITERAL ||
        modulePathNode->data.stringLiteral.value == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Import expression requires a normalized string module path", node->location);
        return;
    }

    if (ZrParser_Compiler_EmitImportModuleExpression(cs, modulePathNode->data.stringLiteral.value, node->location) ==
        (TZrUInt32)-1 &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile import expression", node->location);
    }
}

// 编译主表达式（属性访问链和函数调用链）
void compile_primary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected primary expression", node->location);
        return;
    }

    if (try_emit_compile_time_function_call(cs, node)) {
        return;
    }

    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    TZrUInt32 currentSlot = (TZrUInt32)-1;
    SZrString *rootTypeName = ZR_NULL;
    TZrBool rootIsTypeReference = ZR_FALSE;
    EZrOwnershipQualifier rootOwnershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    TZrSize memberStartIndex = 0;

    // 1. 编译基础属性（标识符或表达式）
    if (primary->property != ZR_NULL) {
        compile_expression_non_tail(cs, primary->property);
        if (cs->hasError) {
            return;
        }
    } else {
        ZrParser_Compiler_Error(cs, "Primary expression property is null", node->location);
        return;
    }

    currentSlot = cs->stackSlotCount - 1;
    resolve_expression_root_type(cs, primary->property, &rootTypeName, &rootIsTypeReference);
    rootOwnershipQualifier = infer_expression_ownership_qualifier_local(cs, primary->property);
    if (cs->hasError) {
        return;
    }
    compile_primary_member_chain(cs, primary->property, primary->members, memberStartIndex, &currentSlot,
                                 &rootTypeName, &rootIsTypeReference, &rootOwnershipQualifier);
}

void compile_prototype_reference_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrPrototypeReferenceExpression *prototypeExpr;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected prototype reference expression", node->location);
        return;
    }

    prototypeExpr = &node->data.prototypeReferenceExpression;
    if (prototypeExpr->target == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Prototype reference target is null", node->location);
        return;
    }

    if (resolve_construct_target_type_name(cs, prototypeExpr->target, ZR_NULL) == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "Prototype reference target must resolve to a registered prototype",
                                node->location);
        return;
    }

    compile_expression_non_tail(cs, prototypeExpr->target);
}

void compile_construct_expression(SZrCompilerState *cs, SZrAstNode *node) {
    SZrConstructExpression *constructExpr;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    SZrString *typeName;
    const TZrChar *op;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected construct expression", node->location);
        return;
    }

    constructExpr = &node->data.constructExpression;
    if (construct_expression_is_ownership_builtin(constructExpr)) {
        if (!compile_ownership_builtin_expression(cs, constructExpr, node->location) && !cs->hasError) {
            ZrParser_Compiler_Error(cs, "Failed to compile ownership builtin expression", node->location);
        }
        return;
    }

    typeName = resolve_construct_target_type_name(cs, constructExpr->target, &prototypeType);
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                                "Prototype construction target must resolve to a registered prototype",
                                node->location);
        return;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        ZrParser_Compiler_Error(cs, "Module values cannot be constructed directly", node->location);
        return;
    }

    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
        ZrParser_Compiler_Error(cs, "Interface prototypes cannot be constructed", node->location);
        return;
    }

    {
        TZrSize savedStackCount = cs->stackSlotCount;
        compile_expression_non_tail(cs, constructExpr->target);
        if (cs->hasError) {
            return;
        }
        ZrParser_Compiler_TrimStackToCount(cs, savedStackCount);
    }

    op = constructExpr->isNew ? "new" : "$";
    if (emit_shorthand_constructor_instance(cs, op, typeName, constructExpr->args, node->location) == (TZrUInt32)-1 &&
        !cs->hasError) {
        ZrParser_Compiler_Error(cs, "Failed to compile construct expression", node->location);
        return;
    }

    if ((constructExpr->isUsing ||
         constructExpr->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) &&
        constructExpr->isNew) {
        if (!wrap_constructed_result_with_ownership_builtin(cs, constructExpr, node->location) && !cs->hasError) {
            ZrParser_Compiler_Error(cs, "Failed to wrap ownership-aware construction result", node->location);
        }
    }
}

// 编译数组字面量
