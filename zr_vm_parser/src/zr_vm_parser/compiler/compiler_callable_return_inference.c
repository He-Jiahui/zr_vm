#include "compiler_internal.h"

static TZrBool compiler_try_infer_expression_type_soft(
        SZrCompilerState *cs,
        SZrAstNode *expr,
        SZrInferredType *result) {
    TZrBool savedHasError;
    TZrBool savedHadRecoverableError;
    TZrBool savedHasFatalError;
    TZrBool savedHasCompileTimeError;
    SZrFileRange savedErrorLocation;
    const TZrChar *savedErrorMessage;

    if (cs == ZR_NULL || expr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    savedHasError = cs->hasError;
    savedHadRecoverableError = cs->hadRecoverableError;
    savedHasFatalError = cs->hasFatalError;
    savedHasCompileTimeError = cs->hasCompileTimeError;
    savedErrorLocation = cs->errorLocation;
    savedErrorMessage = cs->errorMessage;

    cs->hasError = ZR_FALSE;
    cs->hadRecoverableError = ZR_FALSE;
    cs->hasFatalError = ZR_FALSE;

    if (ZrParser_ExpressionType_Infer(cs, expr, result)) {
        return ZR_TRUE;
    }

    cs->hasError = savedHasError;
    cs->hadRecoverableError = savedHadRecoverableError;
    cs->hasFatalError = savedHasFatalError;
    cs->hasCompileTimeError = savedHasCompileTimeError;
    cs->errorLocation = savedErrorLocation;
    cs->errorMessage = savedErrorMessage;
    return ZR_FALSE;
}

static TZrBool compiler_callable_return_type_is_exact(
        const SZrInferredType *type) {
    return type != ZR_NULL &&
           !(type->baseType == ZR_VALUE_TYPE_OBJECT &&
             type->typeName == ZR_NULL &&
             (!type->elementTypes.isValid || type->elementTypes.length == 0U));
}

static SZrFileRange compiler_callable_diagnostic_range(
        const SZrCompilerState *cs,
        SZrFileRange fallback) {
    const SZrAstNode *node = cs != ZR_NULL ? cs->currentFunctionNode : ZR_NULL;

    if (node == ZR_NULL) {
        return fallback;
    }
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return node->data.functionDeclaration.nameLocation;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.nameLocation;
        default:
            return node->location;
    }
}

static void compiler_report_return_type_not_provable(
        SZrCompilerState *cs,
        SZrFileRange firstReturnRange,
        SZrFileRange conflictingReturnRange) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->hasError) {
        return;
    }

    location = compiler_callable_diagnostic_range(cs, conflictingReturnRange);
    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "return_type_not_provable",
                "return type not provable",
                "The callable has no declared return type and its return expressions do not establish one exact common type.",
                "Add an explicit return type or make every return expression use a compatible exact type.") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION) ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                cs->state,
                &diagnostic,
                firstReturnRange,
                "First inferred return type originates here") ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                cs->state,
                &diagnostic,
                conflictingReturnRange,
                "Conflicting or imprecise return type originates here")) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        ZrParser_Compiler_Error(cs, "return type not provable", location);
        return;
    }

    ZrParser_Compiler_StructuredError(cs, &diagnostic);
}

static void compiler_merge_callable_return_type(
        SZrCompilerState *cs,
        const SZrInferredType *candidateType,
        SZrFileRange candidateRange,
        TZrBool *hasReturnType,
        SZrInferredType *accumulatedType,
        SZrFileRange *firstReturnRange) {
    SZrInferredType mergedType;

    if (cs == ZR_NULL || candidateType == ZR_NULL || hasReturnType == ZR_NULL ||
        accumulatedType == ZR_NULL || firstReturnRange == ZR_NULL) {
        return;
    }

    if (!*hasReturnType) {
        ZrParser_InferredType_Copy(cs->state, accumulatedType, candidateType);
        *hasReturnType = ZR_TRUE;
        *firstReturnRange = candidateRange;
        return;
    }

    if (!compiler_callable_return_type_is_exact(accumulatedType) ||
        !compiler_callable_return_type_is_exact(candidateType)) {
        if (compiler_callable_return_type_is_exact(accumulatedType)) {
            ZrParser_InferredType_Free(cs->state, accumulatedType);
            ZrParser_InferredType_Init(
                    cs->state, accumulatedType, ZR_VALUE_TYPE_OBJECT);
        }
        return;
    }

    if (ZrParser_InferredType_Equal(accumulatedType, candidateType)) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &mergedType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_InferredType_GetCommonType(
                cs->state, &mergedType, accumulatedType, candidateType) &&
        compiler_callable_return_type_is_exact(&mergedType)) {
        ZrParser_InferredType_Free(cs->state, accumulatedType);
        ZrParser_InferredType_Copy(cs->state, accumulatedType, &mergedType);
    } else {
        compiler_report_return_type_not_provable(
                cs, *firstReturnRange, candidateRange);
    }
    ZrParser_InferredType_Free(cs->state, &mergedType);
}

