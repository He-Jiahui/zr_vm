#include "debug_evaluation_effect_internal.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

static TZrBool zr_debug_evaluation_effect_range_contains(
        const SZrFileRange *outer,
        const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        (outer->source != ZR_NULL && inner->source != ZR_NULL &&
         outer->source != inner->source && !ZrCore_String_Equal(outer->source, inner->source))) {
        return ZR_FALSE;
    }
    if ((outer->start.offset > 0u || outer->end.offset > 0u) &&
        (inner->start.offset > 0u || inner->end.offset > 0u)) {
        return (TZrBool)(outer->start.offset <= inner->start.offset &&
                         inner->end.offset <= outer->end.offset);
    }
    return (TZrBool)(
            (outer->start.line < inner->start.line ||
             (outer->start.line == inner->start.line &&
              outer->start.column <= inner->start.column)) &&
            (inner->end.line < outer->end.line ||
             (inner->end.line == outer->end.line &&
              inner->end.column <= outer->end.column)));
}

static TZrBool zr_debug_evaluation_effect_has_unresolved_reference(
        const SZrSemanticContext *context,
        const SZrFileRange *range) {
    TZrSize index;

    if (context == ZR_NULL || range == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0u; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (reference != ZR_NULL && !reference->isResolved &&
            zr_debug_evaluation_effect_range_contains(range, &reference->range)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_debug_evaluation_effect_identifier_is_resolved(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0u; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (reference != ZR_NULL && reference->node == node && reference->isResolved &&
            reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
            reference->typeId != ZR_SEMANTIC_ID_INVALID) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_debug_evaluation_effect_identifiers_are_resolved(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return zr_debug_evaluation_effect_identifier_is_resolved(context, node);
        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL) {
                for (index = 0u; index < node->data.functionCall.args->count; ++index) {
                    if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                                context,
                                node->data.functionCall.args->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return (TZrBool)(
                    zr_debug_evaluation_effect_identifiers_are_resolved(
                            context,
                            node->data.assignmentExpression.left) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(
                            context,
                            node->data.assignmentExpression.right));
        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                        context,
                        node->data.constructExpression.target)) {
                return ZR_FALSE;
            }
            if (node->data.constructExpression.args != ZR_NULL) {
                for (index = 0u; index < node->data.constructExpression.args->count; ++index) {
                    if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                                context,
                                node->data.constructExpression.args->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_ARRAY_LITERAL:
            if (node->data.arrayLiteral.elements != ZR_NULL) {
                for (index = 0u; index < node->data.arrayLiteral.elements->count; ++index) {
                    if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                                context,
                                node->data.arrayLiteral.elements->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_OBJECT_LITERAL:
            if (node->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0u; index < node->data.objectLiteral.properties->count; ++index) {
                    if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                                context,
                                node->data.objectLiteral.properties->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_BINARY_EXPRESSION:
            return (TZrBool)(
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.binaryExpression.left) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.binaryExpression.right));
        case ZR_AST_LOGICAL_EXPRESSION:
            return (TZrBool)(
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.logicalExpression.left) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.logicalExpression.right));
        case ZR_AST_UNARY_EXPRESSION:
            return zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.unaryExpression.argument);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.typeCastExpression.expression);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.typeQueryExpression.operand);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return (TZrBool)(
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.conditionalExpression.test) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.conditionalExpression.consequent) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.conditionalExpression.alternate));
        case ZR_AST_IF_EXPRESSION:
            return (TZrBool)(
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.ifExpression.condition) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.ifExpression.thenExpr) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.ifExpression.elseExpr));
        case ZR_AST_PRIMARY_EXPRESSION:
            if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                        context,
                        node->data.primaryExpression.property)) {
                TZrBool hasResolvedMember = ZR_FALSE;

                if (node->data.primaryExpression.property == ZR_NULL ||
                    node->data.primaryExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
                    node->data.primaryExpression.members == ZR_NULL) {
                    return ZR_FALSE;
                }
                for (index = 0u; index < node->data.primaryExpression.members->count; ++index) {
                    const SZrAstNode *member = node->data.primaryExpression.members->nodes[index];
                    const SZrAstNode *property =
                            member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION
                                    ? member->data.memberExpression.property
                                    : ZR_NULL;
                    const SZrSemanticReferenceFact *reference =
                            property != ZR_NULL
                                    ? ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                                              context,
                                              property,
                                              ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS)
                                    : ZR_NULL;

                    if (reference != ZR_NULL && reference->isResolved &&
                        reference->symbolId != ZR_SEMANTIC_ID_INVALID &&
                        reference->typeId != ZR_SEMANTIC_ID_INVALID) {
                        hasResolvedMember = ZR_TRUE;
                        break;
                    }
                }
                if (!hasResolvedMember) {
                    return ZR_FALSE;
                }
            }
            if (node->data.primaryExpression.members != ZR_NULL) {
                for (index = 0u; index < node->data.primaryExpression.members->count; ++index) {
                    if (!zr_debug_evaluation_effect_identifiers_are_resolved(
                                context,
                                node->data.primaryExpression.members->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_MEMBER_EXPRESSION:
            return zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.memberExpression.property);
        case ZR_AST_KEY_VALUE_PAIR:
            return (TZrBool)(
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.keyValuePair.key) &&
                    zr_debug_evaluation_effect_identifiers_are_resolved(context, node->data.keyValuePair.value));
        default:
            return ZR_TRUE;
    }
}

