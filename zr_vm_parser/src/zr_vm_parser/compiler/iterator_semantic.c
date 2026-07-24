#include "compiler_internal.h"

#include "type_inference_internal.h"

static TZrBool iterator_node_contains_yield(const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->type) {
        case ZR_AST_YIELD_STATEMENT:
            return ZR_TRUE;
        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZR_FALSE;
        case ZR_AST_BLOCK:
            if (node->data.block.body == ZR_NULL) {
                return ZR_FALSE;
            }
            for (index = 0U; index < node->data.block.body->count; index++) {
                if (iterator_node_contains_yield(node->data.block.body->nodes[index])) {
                    return ZR_TRUE;
                }
            }
            return ZR_FALSE;
        case ZR_AST_IF_EXPRESSION:
            return (TZrBool)(
                    iterator_node_contains_yield(node->data.ifExpression.thenExpr) ||
                    iterator_node_contains_yield(node->data.ifExpression.elseExpr));
        case ZR_AST_WHILE_LOOP:
            return iterator_node_contains_yield(node->data.whileLoop.block);
        case ZR_AST_FOR_LOOP:
            return (TZrBool)(
                    iterator_node_contains_yield(node->data.forLoop.init) ||
                    iterator_node_contains_yield(node->data.forLoop.cond) ||
                    iterator_node_contains_yield(node->data.forLoop.step) ||
                    iterator_node_contains_yield(node->data.forLoop.block));
        case ZR_AST_FOREACH_LOOP:
            return iterator_node_contains_yield(node->data.foreachLoop.block);
        case ZR_AST_SWITCH_EXPRESSION:
            if (node->data.switchExpression.cases != ZR_NULL) {
                for (index = 0U; index < node->data.switchExpression.cases->count; index++) {
                    if (iterator_node_contains_yield(node->data.switchExpression.cases->nodes[index])) {
                        return ZR_TRUE;
                    }
                }
            }
            return iterator_node_contains_yield(node->data.switchExpression.defaultCase);
        case ZR_AST_SWITCH_CASE:
            return iterator_node_contains_yield(node->data.switchCase.block);
        case ZR_AST_SWITCH_DEFAULT:
            return iterator_node_contains_yield(node->data.switchDefault.block);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            if (iterator_node_contains_yield(node->data.tryCatchFinallyStatement.block)) {
                return ZR_TRUE;
            }
            if (node->data.tryCatchFinallyStatement.catchClauses != ZR_NULL) {
                for (index = 0U;
                     index < node->data.tryCatchFinallyStatement.catchClauses->count;
                     index++) {
                    if (iterator_node_contains_yield(
                                node->data.tryCatchFinallyStatement.catchClauses->nodes[index])) {
                        return ZR_TRUE;
                    }
                }
            }
            return iterator_node_contains_yield(node->data.tryCatchFinallyStatement.finallyBlock);
        case ZR_AST_CATCH_CLAUSE:
            return iterator_node_contains_yield(node->data.catchClause.block);
        default:
            return ZR_FALSE;
    }
}

TZrBool compiler_iterator_function_contains_yield(const SZrAstNode *body) {
    return iterator_node_contains_yield(body);
}

TZrBool compiler_iterator_current_function_contains_yield(
        const SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->currentFunctionNode == ZR_NULL ||
        cs->currentFunctionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }
    return iterator_node_contains_yield(
            cs->currentFunctionNode->data.functionDeclaration.body);
}

static TZrBool iterator_resolve_current_element_type(
        SZrCompilerState *cs,
        SZrInferredType *outElementType,
        SZrFileRange location) {
    SZrFunctionDeclaration *function;
    SZrInferredType returnType;
    TZrBool resolved = ZR_FALSE;

    if (cs == ZR_NULL || outElementType == ZR_NULL ||
        cs->currentFunctionNode == ZR_NULL ||
        cs->currentFunctionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        if (cs != ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "Yield is only valid inside a function with an explicit Iterator<T> return carrier",
                    location);
        }
        return ZR_FALSE;
    }

    function = &cs->currentFunctionNode->data.functionDeclaration;
    if (function->isAsync || function->returnType == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs,
                "Yield requires an explicit synchronous Iterator<T> return carrier",
                location);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &returnType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_AstTypeToInferredType_Convert(cs, function->returnType, &returnType)) {
        resolved = ZrParser_TypeInference_BindProtocolElementType(
                cs,
                &returnType,
                ZR_PROTOCOL_ID_ITERATOR,
                outElementType);
    }
    ZrParser_InferredType_Free(cs->state, &returnType);

    if (!resolved) {
        ZrParser_Compiler_Error(
                cs,
                "Yield requires an explicit canonical Iterator<T> return carrier",
                location);
    }
    return resolved;
}

void compiler_iterator_compile_yield(SZrCompilerState *cs, SZrAstNode *node) {
    SZrInferredType elementType;
    SZrInferredType yieldedType;
    TZrTypeId elementTypeId;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    if (node->type != ZR_AST_YIELD_STATEMENT || node->data.yieldStatement.expr == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Expected yield expression", node->location);
        return;
    }

    ZrParser_InferredType_Init(cs->state, &elementType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &yieldedType, ZR_VALUE_TYPE_OBJECT);
    if (!iterator_resolve_current_element_type(cs, &elementType, node->location) ||
        !ZrParser_ExpressionType_Infer(cs, node->data.yieldStatement.expr, &yieldedType) ||
        !ZrParser_TypeCompatibility_Check(
                cs, &yieldedType, &elementType, node->data.yieldStatement.expr->location)) {
        ZrParser_InferredType_Free(cs->state, &yieldedType);
        ZrParser_InferredType_Free(cs->state, &elementType);
        return;
    }

    elementTypeId = ZrParser_Semantic_RegisterInferredType(
            cs->semanticContext,
            &elementType,
            ZR_SEMANTIC_TYPE_KIND_VALUE,
            elementType.typeName,
            node->data.yieldStatement.expr);
    if (elementTypeId == ZR_SEMANTIC_ID_INVALID ||
        !compiler_semantic_ir_record_iterator_yield(cs, elementTypeId, node->location)) {
        ZrParser_Compiler_Error(cs, "Failed to record iterator yield semantic facts", node->location);
    }

    ZrParser_InferredType_Free(cs->state, &yieldedType);
    ZrParser_InferredType_Free(cs->state, &elementType);
}