static void compiler_callable_type_ref_init_unknown(
        SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static void compiler_callable_type_ref_from_inferred(
        SZrFunctionTypedTypeRef *dest,
        const SZrInferredType *src) {
    if (dest == ZR_NULL) {
        return;
    }

    compiler_callable_type_ref_init_unknown(dest);
    if (src == ZR_NULL) {
        return;
    }

    dest->baseType = src->baseType;
    dest->isNullable = src->isNullable;
    dest->ownershipQualifier = src->ownershipQualifier;
    dest->typeName = src->typeName;
    if (src->baseType == ZR_VALUE_TYPE_ARRAY) {
        dest->isArray = ZR_TRUE;
        if (src->elementTypes.length > 0U) {
            const SZrInferredType *elementType =
                    (const SZrInferredType *)ZrCore_Array_Get(
                            (SZrArray *)&src->elementTypes, 0U);
            if (elementType != ZR_NULL) {
                dest->elementBaseType = elementType->baseType;
                dest->elementTypeName = elementType->typeName;
            }
        }
    }
}

static void compiler_register_function_like_pattern_bindings(
        SZrCompilerState *cs,
        SZrAstNode *pattern) {
    SZrAstNodeArray *entries = ZR_NULL;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || pattern == ZR_NULL) {
        return;
    }
    if (pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrInferredType unknownType;

        if (pattern->data.identifier.name == ZR_NULL) {
            return;
        }
        ZrParser_InferredType_Init(cs->state, &unknownType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_TypeEnvironment_RegisterVariable(
                cs->state,
                cs->typeEnv,
                pattern->data.identifier.name,
                &unknownType);
        ZrParser_InferredType_Free(cs->state, &unknownType);
        return;
    }
    if (pattern->type == ZR_AST_KEY_VALUE_PAIR) {
        if (!pattern->data.keyValuePair.keyIsComputed) {
            compiler_register_function_like_pattern_bindings(
                    cs, pattern->data.keyValuePair.key);
        }
        return;
    }
    if (pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        entries = pattern->data.destructuringObject.keys;
    } else if (pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        entries = pattern->data.destructuringArray.keys;
    }
    if (entries == ZR_NULL || entries->nodes == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0U; index < entries->count; index++) {
        compiler_register_function_like_pattern_bindings(
                cs, entries->nodes[index]);
    }
}

static void compiler_register_function_like_local_variable_type(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrVariableDeclaration *declaration;
    SZrInferredType bindingType;
    TZrBool hasBindingType = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL ||
        node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    declaration = &node->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL) {
        return;
    }
    if (declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->pattern->data.identifier.name == ZR_NULL) {
        compiler_register_function_like_pattern_bindings(
                cs, declaration->pattern);
        return;
    }

    ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (declaration->typeInfo != ZR_NULL) {
        hasBindingType = ZrParser_AstTypeToInferredType_Convert(
                cs, declaration->typeInfo, &bindingType);
    } else if (declaration->value != ZR_NULL) {
        hasBindingType = ZrParser_ExpressionType_Infer(
                cs, declaration->value, &bindingType);
    }

    if (cs->hasError) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
        return;
    }

    if (!hasBindingType) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
        ZrParser_InferredType_Init(cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    }

    ZrParser_TypeEnvironment_RegisterVariable(
            cs->state,
            cs->typeEnv,
            declaration->pattern->data.identifier.name,
            &bindingType);
    ZrParser_InferredType_Free(cs->state, &bindingType);
}

