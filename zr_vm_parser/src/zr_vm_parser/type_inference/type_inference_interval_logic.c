#include "type_inference_semantic_facts.h"

#include <string.h>

typedef struct SZrIntegerInterval {
    TZrInt64 minValue;
    TZrInt64 maxValue;
} SZrIntegerInterval;

static TZrBool type_inference_node_integer_interval(SZrCompilerState *cs,
                                                    SZrAstNode *node,
                                                    SZrIntegerInterval *outInterval) {
    const SZrSemanticNumericFact *fact;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        outInterval == ZR_NULL) {
        return ZR_FALSE;
    }

    fact = ZrParser_SemanticFacts_FindNumericByNode(cs->semanticContext, node);
    if (fact == ZR_NULL ||
        !fact->hasRange ||
        !ZR_VALUE_IS_TYPE_INT(fact->targetType) ||
        fact->minValue > fact->maxValue) {
        return ZR_FALSE;
    }

    outInterval->minValue = fact->minValue;
    outInterval->maxValue = fact->maxValue;
    return ZR_TRUE;
}

static TZrBool type_inference_interval_comparison_value(const TZrChar *op,
                                                        const SZrIntegerInterval *left,
                                                        const SZrIntegerInterval *right,
                                                        TZrBool *outValue) {
    TZrBool isDisjoint;
    TZrBool isSameSingleton;

    if (op == ZR_NULL || left == ZR_NULL || right == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    isDisjoint = left->maxValue < right->minValue || right->maxValue < left->minValue;
    isSameSingleton = left->minValue == left->maxValue &&
                      right->minValue == right->maxValue &&
                      left->minValue == right->minValue;

    if (strcmp(op, "<") == 0) {
        if (left->maxValue < right->minValue) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (left->minValue >= right->maxValue) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
    } else if (strcmp(op, ">") == 0) {
        if (left->minValue > right->maxValue) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (left->maxValue <= right->minValue) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
    } else if (strcmp(op, "<=") == 0) {
        if (left->maxValue <= right->minValue) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (left->minValue > right->maxValue) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
    } else if (strcmp(op, ">=") == 0) {
        if (left->minValue >= right->maxValue) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (left->maxValue < right->minValue) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
    } else if (strcmp(op, "==") == 0) {
        if (isSameSingleton) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (isDisjoint) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
    } else if (strcmp(op, "!=") == 0) {
        if (isDisjoint) {
            *outValue = ZR_TRUE;
            return ZR_TRUE;
        }
        if (isSameSingleton) {
            *outValue = ZR_FALSE;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void type_inference_record_interval_comparison_logical_fact(SZrCompilerState *cs,
                                                            SZrAstNode *node) {
    SZrIntegerInterval leftInterval;
    SZrIntegerInterval rightInterval;
    SZrSemanticLogicalFact fact;
    TZrBool knownValue;
    const TZrChar *op;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_BINARY_EXPRESSION ||
        ZrParser_SemanticFacts_FindLogicalByNode(cs->semanticContext, node) != ZR_NULL) {
        return;
    }

    op = node->data.binaryExpression.op.op;
    if (!type_inference_node_integer_interval(cs, node->data.binaryExpression.left, &leftInterval) ||
        !type_inference_node_integer_interval(cs, node->data.binaryExpression.right, &rightInterval) ||
        !type_inference_interval_comparison_value(op, &leftInterval, &rightInterval, &knownValue)) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = knownValue
                    ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE
                    : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = ZR_TRUE;
    fact.knownValue = knownValue;
    ZrParser_SemanticFacts_AppendLogical(cs->semanticContext, &fact);
}