void zr_debug_evaluation_effect_classify_structure(const SZrAstNode *node, TZrUInt32 *effectFlags) {
    TZrSize index;

    if (node == ZR_NULL || effectFlags == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_CALL:
            *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_CALL;
            if (node->data.functionCall.args != ZR_NULL) {
                for (index = 0u; index < node->data.functionCall.args->count; ++index) {
                    zr_debug_evaluation_effect_classify_structure(
                            node->data.functionCall.args->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_MUTATION;
            zr_debug_evaluation_effect_classify_structure(node->data.assignmentExpression.left, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.assignmentExpression.right, effectFlags);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION;
            zr_debug_evaluation_effect_classify_structure(node->data.constructExpression.target, effectFlags);
            if (node->data.constructExpression.args != ZR_NULL) {
                for (index = 0u; index < node->data.constructExpression.args->count; ++index) {
                    zr_debug_evaluation_effect_classify_structure(
                            node->data.constructExpression.args->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_ARRAY_LITERAL:
            *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION;
            if (node->data.arrayLiteral.elements != ZR_NULL) {
                for (index = 0u; index < node->data.arrayLiteral.elements->count; ++index) {
                    zr_debug_evaluation_effect_classify_structure(
                            node->data.arrayLiteral.elements->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION;
            if (node->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0u; index < node->data.objectLiteral.properties->count; ++index) {
                    zr_debug_evaluation_effect_classify_structure(
                            node->data.objectLiteral.properties->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION;
            break;
        case ZR_AST_BINARY_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.binaryExpression.left, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.binaryExpression.right, effectFlags);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.logicalExpression.left, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.logicalExpression.right, effectFlags);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.unaryExpression.argument, effectFlags);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.typeCastExpression.expression, effectFlags);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.typeQueryExpression.operand, effectFlags);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.conditionalExpression.test, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.conditionalExpression.consequent, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.conditionalExpression.alternate, effectFlags);
            break;
        case ZR_AST_IF_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.ifExpression.condition, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.ifExpression.thenExpr, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.ifExpression.elseExpr, effectFlags);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.primaryExpression.property, effectFlags);
            if (node->data.primaryExpression.members != ZR_NULL) {
                for (index = 0u; index < node->data.primaryExpression.members->count; ++index) {
                    zr_debug_evaluation_effect_classify_structure(
                            node->data.primaryExpression.members->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            zr_debug_evaluation_effect_classify_structure(node->data.memberExpression.property, effectFlags);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            zr_debug_evaluation_effect_classify_structure(node->data.keyValuePair.key, effectFlags);
            zr_debug_evaluation_effect_classify_structure(node->data.keyValuePair.value, effectFlags);
            break;
        default:
            break;
    }
}

TZrBool zr_debug_evaluation_effect_has_canonical_facts(
        const SZrSemanticContext *context,
        const SZrAstNode *expression,
        const SZrSemanticExpressionFact *expressionFact) {
    return (TZrBool)(
            context != ZR_NULL && expression != ZR_NULL && expressionFact != ZR_NULL &&
            expressionFact->exactness == ZR_SEMANTIC_FACT_EXACT &&
            expressionFact->kind != ZR_SEMANTIC_EXPRESSION_FACT_ERROR &&
            !zr_debug_evaluation_effect_has_unresolved_reference(context, &expressionFact->range) &&
            zr_debug_evaluation_effect_identifiers_are_resolved(context, expression));
}

static void zr_debug_evaluation_effect_classify_resolved_ownership(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        TZrUInt32 *effectFlags) {
    const SZrSemanticOwnershipFact *ownership;

    if (context == ZR_NULL || node == ZR_NULL || effectFlags == ZR_NULL) {
        return;
    }

    ownership = ZrParser_SemanticFacts_FindOwnershipByNode(context, node);
    if (ownership != ZR_NULL && !ownership->isViolation) {
        *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_OWNER_MUTATION;
    }
}

void zr_debug_evaluation_effect_classify_resolved_properties(
        const SZrSemanticContext *context,
        const SZrAstNode *node,
        TZrUInt32 *effectFlags) {
    SZrParserSemanticPropertyQuery property;
    const SZrSemanticReferenceFact *reference;
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || effectFlags == ZR_NULL) {
        return;
    }

    zr_debug_evaluation_effect_classify_resolved_ownership(context, node, effectFlags);

    reference = node->type == ZR_AST_IDENTIFIER_LITERAL
                        ? ZrParser_SemanticFacts_FindReferenceAtPosition(
                                  context,
                                  node->location)
                        : ZR_NULL;
    if (reference != ZR_NULL &&
        (reference->kind == ZR_SEMANTIC_REFERENCE_READ ||
         reference->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) &&
        ZrParser_SemanticQuery_PropertyAt(
                context,
                node->location,
                ZR_NULL,
                &property) &&
        property.getterSymbolId != ZR_SEMANTIC_ID_INVALID) {
        *effectFlags |= ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL) {
                for (index = 0u; index < node->data.functionCall.args->count; ++index) {
                    zr_debug_evaluation_effect_classify_resolved_properties(
                            context,
                            node->data.functionCall.args->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.assignmentExpression.left, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.assignmentExpression.right, effectFlags);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.constructExpression.target, effectFlags);
            if (node->data.constructExpression.args != ZR_NULL) {
                for (index = 0u; index < node->data.constructExpression.args->count; ++index) {
                    zr_debug_evaluation_effect_classify_resolved_properties(
                            context,
                            node->data.constructExpression.args->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_ARRAY_LITERAL:
            if (node->data.arrayLiteral.elements != ZR_NULL) {
                for (index = 0u; index < node->data.arrayLiteral.elements->count; ++index) {
                    zr_debug_evaluation_effect_classify_resolved_properties(
                            context,
                            node->data.arrayLiteral.elements->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            if (node->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0u; index < node->data.objectLiteral.properties->count; ++index) {
                    zr_debug_evaluation_effect_classify_resolved_properties(
                            context,
                            node->data.objectLiteral.properties->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_BINARY_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.binaryExpression.left, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.binaryExpression.right, effectFlags);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.logicalExpression.left, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.logicalExpression.right, effectFlags);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.unaryExpression.argument, effectFlags);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.typeCastExpression.expression, effectFlags);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.typeQueryExpression.operand, effectFlags);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.conditionalExpression.test, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.conditionalExpression.consequent, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.conditionalExpression.alternate, effectFlags);
            break;
        case ZR_AST_IF_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.ifExpression.condition, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.ifExpression.thenExpr, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.ifExpression.elseExpr, effectFlags);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.primaryExpression.property, effectFlags);
            if (node->data.primaryExpression.members != ZR_NULL) {
                for (index = 0u; index < node->data.primaryExpression.members->count; ++index) {
                    zr_debug_evaluation_effect_classify_resolved_properties(
                            context,
                            node->data.primaryExpression.members->nodes[index],
                            effectFlags);
                }
            }
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.memberExpression.property, effectFlags);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.keyValuePair.key, effectFlags);
            zr_debug_evaluation_effect_classify_resolved_properties(
                    context, node->data.keyValuePair.value, effectFlags);
            break;
        default:
            break;
    }
}

TZrBool ZrDebug_ClassifyEvaluationEffect(ZrDebugAgent *agent,
                                         TZrUInt32 frameId,
                                         const TZrChar *expression,
                                         ZrDebugEvaluationEffectPolicy *outPolicy,
                                         TZrChar *errorBuffer,
                                         TZrSize errorBufferSize) {
    SZrDebugFormalEvaluationContext context;

    if (outPolicy != ZR_NULL) {
        memset(outPolicy, 0, sizeof(*outPolicy));
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || agent->state == ZR_NULL || expression == ZR_NULL || outPolicy == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid debug evaluation effect request");
        return ZR_FALSE;
    }

    memset(&context, 0, sizeof(context));
    if (!zr_debug_formal_prepare_expression(agent,
                                            frameId,
                                            expression,
                                            &context,
                                            errorBuffer,
                                            errorBufferSize)) {
        return ZR_FALSE;
    }

    zr_debug_evaluation_effect_classify_structure(context.expression, &outPolicy->effectFlags);
    outPolicy->hasCanonicalFacts = context.hasCanonicalFacts;
    if (context.hasCanonicalFacts) {
        zr_debug_evaluation_effect_classify_resolved_properties(
                context.compilerState.semanticContext,
                context.expression,
                &outPolicy->effectFlags);
    }
    outPolicy->isPure = outPolicy->hasCanonicalFacts &&
                        outPolicy->effectFlags == ZR_DEBUG_EVALUATION_EFFECT_NONE;
    zr_debug_formal_free_prepared_expression(&context);
    return ZR_TRUE;
}

TZrBool ZrDebug_EvaluationEffectPolicy_Allows(
        const ZrDebugEvaluationEffectPolicy *policy,
        TZrUInt32 allowedEffectFlags) {
    if (policy == ZR_NULL || !policy->hasCanonicalFacts) {
        return ZR_FALSE;
    }

    return (TZrBool)((policy->effectFlags & ~allowedEffectFlags) == 0u);
}
