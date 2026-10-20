#include "eval/debug_evaluation_effect_internal.h"

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
    /* 根表达式精确且范围内引用均已解析，才能用语义事实给副作用分类背书。 */
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

    /* AST 结构分类看不到 getter 与所有权动作，此处用已解析事实补齐能力位。 */
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

    /* 分类成功与“可安全执行”不同：仅有规范事实且无副作用位时才标记纯表达式。 */
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

/**
 * @fn TZrBool zr_debug_evaluation_effect_range_contains( const SZrFileRange *outer, const SZrFileRange *inner)
 * @unit zr_debug_evaluation_effect_range_contains
 *
 * ## Summary
 * 为未解析引用筛选表达式内的范围；先比较双方均有的源身份，再按可用坐标选择包含关系。
 *
 * ## Parameters
 * @param[in] outer 借用的外层范围；空指针返回 false。
 * @param[in] inner 借用的候选范围；空指针返回 false。
 *
 * ## Returns
 * @return true 表示当前坐标规则判为包含；不验证范围来源完整、端点次序或 AST 身份。
 *
 * ## Constraints and lifecycle
 * 仅当双方 source 均非空时比较指针或字符串相等；双方各自至少一个 offset 非零时使用 offset，否则比较行列。源缺失与零 offset 不单独构成拒绝。无所有权转移。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:12
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:17
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:22
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:45
 */

/**
 * @fn TZrBool zr_debug_evaluation_effect_has_unresolved_reference( const SZrSemanticContext *context, const SZrFileRange *range)
 * @unit zr_debug_evaluation_effect_has_unresolved_reference
 *
 * ## Summary
 * 在根表达式范围内查找未解析引用，给规范事实门禁提供否定证据。
 *
 * ## Parameters
 * @param[in] context 借用的语义上下文与 referenceFacts 数组。
 * @param[in] range 待检查表达式的范围，须与上下文事实对应。
 *
 * ## Returns
 * @return 存在范围内未解析引用时 true；空参数或 referenceFacts 无效时 false，false 本身不证明事实完整。
 *
 * ## Constraints and lifecycle
 * 只读取当前数组与范围；由 has_canonical_facts 联合根事实精确度、错误类别和标识符身份判断。调用期间上下文及其中字符串须存活且不被并发改写。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:36
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:44
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:329
 */

/**
 * @fn TZrBool zr_debug_evaluation_effect_identifier_is_resolved( const SZrSemanticContext *context, const SZrAstNode *node)
 * @unit zr_debug_evaluation_effect_identifier_is_resolved
 *
 * ## Summary
 * 以同一个 AST 节点指针查找已解析且同时具备 SymbolId 与 TypeId 的引用事实，避免只凭同名文本授信。
 *
 * ## Parameters
 * @param[in] context 持有引用事实的借用上下文。
 * @param[in] node 待核对的标识符节点；不接管 AST。
 *
 * ## Returns
 * @return 存在节点指针相同、isResolved 为真且两个 ID 有效的事实时 true；空参数、无效数组或无匹配时 false。
 *
 * ## Constraints and lifecycle
 * 该检查不独立验证暂停帧的 place、token 或运行时代际；那些检查仍由正式读值路径负责。节点与事实必须来自同一未释放的准备上下文。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:57
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:65
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation_execute.c:72
 */

/**
 * @fn TZrBool zr_debug_evaluation_effect_identifiers_are_resolved( const SZrSemanticContext *context, const SZrAstNode *node)
 * @unit zr_debug_evaluation_effect_identifiers_are_resolved
 *
 * ## Summary
 * 递归核对当前 AST 分派覆盖的操作数引用；主表达式根无直接身份时，允许已解析成员事实作为窄补充。
 *
 * ## Parameters
 * @param[in] context 与 AST 配套的借用语义事实。
 * @param[in] node 待遍历节点；空节点按无需核对处理。
 *
 * ## Returns
 * @return 当前已检查子节点均符合规则时 true；任一必需引用缺失时 false。默认分支返回 true，不是任意未来节点种类的完整性证明。
 *
 * ## Constraints and lifecycle
 * 调用表达式在此遍历实参；根范围未解析引用由另一检查捕获。逻辑与条件表达式同时核对各分支，不因运行时短路跳过。仅遍历有限、合法的 AST，不执行表达式。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:79
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:85
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:166
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:218
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:329
 */

/**
 * @fn void zr_debug_evaluation_effect_classify_structure(const SZrAstNode *node, TZrUInt32 *effectFlags)
 * @unit zr_debug_evaluation_effect_classify_structure
 *
 * ## Summary
 * 从已解析 AST 累加调用、分配和赋值能力位，供分类及正式执行前的门禁使用；收集的是潜在结构作用，不执行这些作用。
 *
 * ## Parameters
 * @param[in] node 借用节点；空节点不改变输出。
 * @param[in,out] effectFlags 调用方初始化的累加位集；只 OR 新位，空指针不处理。
 *
 * ## Returns
 * void；结果通过 effectFlags 累加，不清除已有位，也不报告节点支持度。
 *
 * ## Constraints and lifecycle
 * 逻辑、条件和 if 的所有受支持子分支均参与分类；lambda 计创建时分配，不遍历其函数体。本函数不证明规范事实、不授予能力，也不从 CALL 推断 NATIVE_CALL。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:231
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:257
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:277
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:297
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:379
 */

