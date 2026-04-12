#include "parser_internal.h"

void free_type_info(SZrState *state, SZrType *type) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }

    if (type->name != ZR_NULL) {
        ZrParser_Ast_Free(state, type->name);
    }
    if (type->subType != ZR_NULL) {
        free_type_info(state, type->subType);
        ZrCore_Memory_RawFreeWithType(state->global, type->subType, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (type->arraySizeExpression != ZR_NULL) {
        ZrParser_Ast_Free(state, type->arraySizeExpression);
    }
}

void free_ast_node_array_with_elements(SZrState *state, SZrAstNodeArray *array) {
    if (state == ZR_NULL || array == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < array->count; i++) {
        ZrParser_Ast_Free(state, array->nodes[i]);
    }

    ZrParser_AstNodeArray_Free(state, array);
}

void free_identifier_node_from_ptr(SZrState *state, SZrIdentifier *identifier) {
    if (state == ZR_NULL || identifier == ZR_NULL) {
        return;
    }

    SZrAstNode *nameNode = (SZrAstNode *) ((char *) identifier - offsetof(SZrAstNode, data.identifier));
    if (nameNode != ZR_NULL && nameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrParser_Ast_Free(state, nameNode);
    }
}

void free_parameter_node_from_ptr(SZrState *state, SZrParameter *parameter) {
    if (state == ZR_NULL || parameter == ZR_NULL) {
        return;
    }

    SZrAstNode *parameterNode = (SZrAstNode *) ((char *) parameter - offsetof(SZrAstNode, data.parameter));
    if (parameterNode != ZR_NULL && parameterNode->type == ZR_AST_PARAMETER) {
        ZrParser_Ast_Free(state, parameterNode);
    }
}

void free_owned_type(SZrState *state, SZrType *type) {
    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }

    free_type_info(state, type);
    ZrCore_Memory_RawFreeWithType(state->global, type, sizeof(SZrType), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

void free_generic_declaration(SZrState *state, SZrGenericDeclaration *generic) {
    if (state == ZR_NULL || generic == ZR_NULL) {
        return;
    }

    free_ast_node_array_with_elements(state, generic->params);
    ZrCore_Memory_RawFreeWithType(state->global, generic, sizeof(SZrGenericDeclaration), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

// 释放 AST 节点（递归释放所有子节点）

void ZrParser_Ast_Free(SZrState *state, SZrAstNode *node) {
    if (node == ZR_NULL) {
        return;
    }

    // 递归释放所有子节点
    // 根据节点类型释放相应的资源
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    ZrParser_Ast_Free(state, script->statements->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, script->statements);
            }
            break;
        }
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *func = &node->data.functionDeclaration;
            free_identifier_node_from_ptr(state, func->name);
            free_generic_declaration(state, func->generic);
            free_ast_node_array_with_elements(state, func->params);
            free_parameter_node_from_ptr(state, func->args);
            free_owned_type(state, func->returnType);
            if (func->body != ZR_NULL) {
                ZrParser_Ast_Free(state, func->body);
            }
            free_ast_node_array_with_elements(state, func->decorators);
            break;
        }
        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *test = &node->data.testDeclaration;
            free_identifier_node_from_ptr(state, test->name);
            free_ast_node_array_with_elements(state, test->params);
            free_parameter_node_from_ptr(state, test->args);
            if (test->body != ZR_NULL) {
                ZrParser_Ast_Free(state, test->body);
            }
            break;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *var = &node->data.variableDeclaration;
            if (var->pattern != ZR_NULL) {
                ZrParser_Ast_Free(state, var->pattern);
            }
            if (var->value != ZR_NULL) {
                ZrParser_Ast_Free(state, var->value);
            }
            free_owned_type(state, var->typeInfo);
            break;
        }
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *decl = &node->data.structDeclaration;
            free_identifier_node_from_ptr(state, decl->name);
            free_generic_declaration(state, decl->generic);
            free_ast_node_array_with_elements(state, decl->inherits);
            free_ast_node_array_with_elements(state, decl->members);
            break;
        }
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *decl = &node->data.classDeclaration;
            free_identifier_node_from_ptr(state, decl->name);
            free_generic_declaration(state, decl->generic);
            free_ast_node_array_with_elements(state, decl->inherits);
            free_ast_node_array_with_elements(state, decl->members);
            free_ast_node_array_with_elements(state, decl->decorators);
            break;
        }
        case ZR_AST_STRUCT_FIELD: {
            SZrStructField *field = &node->data.structField;
            free_identifier_node_from_ptr(state, field->name);
            free_owned_type(state, field->typeInfo);
            if (field->init != ZR_NULL) {
                ZrParser_Ast_Free(state, field->init);
            }
            break;
        }
        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &node->data.structMethod;
            free_ast_node_array_with_elements(state, method->decorators);
            free_identifier_node_from_ptr(state, method->name);
            free_generic_declaration(state, method->generic);
            free_ast_node_array_with_elements(state, method->params);
            free_parameter_node_from_ptr(state, method->args);
            free_owned_type(state, method->returnType);
            if (method->body != ZR_NULL) {
                ZrParser_Ast_Free(state, method->body);
            }
            break;
        }
        case ZR_AST_STRUCT_META_FUNCTION: {
            SZrStructMetaFunction *meta = &node->data.structMetaFunction;
            free_identifier_node_from_ptr(state, meta->meta);
            free_ast_node_array_with_elements(state, meta->params);
            free_parameter_node_from_ptr(state, meta->args);
            free_owned_type(state, meta->returnType);
            if (meta->body != ZR_NULL) {
                ZrParser_Ast_Free(state, meta->body);
            }
            break;
        }
        case ZR_AST_CLASS_FIELD: {
            SZrClassField *field = &node->data.classField;
            free_ast_node_array_with_elements(state, field->decorators);
            free_identifier_node_from_ptr(state, field->name);
            free_owned_type(state, field->typeInfo);
            if (field->init != ZR_NULL) {
                ZrParser_Ast_Free(state, field->init);
            }
            break;
        }
        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            free_ast_node_array_with_elements(state, method->decorators);
            free_identifier_node_from_ptr(state, method->name);
            free_generic_declaration(state, method->generic);
            free_ast_node_array_with_elements(state, method->params);
            free_parameter_node_from_ptr(state, method->args);
            free_owned_type(state, method->returnType);
            if (method->body != ZR_NULL) {
                ZrParser_Ast_Free(state, method->body);
            }
            break;
        }
        case ZR_AST_CLASS_PROPERTY: {
            SZrClassProperty *property = &node->data.classProperty;
            free_ast_node_array_with_elements(state, property->decorators);
            if (property->modifier != ZR_NULL) {
                ZrParser_Ast_Free(state, property->modifier);
            }
            break;
        }
        case ZR_AST_CLASS_META_FUNCTION: {
            SZrClassMetaFunction *meta = &node->data.classMetaFunction;
            free_identifier_node_from_ptr(state, meta->meta);
            free_ast_node_array_with_elements(state, meta->params);
            free_parameter_node_from_ptr(state, meta->args);
            free_ast_node_array_with_elements(state, meta->superArgs);
            free_owned_type(state, meta->returnType);
            if (meta->body != ZR_NULL) {
                ZrParser_Ast_Free(state, meta->body);
            }
            break;
        }
        case ZR_AST_PARAMETER: {
            SZrParameter *parameter = &node->data.parameter;
            free_identifier_node_from_ptr(state, parameter->name);
            free_owned_type(state, parameter->typeInfo);
            if (parameter->defaultValue != ZR_NULL) {
                ZrParser_Ast_Free(state, parameter->defaultValue);
            }
            free_ast_node_array_with_elements(state, parameter->genericTypeConstraints);
            free_ast_node_array_with_elements(state, parameter->decorators);
            break;
        }
        case ZR_AST_DECORATOR_EXPRESSION: {
            SZrDecoratorExpression *decorator = &node->data.decoratorExpression;
            if (decorator->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, decorator->expr);
            }
            break;
        }
        case ZR_AST_PROPERTY_GET: {
            SZrPropertyGet *getter = &node->data.propertyGet;
            free_identifier_node_from_ptr(state, getter->name);
            free_owned_type(state, getter->targetType);
            if (getter->body != ZR_NULL) {
                ZrParser_Ast_Free(state, getter->body);
            }
            break;
        }
        case ZR_AST_PROPERTY_SET: {
            SZrPropertySet *setter = &node->data.propertySet;
            free_identifier_node_from_ptr(state, setter->name);
            free_identifier_node_from_ptr(state, setter->param);
            free_owned_type(state, setter->targetType);
            if (setter->body != ZR_NULL) {
                ZrParser_Ast_Free(state, setter->body);
            }
            break;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *expr = &node->data.binaryExpression;
            if (expr->left != ZR_NULL) {
                ZrParser_Ast_Free(state, expr->left);
            }
            if (expr->right != ZR_NULL) {
                ZrParser_Ast_Free(state, expr->right);
            }
            break;
        }
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *expr = &node->data.unaryExpression;
            if (expr->argument != ZR_NULL) {
                ZrParser_Ast_Free(state, expr->argument);
            }
            break;
        }
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *call = &node->data.functionCall;
            // 注意：SZrFunctionCall没有callee成员，函数调用在primary expression中处理
            if (call->args != ZR_NULL) {
                for (TZrSize i = 0; i < call->args->count; i++) {
                    ZrParser_Ast_Free(state, call->args->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, call->args);
            }
            free_ast_node_array_with_elements(state, call->genericArguments);
            if (call->argNames != ZR_NULL) {
                ZrCore_Array_Free(state, call->argNames);
                ZrCore_Memory_RawFreeWithType(state->global, call->argNames, sizeof(SZrArray),
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            break;
        }
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arr = &node->data.arrayLiteral;
            if (arr->elements != ZR_NULL) {
                for (TZrSize i = 0; i < arr->elements->count; i++) {
                    ZrParser_Ast_Free(state, arr->elements->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, arr->elements);
            }
            break;
        }
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *obj = &node->data.objectLiteral;
            if (obj->properties != ZR_NULL) {
                for (TZrSize i = 0; i < obj->properties->count; i++) {
                    ZrParser_Ast_Free(state, obj->properties->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, obj->properties);
            }
            break;
        }
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    ZrParser_Ast_Free(state, block->body->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, block->body);
            }
            break;
        }
        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            if (exprStmt->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, exprStmt->expr);
            }
            break;
        }
        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->resource != ZR_NULL) {
                ZrParser_Ast_Free(state, usingStmt->resource);
            }
            if (usingStmt->body != ZR_NULL) {
                ZrParser_Ast_Free(state, usingStmt->body);
            }
            break;
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            if (ifExpr->condition != ZR_NULL) {
                ZrParser_Ast_Free(state, ifExpr->condition);
            }
            if (ifExpr->thenExpr != ZR_NULL) {
                ZrParser_Ast_Free(state, ifExpr->thenExpr);
            }
            if (ifExpr->elseExpr != ZR_NULL) {
                ZrParser_Ast_Free(state, ifExpr->elseExpr);
            }
            break;
        }
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
            if (condExpr->test != ZR_NULL) {
                ZrParser_Ast_Free(state, condExpr->test);
            }
            if (condExpr->consequent != ZR_NULL) {
                ZrParser_Ast_Free(state, condExpr->consequent);
            }
            if (condExpr->alternate != ZR_NULL) {
                ZrParser_Ast_Free(state, condExpr->alternate);
            }
            break;
        }
        case ZR_AST_SWITCH_EXPRESSION: {
            SZrSwitchExpression *switchExpr = &node->data.switchExpression;
            if (switchExpr->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, switchExpr->expr);
            }
            if (switchExpr->cases != ZR_NULL) {
                for (TZrSize i = 0; i < switchExpr->cases->count; i++) {
                    ZrParser_Ast_Free(state, switchExpr->cases->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, switchExpr->cases);
            }
            if (switchExpr->defaultCase != ZR_NULL) {
                ZrParser_Ast_Free(state, switchExpr->defaultCase);
            }
            break;
        }
        case ZR_AST_LAMBDA_EXPRESSION: {
            SZrLambdaExpression *lambda = &node->data.lambdaExpression;
            if (lambda->params != ZR_NULL) {
                for (TZrSize i = 0; i < lambda->params->count; i++) {
                    ZrParser_Ast_Free(state, lambda->params->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, lambda->params);
            }
            free_parameter_node_from_ptr(state, lambda->args);
            if (lambda->block != ZR_NULL) {
                ZrParser_Ast_Free(state, lambda->block);
            }
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            if (primary->property != ZR_NULL) {
                ZrParser_Ast_Free(state, primary->property);
            }
            if (primary->members != ZR_NULL) {
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    ZrParser_Ast_Free(state, primary->members->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, primary->members);
            }
            break;
        }
        case ZR_AST_TYPE_QUERY_EXPRESSION: {
            SZrTypeQueryExpression *typeQuery = &node->data.typeQueryExpression;
            if (typeQuery->operand != ZR_NULL) {
                ZrParser_Ast_Free(state, typeQuery->operand);
            }
            break;
        }
        case ZR_AST_TYPE_LITERAL_EXPRESSION: {
            SZrTypeLiteralExpression *typeLiteral = &node->data.typeLiteralExpression;
            if (typeLiteral->typeInfo != ZR_NULL) {
                free_owned_type(state, typeLiteral->typeInfo);
            }
            break;
        }
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION: {
            SZrPrototypeReferenceExpression *prototypeRef = &node->data.prototypeReferenceExpression;
            if (prototypeRef->target != ZR_NULL) {
                ZrParser_Ast_Free(state, prototypeRef->target);
            }
            break;
        }
        case ZR_AST_CONSTRUCT_EXPRESSION: {
            SZrConstructExpression *construct = &node->data.constructExpression;
            if (construct->target != ZR_NULL) {
                ZrParser_Ast_Free(state, construct->target);
            }
            if (construct->args != ZR_NULL) {
                for (TZrSize i = 0; i < construct->args->count; i++) {
                    ZrParser_Ast_Free(state, construct->args->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, construct->args);
            }
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *member = &node->data.memberExpression;
            if (member->property != ZR_NULL) {
                ZrParser_Ast_Free(state, member->property);
            }
            break;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assign = &node->data.assignmentExpression;
            if (assign->left != ZR_NULL) {
                ZrParser_Ast_Free(state, assign->left);
            }
            if (assign->right != ZR_NULL) {
                ZrParser_Ast_Free(state, assign->right);
            }
            break;
        }
        case ZR_AST_KEY_VALUE_PAIR: {
            SZrKeyValuePair *kv = &node->data.keyValuePair;
            if (kv->key != ZR_NULL) {
                ZrParser_Ast_Free(state, kv->key);
            }
            if (kv->value != ZR_NULL) {
                ZrParser_Ast_Free(state, kv->value);
            }
            break;
        }
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *loop = &node->data.whileLoop;
            if (loop->cond != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->cond);
            }
            if (loop->block != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->block);
            }
            break;
        }
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP: {
            // 循环语句有cond和block
            SZrWhileLoop *loop = &node->data.whileLoop;
            if (loop->cond != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->cond);
            }
            if (loop->block != ZR_NULL) {
                ZrParser_Ast_Free(state, loop->block);
            }
            break;
        }
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *ret = &node->data.returnStatement;
            if (ret->expr != ZR_NULL) {
                ZrParser_Ast_Free(state, ret->expr);
            }
            break;
        }
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *tryStmt = &node->data.tryCatchFinallyStatement;
            if (tryStmt->block != ZR_NULL) {
                ZrParser_Ast_Free(state, tryStmt->block);
            }
            if (tryStmt->catchClauses != ZR_NULL) {
                for (TZrSize i = 0; i < tryStmt->catchClauses->count; i++) {
                    if (tryStmt->catchClauses->nodes[i] != ZR_NULL) {
                        ZrParser_Ast_Free(state, tryStmt->catchClauses->nodes[i]);
                    }
                }
                ZrParser_AstNodeArray_Free(state, tryStmt->catchClauses);
            }
            if (tryStmt->finallyBlock != ZR_NULL) {
                ZrParser_Ast_Free(state, tryStmt->finallyBlock);
            }
            break;
        }
        case ZR_AST_CATCH_CLAUSE: {
            SZrCatchClause *catchClause = &node->data.catchClause;
            if (catchClause->pattern != ZR_NULL) {
                for (TZrSize i = 0; i < catchClause->pattern->count; i++) {
                    if (catchClause->pattern->nodes[i] != ZR_NULL) {
                        ZrParser_Ast_Free(state, catchClause->pattern->nodes[i]);
                    }
                }
                ZrParser_AstNodeArray_Free(state, catchClause->pattern);
            }
            if (catchClause->block != ZR_NULL) {
                ZrParser_Ast_Free(state, catchClause->block);
            }
            break;
        }
        case ZR_AST_SWITCH_CASE: {
            SZrSwitchCase *switchCase = &node->data.switchCase;
            if (switchCase->value != ZR_NULL) {
                ZrParser_Ast_Free(state, switchCase->value);
            }
            if (switchCase->block != ZR_NULL) {
                ZrParser_Ast_Free(state, switchCase->block);
            }
            break;
        }
        case ZR_AST_SWITCH_DEFAULT: {
            SZrSwitchDefault *switchDefault = &node->data.switchDefault;
            if (switchDefault->block != ZR_NULL) {
                ZrParser_Ast_Free(state, switchDefault->block);
            }
            break;
        }
        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    ZrParser_Ast_Free(state, templateLiteral->segments->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, templateLiteral->segments);
            }
            break;
        }
        case ZR_AST_INTERPOLATED_SEGMENT: {
            SZrInterpolatedSegment *segment = &node->data.interpolatedSegment;
            if (segment->expression != ZR_NULL) {
                ZrParser_Ast_Free(state, segment->expression);
            }
            break;
        }
        case ZR_AST_TYPE: {
            free_type_info(state, &node->data.type);
            break;
        }
        case ZR_AST_FUNCTION_TYPE: {
            SZrFunctionType *funcType = &node->data.functionType;
            if (funcType->generic != ZR_NULL) {
                free_generic_declaration(state, funcType->generic);
            }
            if (funcType->params != ZR_NULL) {
                for (TZrSize i = 0; i < funcType->params->count; i++) {
                    ZrParser_Ast_Free(state, funcType->params->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, funcType->params);
            }
            free_parameter_node_from_ptr(state, funcType->args);
            if (funcType->returnType != ZR_NULL) {
                free_owned_type(state, funcType->returnType);
            }
            break;
        }
        case ZR_AST_GENERIC_TYPE: {
            SZrGenericType *generic = &node->data.genericType;
            if (generic->params != ZR_NULL) {
                for (TZrSize i = 0; i < generic->params->count; i++) {
                    ZrParser_Ast_Free(state, generic->params->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, generic->params);
            }
            if (generic->name != ZR_NULL) {
                SZrAstNode *nameNode = (SZrAstNode *) ((char *) generic->name - offsetof(SZrAstNode, data.identifier));
                if (nameNode != ZR_NULL && nameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
                    ZrParser_Ast_Free(state, nameNode);
                }
            }
            break;
        }
        case ZR_AST_TUPLE_TYPE: {
            SZrTupleType *tuple = &node->data.tupleType;
            if (tuple->elements != ZR_NULL) {
                for (TZrSize i = 0; i < tuple->elements->count; i++) {
                    ZrParser_Ast_Free(state, tuple->elements->nodes[i]);
                }
                ZrParser_AstNodeArray_Free(state, tuple->elements);
            }
            break;
        }
        // 其他节点类型（字面量、标识符等）通常没有子节点，不需要递归释放
        default:
            // TODO: 对于未知节点类型，暂时不释放子节点（避免错误）
            break;
    }

    // 释放节点本身
    ZrCore_Memory_RawFreeWithType(state->global, node, sizeof(SZrAstNode), ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

// 解析结构体字段
