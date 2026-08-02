#include "debug_evaluation_effect_internal.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/type_inference.h"

#include <string.h>

TZrBool zr_debug_formal_has_paused_array_index_facts(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact);

static TZrBool zr_debug_formal_is_reference_free_value_expression(const SZrAstNode *expression) {
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (expression->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_NULL_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
            return ZR_TRUE;
        case ZR_AST_UNARY_EXPRESSION:
            return zr_debug_formal_is_reference_free_value_expression(
                    expression->data.unaryExpression.argument);
        case ZR_AST_BINARY_EXPRESSION:
            return (TZrBool)(
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.binaryExpression.left) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.binaryExpression.right));
        case ZR_AST_LOGICAL_EXPRESSION:
            return (TZrBool)(
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.logicalExpression.left) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.logicalExpression.right));
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return (TZrBool)(
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.conditionalExpression.test) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.conditionalExpression.consequent) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.conditionalExpression.alternate));
        case ZR_AST_PRIMARY_EXPRESSION:
            return (TZrBool)(
                    (expression->data.primaryExpression.members == ZR_NULL ||
                     expression->data.primaryExpression.members->count == 0u) &&
                    zr_debug_formal_is_reference_free_value_expression(
                            expression->data.primaryExpression.property));
        default:
            return ZR_FALSE;
    }
}