/**
 * @fn TZrBool zr_debug_evaluation_effect_has_canonical_facts( const SZrSemanticContext *context, const SZrAstNode *expression, const SZrSemanticExpressionFact *expressionFact)
 * @unit zr_debug_evaluation_effect_has_canonical_facts
 *
 * ## Summary
 * 合并根表达式事实的精确度与引用身份检查，为副作用策略提供规范事实门禁。
 *
 * ## Parameters
 * @param[in] context 与 expression 配套的语义上下文。
 * @param[in] expression 借用根 AST 节点。
 * @param[in] expressionFact 借用根表达式事实；须由同一上下文查得。
 *
 * ## Returns
 * @return 仅当三个参数非空、根事实 EXACT 且非 ERROR、范围内无未解析引用、当前标识符检查通过时 true。
 *
 * ## Constraints and lifecycle
 * 不执行表达式，也不检查最终能力白名单。本检查返回 false 后，准备层可另外核对暂停数组整数字面量索引窄路径；该补充不是本函数的返回语义。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:333
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:338
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:274
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:281
 */

/**
 * @fn void zr_debug_evaluation_effect_classify_resolved_ownership( const SZrSemanticContext *context, const SZrAstNode *node, TZrUInt32 *effectFlags)
 * @unit zr_debug_evaluation_effect_classify_resolved_ownership
 *
 * ## Summary
 * 读取当前节点的所有权事实，为具有非违规事实的节点追加 OWNER_MUTATION 能力要求。
 *
 * ## Parameters
 * @param[in] context 借用语义上下文。
 * @param[in] node 待查询的借用 AST 节点。
 * @param[in,out] effectFlags 已初始化的能力累加位集。
 *
 * ## Returns
 * void；空参数、无事实或 isViolation 为真时不变；其余匹配事实 OR OWNER_MUTATION。
 *
 * ## Constraints and lifecycle
 * 该位说明调用者需要考虑的所有权作用，不表示本函数已执行移动、借用或写入。仅由规范事实已获外层认可的语义分类路径使用。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:348
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:352
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:371
 * - tests/debug/test_debug_evaluation_effect_policy_cases.h:531
 */

/**
 * @fn void zr_debug_evaluation_effect_classify_resolved_properties( const SZrSemanticContext *context, const SZrAstNode *node, TZrUInt32 *effectFlags)
 * @unit zr_debug_evaluation_effect_classify_resolved_properties
 *
 * ## Summary
 * 补充结构分类不可见的 getter 与所有权能力要求，供已取得规范事实的上层决定是否执行。
 *
 * ## Parameters
 * @param[in] context 与节点配套且调用期间稳定的语义上下文。
 * @param[in] node 借用 AST；空参数不处理。
 * @param[in,out] effectFlags 已有结构能力位；只累加，不重置。
 *
 * ## Returns
 * void；按查询成功的 getter SymbolId 和非违规所有权事实追加位；查询无匹配不单独报错。
 *
 * ## Constraints and lifecycle
 * 本函数不自行验证 hasCanonicalFacts；两个生产调用点先作该门禁。只查询事实并递归当前支持的子节点，不调用 getter，不把无新增位视为完整或纯的证明。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:370
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:373
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:386
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:390
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:373
 */

/**
 * @fn TZrBool ZrDebug_ClassifyEvaluationEffect(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, ZrDebugEvaluationEffectPolicy *outPolicy, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * @unit ZrDebug_ClassifyEvaluationEffect
 *
 * ## Summary
 * 为调试客户端和测试分析一条表达式的潜在能力要求；分析完成与表达式可执行、纯净或有完整规范事实分开交付。
 *
 * ## Parameters
 * @param[in,out] agent 借用 agent/state；解析和推断使用其 VM 分配资源，不接管 agent。
 * @param[in] frameId 原样传给暂停绑定注册的帧标识；本入口不归一化零值。
 * @param[in] expression 调用期间有效的 NUL 结尾源码；不执行表达式。
 * @param[out] outPolicy 必需的可写策略；非空时入口先清零。
 * @param[out] errorBuffer 可空的诊断区；仅非空且容量大于零时入口写 NUL。
 * @param[in] errorBufferSize errorBuffer 的可写字节容量；零容量不写。
 *
 * ## Returns
 * @return false 表示请求或准备阶段未成功；普通 false 后非空 outPolicy 保持入口零值。true 表示已交付分类，hasCanonicalFacts 仍可为 false；isPure 仅在有规范事实且 effectFlags 为 NONE 时为 true。
 *
 * ## Constraints and lifecycle
 * 成功准备后普通路径释放 context；准备解析失败由准备函数释放。绑定或推断未获完整事实也可能成功返回，不能仅看返回值授权。调用方稳定 agent/state 与所用暂停帧；此层不建立互斥或非局部异常清理，分析本身仍会分配资源。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:514
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:526
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:535
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:544
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:546
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:250
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:261
 */

/**
 * @fn TZrBool ZrDebug_EvaluationEffectPolicy_Allows( const ZrDebugEvaluationEffectPolicy *policy, TZrUInt32 allowedEffectFlags)
 * @unit ZrDebug_EvaluationEffectPolicy_Allows
 *
 * ## Summary
 * 检查已分类策略是否具备规范事实，并且全部作用位都包含在调用者给出的许可集合内。
 *
 * ## Parameters
 * @param[in] policy 借用的策略值；空指针或 hasCanonicalFacts 为 false 一律拒绝。
 * @param[in] allowedEffectFlags 调用者许可的位集；本函数不筛除未知位。
 *
 * ## Returns
 * @return 规范事实存在且 (effectFlags & ~allowedEffectFlags)==0 时 true，否则 false；不读取 isPure。
 *
 * ## Constraints and lifecycle
 * 这是纯位集判断，不执行表达式、不重新分类、也不替代正式执行器的支持度或只读语法门禁。即使允许 MUTATION 位，正式求值仍有独立赋值拒绝分支。
 *
 * ## References
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:553
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_evaluation_effect.c:557
 * - tests/debug/test_debug_evaluation_effect_policy_cases.h:119
 * - zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:379
 */
