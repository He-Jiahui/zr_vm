//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

/* Borrow/Loan 是不能跨闭包边界逃逸的所有权类别；其余限定符不会在这里被拦截。 */
static TZrBool compiler_ownership_qualifier_is_borrow_escape(EZrOwnershipQualifier qualifier) {
    return qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
           qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED;
}

/* 递归检查推断类型及其 elementTypes，覆盖容器或泛型参数中的嵌套 Borrow/Loan。 */
static TZrBool compiler_inferred_type_contains_borrow_escape_ownership(const SZrInferredType *type) {
    TZrSize index;

    if (type == ZR_NULL) {
        return ZR_FALSE;
    }

    if (compiler_ownership_qualifier_is_borrow_escape(type->ownershipQualifier)) {
        return ZR_TRUE;
    }

    for (index = 0; index < type->elementTypes.length; index++) {
        SZrInferredType *elementType =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&type->elementTypes, index);

        if (compiler_inferred_type_contains_borrow_escape_ownership(elementType)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 在真正写入 closureVars 前读取父级 typeEnv 检查 Borrow/Loan；typeEnv 缺失或查找失败会放行，语义待确认。 */
static TZrBool compiler_validate_closure_capture_ownership_escape(SZrCompilerState *cs,
                                                                  SZrCompilerState *parentCompiler,
                                                                  SZrString *name,
                                                                  SZrFileRange location) {
    SZrInferredType capturedType;
    TZrBool hasType;

    if (cs == ZR_NULL || parentCompiler == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (parentCompiler->typeEnv == ZR_NULL) {
        /* TODO: 缺少父级 typeEnv 时当前仍放行捕获；核实无类型环境入口是否可能承载 Borrow/Loan，并补上未知类型闭包场景。 */
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, &capturedType, ZR_VALUE_TYPE_OBJECT);
    hasType = ZrParser_TypeEnvironment_LookupVariable(cs->state,
                                                      parentCompiler->typeEnv,
                                                      name,
                                                      &capturedType);
    if (!hasType) {
        ZrParser_InferredType_Free(cs->state, &capturedType);
        /* TODO: 父级槽位存在但 TypeEnvironment 查不到 binding 时也会放行；核实注册覆盖范围并补查找失败测试。 */
        return ZR_TRUE;
    }

    if (compiler_inferred_type_contains_borrow_escape_ownership(&capturedType)) {
        ZrParser_InferredType_Free(cs->state, &capturedType);
        ZrParser_Compiler_Error(cs,
                                "Borrowed and loaned owners cannot escape through closure capture",
                                location);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(cs->state, &capturedType);
    return ZR_TRUE;
}

/* 尽可能为捕获槽补充符号身份：优先使用父级 TypeEnvironment 声明范围，其次核对父级 Semantic IR 槽身份，最后沿父闭包记录回退。 */
static void compiler_closure_capture_identity_from_parent(
        SZrFunctionClosureVariable *capture,
        const SZrCompilerState *parentCompiler,
        SZrString *name,
        TZrUInt32 parentLocalIndex,
        TZrUInt32 parentClosureIndex) {
    const SZrTypeBinding *binding;
    const SZrFunctionClosureVariable *parentCapture = ZR_NULL;
    SZrCompilerSemanticIrSlotIdentity slotIdentity;

    if (capture == ZR_NULL || parentCompiler == ZR_NULL || name == ZR_NULL) {
        return;
    }

    binding = ZrParser_TypeEnvironment_FindVariableBinding(parentCompiler->typeEnv, name);
    if (binding != ZR_NULL &&
        binding->symbolId != ZR_SEMANTIC_ID_INVALID &&
        binding->typeId != ZR_SEMANTIC_ID_INVALID) {
        if (binding->hasDeclarationRange &&
            binding->declarationRange.start.line >= 0 &&
            binding->declarationRange.start.column >= 0 &&
            binding->declarationRange.end.line >= 0 &&
            binding->declarationRange.end.column >= 0) {
            capture->symbolId = binding->symbolId;
            capture->typeId = binding->typeId;
            capture->declarationStartLine = (TZrUInt32)binding->declarationRange.start.line;
            capture->declarationStartColumn = (TZrUInt32)binding->declarationRange.start.column;
            capture->declarationEndLine = (TZrUInt32)binding->declarationRange.end.line;
            capture->declarationEndColumn = (TZrUInt32)binding->declarationRange.end.column;
            return;
        }

        ZrCore_Memory_RawSet(&slotIdentity, 0, sizeof(slotIdentity));
        if (parentLocalIndex != ZR_PARSER_SLOT_NONE &&
            compiler_semantic_ir_get_slot_identity(
                    (SZrCompilerState *)parentCompiler,
                    parentLocalIndex,
                    &slotIdentity) &&
            slotIdentity.symbolId == binding->symbolId &&
            slotIdentity.typeId == binding->typeId &&
            slotIdentity.declarationRange.start.line >= 0 &&
            slotIdentity.declarationRange.start.column >= 0 &&
            slotIdentity.declarationRange.end.line >= 0 &&
            slotIdentity.declarationRange.end.column >= 0) {
            capture->symbolId = slotIdentity.symbolId;
            capture->typeId = slotIdentity.typeId;
            capture->declarationStartLine = (TZrUInt32)slotIdentity.declarationRange.start.line;
            capture->declarationStartColumn = (TZrUInt32)slotIdentity.declarationRange.start.column;
            capture->declarationEndLine = (TZrUInt32)slotIdentity.declarationRange.end.line;
            capture->declarationEndColumn = (TZrUInt32)slotIdentity.declarationRange.end.column;
            return;
        }
    }

    if (parentClosureIndex < parentCompiler->closureVars.length) {
        parentCapture = (const SZrFunctionClosureVariable *)ZrCore_Array_Get(
                (SZrArray *)&parentCompiler->closureVars,
                parentClosureIndex);
    }
    if (parentCapture != ZR_NULL) {
        capture->symbolId = parentCapture->symbolId;
        capture->typeId = parentCapture->typeId;
        capture->declarationStartLine = parentCapture->declarationStartLine;
        capture->declarationStartColumn = parentCapture->declarationStartColumn;
        capture->declarationEndLine = parentCapture->declarationEndLine;
        capture->declarationEndColumn = parentCapture->declarationEndColumn;
    }
}

/* TODO: 仓内只有 compiler_internal.h 的声明和本定义，没有调用入口；确认该兼容名单是否仍需维护，并在保留时核实字符串指针的生命周期与去重契约。 */
void record_external_var_reference(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL || cs->hasError) {
        return;
    }
    
    /* 该旧名单按 SZrString 对象指针去重，而非比较字符串内容。 */
    for (TZrSize i = 0; i < cs->referencedExternalVars.length; i++) {
        SZrString **varName = (SZrString **)ZrCore_Array_Get(&cs->referencedExternalVars, i);
        if (varName != ZR_NULL && *varName == name) {
            return; // 已存在
        }
    }
    
    /* 只把 SZrString* 存进数组，不复制或单独释放字符串；compiler_state.c 释放的是数组缓冲区。 */
    ZrCore_Array_Push(cs->state, &cs->referencedExternalVars, &name);
}

void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames);

/* 遍历 AST 节点数组，把每个非空子树交给统一 visitor；数组只借用调用方存储。 */
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

/* 递归遍历闭包体 AST，先按语法结构收集标识符，再由外层函数解析其作用域归属。 */
void collect_identifiers_from_node(SZrCompilerState *cs, SZrAstNode *node, SZrArray *identifierNames) {
    if (cs == ZR_NULL || node == ZR_NULL || identifierNames == ZR_NULL) {
        return;
    }
    
    /* 标识符节点是收集终点；用对象指针去重以复用解析阶段的字符串对象。 */
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
    
    /* 仅沿当前 AST 结构的表达式/语句子字段递归，不改写 AST 或编译器状态。 */
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
            /* 调用节点仅持有参数列表；可调用表达式本身位于外层 PrimaryExpression.property。 */
            collect_identifiers_from_array(cs, funcCall->args, identifierNames);
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *memberExpr = &node->data.memberExpression;
            /* 点号后的标识符是字段名而非变量引用；只有方括号计算属性才需扫描。接收者由 PrimaryExpression 负责。 */
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
            /* 对象字面量的普通标识符/字符串键是静态名称；只遍历计算键表达式和值表达式。 */
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
            /* BUG: 外层扫描递归进入嵌套 lambda 的 block，却没有把 lambda->params 当作内层绑定屏蔽；当形参遮蔽父级同名 Borrow/Loan 局部变量时，外层会误记捕获并报“Borrowed and loaned owners cannot escape through closure capture”。触发路径：参数字段见 zr_vm_parser/include/zr_vm_parser/ast.h:456-459；lambda 在清空子级局部表后、编译 body 前执行分析见 zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c:222-224,310。 */
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
            if (usingStmt->elseBody != ZR_NULL) {
                collect_identifiers_from_node(cs, usingStmt->elseBody, identifierNames);
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
            /* BUG: ZR_AST_TEMPLATE_STRING_LITERAL 及其 ZR_AST_INTERPOLATED_SEGMENT.expression 不会进入 visitor；lambda 只在插值中引用父级局部变量时不会登记捕获。lambda 先清空子级 localVars/closureVars 再调用本分析（zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_lambda.c:222-224,310），随后插值编译 identifier 时只查子级 local/closure 并报告未解析名字（zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c:425,444,531；表达式入口见 zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c:261-264）。AST 子字段和构造路径见 zr_vm_parser/include/zr_vm_parser/ast.h:403-409、zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c:135,348。 */
            /* TODO: 还需对照 ast.h 的完整表达式 AST/子字段表，补齐类型转换、await、spread、解包和生成器等当前未递归的包装节点，并逐类补捕获测试。 */
            break;
    }
}

/* 分析闭包体中未在当前编译器绑定的标识符，并把父级局部或闭包槽复制为子闭包捕获。 */
void ZrParser_ExternalVariables_Analyze(SZrCompilerState *cs, SZrAstNode *node, SZrCompilerState *parentCompiler) {
    if (cs == ZR_NULL || node == ZR_NULL || parentCompiler == ZR_NULL || cs->hasError) {
        return;
    }
    
    /* 先建立临时名字数组；数组只存 SZrString*，清理时不释放字符串对象。 */
    SZrArray identifierNames;
    /* BUG: Array_Init/Push 为无返回值 API；共享实现的 Init 分配失败会保留空 head，Push 仍断言/写入该缓冲区。闭包体含标识符且底层分配失败时，本分析不能转成编译诊断，会中止或空指针写入；closureVars 扩容也有同类路径。证据：zr_vm_core/include/zr_vm_core/array.h:29-38,73-88；下方初始化与追加见本函数。 */
    ZrCore_Array_Init(cs->state, &identifierNames, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    collect_identifiers_from_node(cs, node, &identifierNames);
    
    /* 只处理当前作用域未绑定、但父级存在局部槽或上值索引的名字。 */
    for (TZrSize i = 0; i < identifierNames.length; i++) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get(&identifierNames, i);
        if (namePtr == ZR_NULL || *namePtr == ZR_NULL) {
            continue;
        }
        SZrString *name = *namePtr;
        
        /* 当前绑定优先，避免把自身局部变量误记为外部捕获。 */
        TZrUInt32 localIndex = find_local_var(cs, name);
        TZrUInt32 closureIndex = find_closure_var(cs, name);
        
        // 如果既不是局部变量也不是闭包变量，可能是外部变量
        if (localIndex == ZR_PARSER_SLOT_NONE && closureIndex == ZR_PARSER_INDEX_NONE) {
            /* 父级 local/closure 索引是后续 GETUPVAL/GET_CLOSURE 的来源。 */
            TZrUInt32 parentLocalIndex = find_local_var(parentCompiler, name);
            TZrUInt32 parentClosureIndex = find_closure_var(parentCompiler, name);
            if (parentLocalIndex != ZR_PARSER_SLOT_NONE || parentClosureIndex != ZR_PARSER_INDEX_NONE) {
                /* 捕获只追加一次；index 必须保持父级真实槽位或上值索引，不能使用当前 closureVars 长度。 */
                if (find_closure_var(cs, name) == ZR_PARSER_INDEX_NONE) {
                    SZrFunctionClosureVariable closureVar;

                    ZrCore_Memory_RawSet(&closureVar, 0, sizeof(closureVar));
                    /* 所有权边界在写入 closureVars 前检查，失败时停止后续捕获。 */
                    if (!compiler_validate_closure_capture_ownership_escape(cs,
                                                                            parentCompiler,
                                                                            name,
                                                                            node->location)) {
                        break;
                    }
                    closureVar.name = name;
                    closureVar.inStack = (parentLocalIndex != ZR_PARSER_SLOT_NONE) ? ZR_TRUE : ZR_FALSE;
                    closureVar.index = (parentLocalIndex != ZR_PARSER_SLOT_NONE) ? parentLocalIndex : parentClosureIndex;
                    closureVar.valueType = ZR_VALUE_TYPE_NULL;
                    /* 作用域深度和逃逸标志写入 closureValueList 后供运行时闭包捕获元数据/GC 逃逸路径读取。 */
                    closureVar.scopeDepth = 0u;
                    if (cs->scopeStack.length > 0) {
                        SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
                        if (scope != ZR_NULL) {
                            closureVar.scopeDepth = scope->depth;
                        }
                    }
                    /* 该标记随 capture 元数据传给后续函数/GC 逃逸分析。 */
                    closureVar.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
                    compiler_closure_capture_identity_from_parent(&closureVar,
                                                                   parentCompiler,
                                                                   name,
                                                                   parentLocalIndex,
                                                                   parentClosureIndex);
                    ZrCore_Array_Push(cs->state, &cs->closureVars, &closureVar);
                    cs->closureVarCount++;
                }
            }
        }
    }
    
    /* 临时名字数组不拥有字符串；分析结束后仅释放数组容器。 */
    ZrCore_Array_Free(cs->state, &identifierNames);
}

// 指令优化函数（占位实现，后续用于压缩和优化指令）

// 压缩指令（消除冗余指令、合并指令等）