static TZrBool zr_debug_formal_allows_legacy_live_scope_compatibility(
        const SZrSemanticContext *semanticContext,
        const SZrAstNode *expression) {
    TZrBool hasCompatibleReference = ZR_FALSE;
    TZrSize index;

    if (semanticContext == ZR_NULL || !semanticContext->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    if (semanticContext->referenceFacts.length == 0u) {
        return expression != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
    for (index = 0u; index < semanticContext->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&semanticContext->referenceFacts,
                        index);

        if (fact == ZR_NULL) {
            continue;
        }
        if (fact->isResolved) {
            if (fact->kind == ZR_SEMANTIC_REFERENCE_READ &&
                fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
                hasCompatibleReference = ZR_TRUE;
            } else if (fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
                hasCompatibleReference = ZR_TRUE;
            }
            continue;
        }
        if (fact->kind != ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
            return ZR_FALSE;
        }
        hasCompatibleReference = ZR_TRUE;
    }
    return hasCompatibleReference;
}

void zr_debug_formal_free_prepared_expression(SZrDebugFormalEvaluationContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    if (context->inferredTypeInitialized) {
        ZrParser_InferredType_Free(context->compilerState.state, &context->inferredType);
    }
    if (context->compilerStateInitialized) {
        ZrParser_CompilerState_Free(&context->compilerState);
    }
    if (context->expression != ZR_NULL) {
        ZrParser_Ast_Free(context->parserState.state, context->expression);
    }
    if (context->parserStateInitialized) {
        ZrParser_State_Free(&context->parserState);
    }
    memset(context, 0, sizeof(*context));
}

TZrBool zr_debug_formal_prepare_expression(ZrDebugAgent *agent,
                                           TZrUInt32 frameId,
                                           const TZrChar *expression,
                                           SZrDebugFormalEvaluationContext *outContext,
                                           TZrChar *errorBuffer,
                                           TZrSize errorBufferSize) {
    SZrString *sourceName;

    if (outContext == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outContext, 0, sizeof(*outContext));
    if (agent == ZR_NULL || agent->state == ZR_NULL || expression == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid debug formal evaluation request");
        return ZR_FALSE;
    }

    sourceName = ZrCore_String_CreateFromNative(agent->state, "<debug:formal-evaluation>");
    ZrParser_State_Init(&outContext->parserState,
                        agent->state,
                        expression,
                        strlen(expression),
                        sourceName);
    outContext->parserStateInitialized = ZR_TRUE;
    outContext->parserState.suppressErrorOutput = ZR_TRUE;
    outContext->expression = ZrParser_ParseExpressionWithState(&outContext->parserState);
    if (outContext->parserState.hasError || outContext->expression == ZR_NULL) {
        zr_debug_copy_text(errorBuffer,
                           errorBufferSize,
                           outContext->parserState.errorMessage != ZR_NULL
                                   ? outContext->parserState.errorMessage
                                   : "failed to parse debug evaluation expression");
        zr_debug_formal_free_prepared_expression(outContext);
        return ZR_FALSE;
    }

    ZrParser_CompilerState_Init(&outContext->compilerState, agent->state);
    outContext->compilerStateInitialized = ZR_TRUE;
    outContext->compilerState.currentAst = outContext->expression;
    outContext->compilerState.scriptAst = outContext->expression;
    outContext->compilerState.suppressErrorOutput = ZR_TRUE;
    if (!zr_debug_semantic_register_bindings(agent, frameId, &outContext->compilerState)) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(agent->state, &outContext->inferredType, ZR_VALUE_TYPE_OBJECT);
    outContext->inferredTypeInitialized = ZR_TRUE;
    if (!ZrParser_ExpressionType_Infer(
                &outContext->compilerState, outContext->expression, &outContext->inferredType) ||
        outContext->compilerState.hasError) {
        return ZR_TRUE;
    }

    outContext->expressionFact = ZrParser_SemanticFacts_FindExpressionByNode(
            outContext->compilerState.semanticContext,
            outContext->expression);
    outContext->hasCanonicalFacts = zr_debug_evaluation_effect_has_canonical_facts(
            outContext->compilerState.semanticContext,
            outContext->expression,
            outContext->expressionFact);
    if (!outContext->hasCanonicalFacts) {
        outContext->hasCanonicalFacts = zr_debug_formal_has_paused_array_index_facts(
                agent,
                frameId,
                outContext->compilerState.semanticContext,
                outContext->expression,
                outContext->expressionFact);
    }
    return ZR_TRUE;
}

TZrBool zr_debug_formal_evaluate_expression(ZrDebugAgent *agent,
                                            TZrUInt32 frameId,
                                            const TZrChar *expression,
                                            TZrUInt32 allowedEffectFlags,
                                            SZrTypeValue *outValue,
                                            TZrUInt32 *outCanonicalTypeId,
                                            TZrChar *errorBuffer,
                                            TZrSize errorBufferSize,
                                            TZrBool *outHandled,
                                            TZrBool *outParserFailure) {
    SZrDebugFormalEvaluationContext context;
    TZrBool supported = ZR_FALSE;
    TZrBool ok = ZR_TRUE;
    TZrUInt32 effectFlags = ZR_DEBUG_EVALUATION_EFFECT_NONE;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticTypeQuery typeQuery;

    if (outHandled != ZR_NULL) {
        *outHandled = ZR_FALSE;
    }
    if (outParserFailure != ZR_NULL) {
        *outParserFailure = ZR_FALSE;
    }
    if (outCanonicalTypeId != ZR_NULL) {
        *outCanonicalTypeId = ZR_DEBUG_CANONICAL_TYPE_ID_INVALID;
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || expression == ZR_NULL ||
        outValue == ZR_NULL || outHandled == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&context, 0, sizeof(context));
    if (!zr_debug_formal_prepare_expression(agent,
                                            frameId,
                                            expression,
                                            &context,
                                            errorBuffer,
                                            errorBufferSize)) {
        /* The caller may select the legacy syntax-compatibility route explicitly. */
        if (outParserFailure != ZR_NULL) {
            *outParserFailure = ZR_TRUE;
        }
        return ZR_FALSE;
    }

    zr_debug_evaluation_effect_classify_structure(context.expression, &effectFlags);
    if (context.hasCanonicalFacts) {
        zr_debug_evaluation_effect_classify_resolved_properties(
                context.compilerState.semanticContext,
                context.expression,
                &effectFlags);
    }
    if ((effectFlags & ZR_DEBUG_EVALUATION_EFFECT_MUTATION) != 0u) {
        zr_debug_copy_text(
                errorBuffer,
                errorBufferSize,
                "Assignment is not allowed in safe debug evaluate. Cause: debug evaluate and conditional "
                "breakpoints are read-only and must not mutate program state. Suggestion: inspect an "
                "expression without '=' or change the program state from source code instead.");
        ok = ZR_FALSE;
        goto cleanup;
    }
    if (!context.hasCanonicalFacts &&
        !zr_debug_formal_is_reference_free_value_expression(context.expression)) {
        if (agent->entryFunction != ZR_NULL &&
            effectFlags == ZR_DEBUG_EVALUATION_EFFECT_NONE &&
            zr_debug_formal_allows_legacy_live_scope_compatibility(
                    context.compilerState.semanticContext,
                    context.expression)) {
            /* The caller owns compatibility for exact live-scope metadata gaps. */
            zr_debug_copy_text(errorBuffer,
                               errorBufferSize,
                               "canonical semantic facts are unavailable for debug evaluation");
            goto cleanup;
        }
        zr_debug_copy_text(errorBuffer,
                           errorBufferSize,
                           "canonical semantic facts are unavailable for debug evaluation");
        ok = ZR_FALSE;
        goto cleanup;
    }
    if (outCanonicalTypeId != ZR_NULL) {
        ZrParser_SemanticQueryScope_Node(&scope, context.expression);
        memset(&typeQuery, 0, sizeof(typeQuery));
        if (ZrParser_SemanticQuery_CanonicalTypeAt(context.compilerState.semanticContext,
                                                    context.expression->location,
                                                    &scope,
                                                    &typeQuery)) {
            *outCanonicalTypeId = typeQuery.typeId;
        }
    }
    if ((effectFlags & ~allowedEffectFlags) != 0u) {
        zr_debug_copy_text(errorBuffer,
                           errorBufferSize,
                           "debug evaluation requires an explicit capability for canonical effects");
        ok = ZR_FALSE;
        goto cleanup;
    }

    ok = zr_debug_formal_evaluate_node(agent,
                                       frameId,
                                       context.compilerState.semanticContext,
                                       context.expression,
                                       outValue,
                                       &supported,
                                       errorBuffer,
                                       errorBufferSize);
    *outHandled = supported;

cleanup:
    zr_debug_formal_free_prepared_expression(&context);
    return ok;
}