static void compiler_collect_function_like_return_type(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrBool *hasReturnType,
        SZrInferredType *accumulatedType,
        SZrFileRange *firstReturnRange) {
    if (cs == ZR_NULL || node == ZR_NULL || hasReturnType == ZR_NULL ||
        accumulatedType == ZR_NULL || firstReturnRange == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body == ZR_NULL) {
                return;
            }

            for (TZrSize index = 0U; index < block->body->count; index++) {
                compiler_collect_function_like_return_type(
                        cs,
                        block->body->nodes[index],
                        hasReturnType,
                        accumulatedType,
                        firstReturnRange);
                if (cs->hasError) {
                    return;
                }
            }
            return;
        }

        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStatement = &node->data.returnStatement;
            SZrInferredType candidateType;

            ZrParser_InferredType_Init(
                    cs->state, &candidateType, ZR_VALUE_TYPE_NULL);
            if (returnStatement->expr != ZR_NULL &&
                !compiler_try_infer_expression_type_soft(
                        cs, returnStatement->expr, &candidateType)) {
                ZrParser_InferredType_Free(cs->state, &candidateType);
                ZrParser_InferredType_Init(
                        cs->state, &candidateType, ZR_VALUE_TYPE_OBJECT);
            }

            compiler_merge_callable_return_type(
                    cs,
                    &candidateType,
                    returnStatement->expr != ZR_NULL
                            ? returnStatement->expr->location
                            : node->location,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            ZrParser_InferredType_Free(cs->state, &candidateType);
            return;
        }

        case ZR_AST_VARIABLE_DECLARATION:
            compiler_register_function_like_local_variable_type(cs, node);
            return;

        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpression = &node->data.ifExpression;
            compiler_collect_function_like_return_type(
                    cs,
                    ifExpression->thenExpr,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            if (cs->hasError) {
                return;
            }
            compiler_collect_function_like_return_type(
                    cs,
                    ifExpression->elseExpr,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            return;
        }

        case ZR_AST_WHILE_LOOP:
            compiler_collect_function_like_return_type(
                    cs,
                    node->data.whileLoop.block,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            return;

        case ZR_AST_FOR_LOOP:
            compiler_collect_function_like_return_type(
                    cs,
                    node->data.forLoop.block,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            return;

        case ZR_AST_FOREACH_LOOP:
            compiler_collect_function_like_return_type(
                    cs,
                    node->data.foreachLoop.block,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            return;

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *tryStatement =
                    &node->data.tryCatchFinallyStatement;
            compiler_collect_function_like_return_type(
                    cs,
                    tryStatement->block,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            if (cs->hasError) {
                return;
            }

            if (tryStatement->catchClauses != ZR_NULL) {
                for (TZrSize index = 0U;
                     index < tryStatement->catchClauses->count;
                     index++) {
                    SZrAstNode *catchNode =
                            tryStatement->catchClauses->nodes[index];
                    if (catchNode == ZR_NULL ||
                        catchNode->type != ZR_AST_CATCH_CLAUSE) {
                        continue;
                    }

                    compiler_collect_function_like_return_type(
                            cs,
                            catchNode->data.catchClause.block,
                            hasReturnType,
                            accumulatedType,
                            firstReturnRange);
                    if (cs->hasError) {
                        return;
                    }
                }
            }

            compiler_collect_function_like_return_type(
                    cs,
                    tryStatement->finallyBlock,
                    hasReturnType,
                    accumulatedType,
                    firstReturnRange);
            return;
        }

        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_LAMBDA_EXPRESSION:
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
            return;

        default:
            return;
    }
}

TZrBool compiler_build_callable_return_type_metadata_with_inferred(
        SZrCompilerState *cs,
        SZrType *declaredReturnType,
        SZrAstNode *bodyNode,
        SZrFunctionTypedTypeRef *outType,
        TZrBool *outHasType,
        SZrInferredType *outInferredType) {
    SZrInferredType inferredType;
    TZrBool hasReturnType = ZR_FALSE;
    SZrFileRange firstReturnRange;

    if (outType == ZR_NULL || outHasType == ZR_NULL) {
        return ZR_FALSE;
    }

    *outHasType = ZR_FALSE;
    compiler_callable_type_ref_init_unknown(outType);
    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (declaredReturnType != ZR_NULL) {
        ZrParser_InferredType_Init(
                cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_AstTypeToInferredType_Convert(
                    cs, declaredReturnType, &inferredType)) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return ZR_FALSE;
        }
        compiler_callable_type_ref_from_inferred(outType, &inferredType);
        if (outInferredType != ZR_NULL) {
            ZrParser_InferredType_Copy(
                    cs->state, outInferredType, &inferredType);
        }
        ZrParser_InferredType_Free(cs->state, &inferredType);
        *outHasType = ZR_TRUE;
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(
            cs->state, &inferredType, ZR_VALUE_TYPE_NULL);
    ZrCore_Memory_RawSet(&firstReturnRange, 0, sizeof(firstReturnRange));
    if (bodyNode != ZR_NULL) {
        compiler_collect_function_like_return_type(
                cs,
                bodyNode,
                &hasReturnType,
                &inferredType,
                &firstReturnRange);
        if (cs->hasError) {
            ZrParser_InferredType_Free(cs->state, &inferredType);
            return cs->hasStructuredError;
        }
    }

    if (!hasReturnType) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        ZrParser_InferredType_Init(
                cs->state, &inferredType, ZR_VALUE_TYPE_NULL);
        hasReturnType = ZR_TRUE;
    }

    compiler_callable_type_ref_from_inferred(outType, &inferredType);
    if (outInferredType != ZR_NULL) {
        ZrParser_InferredType_Copy(cs->state, outInferredType, &inferredType);
    }
    *outHasType = hasReturnType;
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return ZR_TRUE;
}

TZrBool compiler_build_callable_return_type_metadata(
        SZrCompilerState *cs,
        SZrType *declaredReturnType,
        SZrAstNode *bodyNode,
        SZrFunctionTypedTypeRef *outType,
        TZrBool *outHasType) {
    return compiler_build_callable_return_type_metadata_with_inferred(
            cs,
            declaredReturnType,
            bodyNode,
            outType,
            outHasType,
            ZR_NULL);
}

TZrBool ZrParser_Compiler_InferCallableReturnType(
        SZrCompilerState *cs,
        const SZrAstNode *declaration,
        SZrInferredType *result) {
    SZrFunctionTypedTypeRef metadata;
    SZrInferredType inferredType;
    SZrAstNode *savedCurrentFunctionNode;
    SZrType *declaredReturnType = ZR_NULL;
    SZrAstNode *body = ZR_NULL;
    TZrBool hasType = ZR_FALSE;
    TZrBool built;

    if (cs == ZR_NULL || cs->state == ZR_NULL || declaration == ZR_NULL ||
        result == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            declaredReturnType =
                    declaration->data.functionDeclaration.returnType;
            body = declaration->data.functionDeclaration.body;
            break;
        case ZR_AST_CLASS_METHOD:
            declaredReturnType = declaration->data.classMethod.returnType;
            body = declaration->data.classMethod.body;
            break;
        case ZR_AST_CLASS_META_FUNCTION:
            declaredReturnType = declaration->data.classMetaFunction.returnType;
            body = declaration->data.classMetaFunction.body;
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            declaredReturnType = declaration->data.lambdaExpression.returnType;
            body = declaration->data.lambdaExpression.block;
            break;
        default:
            return ZR_FALSE;
    }

    savedCurrentFunctionNode = cs->currentFunctionNode;
    cs->currentFunctionNode = (SZrAstNode *)declaration;
    ZrParser_InferredType_Init(
            cs->state, &inferredType, ZR_VALUE_TYPE_NULL);
    built = compiler_build_callable_return_type_metadata_with_inferred(
            cs,
            declaredReturnType,
            body,
            &metadata,
            &hasType,
            &inferredType);
    cs->currentFunctionNode = savedCurrentFunctionNode;

    if (!built || cs->hasError || !hasType ||
        !compiler_callable_return_type_is_exact(&inferredType)) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(cs->state, result);
    ZrParser_InferredType_Copy(cs->state, result, &inferredType);
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return ZR_TRUE;
}
