//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

void record_external_var_reference(SZrCompilerState *cs, SZrString *name) {
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

void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames);

void collect_identifiers_from_array(SZrCompilerState *cs, SZrAstNodeArray *nodes, SZrArray *identifierNames) {
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
void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames) {
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
        case ZR_AST_TYPE_QUERY_EXPRESSION: {
            SZrTypeQueryExpression *typeQuery = &node->data.typeQueryExpression;
            if (typeQuery->operand != ZR_NULL) {
                collect_identifiers_from_node(cs, typeQuery->operand, identifierNames);
            }
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
    ZrCore_Array_Init(cs->state, &identifierNames, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
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
        if (localIndex == ZR_PARSER_SLOT_NONE && closureIndex == ZR_PARSER_INDEX_NONE) {
            // 在父编译器中查找（外部作用域的变量）
            TZrUInt32 parentLocalIndex = find_local_var(parentCompiler, name);
            TZrUInt32 parentClosureIndex = find_closure_var(parentCompiler, name);
            if (parentLocalIndex != ZR_PARSER_SLOT_NONE || parentClosureIndex != ZR_PARSER_INDEX_NONE) {
                // 这是外部变量，需要捕获到闭包中
                // 注意：index 必须指向父作用域中的真实槽位/上值索引，而不是当前闭包数组长度。
                if (find_closure_var(cs, name) == ZR_PARSER_INDEX_NONE) {
                    SZrFunctionClosureVariable closureVar;
                    closureVar.name = name;
                    closureVar.inStack = (parentLocalIndex != ZR_PARSER_SLOT_NONE) ? ZR_TRUE : ZR_FALSE;
                    closureVar.index = (parentLocalIndex != ZR_PARSER_SLOT_NONE) ? parentLocalIndex : parentClosureIndex;
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
