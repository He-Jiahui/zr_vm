#include "repl/repl_semantic_facts.h"

#include "repl/repl_semantic_expression_walk.h"

typedef struct SZrCliReplNumericFactWalk {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    const SZrSemanticNumericFact *lastFact;
} SZrCliReplNumericFactWalk;

typedef struct SZrCliReplLogicalFactWalk {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    const SZrSemanticLogicalFact *lastFact;
} SZrCliReplLogicalFactWalk;

typedef struct SZrCliReplExpressionFactWalk {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    const SZrSemanticExpressionFact *lastFact;
} SZrCliReplExpressionFactWalk;

typedef struct SZrCliReplOwnershipFactWalk {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    const SZrSemanticOwnershipFact *lastFact;
} SZrCliReplOwnershipFactWalk;

typedef struct SZrCliReplReferenceFactWalk {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    const SZrSemanticReferenceFact *lastFact;
} SZrCliReplReferenceFactWalk;

typedef struct SZrCliReplReachabilityFactWalk {
    SZrState *state;
    SZrSemanticContext *semanticContext;
    const SZrSemanticReachabilityFact *lastFact;
} SZrCliReplReachabilityFactWalk;

static TZrBool repl_write_expression_fact_for_walk_node(SZrAstNode *node, void *userData);

static void repl_reference_fact_range_for_member(SZrAstNode *memberNode, SZrFileRange *outRange) {
    SZrAstNode *property;

    if (memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        outRange == ZR_NULL) {
        return;
    }

    property = memberNode->data.memberExpression.property;
    if (!memberNode->data.memberExpression.computed || property == ZR_NULL) {
        *outRange = property != ZR_NULL ? property->location : memberNode->location;
        return;
    }

    *outRange = property->location;
    if (outRange->start.offset > memberNode->location.start.offset) {
        outRange->start.offset -= 1;
        if (outRange->start.column > 0) {
            outRange->start.column -= 1;
        }
        outRange->end = outRange->start;
    } else {
        *outRange = memberNode->location;
    }
}

static TZrBool repl_write_numeric_fact_for_walk_node(SZrAstNode *node, void *userData) {
    SZrCliReplNumericFactWalk *walk = (SZrCliReplNumericFactWalk *)userData;
    const SZrSemanticNumericFact *fact;

    if (walk == ZR_NULL ||
        walk->state == ZR_NULL ||
        walk->semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return ZR_TRUE;
    }

    fact = ZrParser_SemanticFacts_FindNumericByNode(walk->semanticContext, node);
    if (fact == ZR_NULL || fact == walk->lastFact) {
        return ZR_TRUE;
    }

    ZrCli_ReplSemanticFacts_WriteNumeric(walk->state, fact);
    walk->lastFact = fact;

    return ZR_FALSE;
}

void ZrCli_ReplSemanticFacts_WriteNumericForExpression(SZrState *state,
                                                       SZrSemanticContext *semanticContext,
                                                       SZrAstNode *node) {
    SZrCliReplNumericFactWalk walk;

    walk.state = state;
    walk.semanticContext = semanticContext;
    walk.lastFact = ZR_NULL;

    ZrCli_ReplSemanticExpressionWalk(node, repl_write_numeric_fact_for_walk_node, &walk);
}

static TZrBool repl_write_logical_fact_for_walk_node(SZrAstNode *node, void *userData) {
    SZrCliReplLogicalFactWalk *walk = (SZrCliReplLogicalFactWalk *)userData;
    const SZrSemanticLogicalFact *fact;

    if (walk == ZR_NULL ||
        walk->state == ZR_NULL ||
        walk->semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return ZR_TRUE;
    }

    fact = ZrParser_SemanticFacts_FindLogicalByNode(walk->semanticContext, node);
    if (fact == ZR_NULL || fact == walk->lastFact) {
        return ZR_TRUE;
    }

    ZrCli_ReplSemanticFacts_WriteLogical(walk->state, fact);
    walk->lastFact = fact;

    return ZR_FALSE;
}

void ZrCli_ReplSemanticFacts_WriteLogicalForExpression(SZrState *state,
                                                       SZrSemanticContext *semanticContext,
                                                       SZrAstNode *node) {
    SZrCliReplLogicalFactWalk walk;

    walk.state = state;
    walk.semanticContext = semanticContext;
    walk.lastFact = ZR_NULL;

    ZrCli_ReplSemanticExpressionWalk(node, repl_write_logical_fact_for_walk_node, &walk);
}

static TZrBool repl_expression_node_is_fact_bearing_receiver(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION:
        case ZR_AST_LOGICAL_EXPRESSION:
        case ZR_AST_UNARY_EXPRESSION:
        case ZR_AST_TYPE_CAST_EXPRESSION:
        case ZR_AST_ASSIGNMENT_EXPRESSION:
        case ZR_AST_ARRAY_LITERAL:
        case ZR_AST_OBJECT_LITERAL:
        case ZR_AST_FUNCTION_CALL:
        case ZR_AST_PRIMARY_EXPRESSION:
        case ZR_AST_CONSTRUCT_EXPRESSION:
        case ZR_AST_CONDITIONAL_EXPRESSION:
        case ZR_AST_LAMBDA_EXPRESSION:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool repl_expression_fact_should_descend(SZrAstNode *node,
                                                   const SZrSemanticExpressionFact *fact,
                                                   SZrCliReplExpressionFactWalk *walk) {
    SZrAstNode *receiver;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }
    if (node != ZR_NULL &&
        node->type == ZR_AST_TYPE_QUERY_EXPRESSION) {
        return ZR_TRUE;
    }

    switch (fact->kind) {
        case ZR_SEMANTIC_EXPRESSION_FACT_ARRAY:
        case ZR_SEMANTIC_EXPRESSION_FACT_OBJECT:
        case ZR_SEMANTIC_EXPRESSION_FACT_CALL:
        case ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA:
            return ZR_TRUE;
        case ZR_SEMANTIC_EXPRESSION_FACT_MEMBER:
            if (node != ZR_NULL &&
                node->type == ZR_AST_PRIMARY_EXPRESSION) {
                receiver = node->data.primaryExpression.property;
                if (repl_expression_node_is_fact_bearing_receiver(receiver)) {
                    ZrCli_ReplSemanticExpressionWalk(receiver,
                                                     repl_write_expression_fact_for_walk_node,
                                                     walk);
                }
            }
            return fact->hasCallInfo;
        default:
            return ZR_FALSE;
    }
}

static TZrBool repl_write_expression_fact_for_walk_node(SZrAstNode *node, void *userData) {
    SZrCliReplExpressionFactWalk *walk = (SZrCliReplExpressionFactWalk *)userData;
    const SZrSemanticExpressionFact *fact;

    if (walk == ZR_NULL ||
        walk->state == ZR_NULL ||
        walk->semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return ZR_TRUE;
    }

    fact = ZrParser_SemanticFacts_FindExpressionByNode(walk->semanticContext, node);
    if (fact == ZR_NULL || fact == walk->lastFact) {
        return ZR_TRUE;
    }

    ZrCli_ReplSemanticFacts_WriteExpression(walk->state, fact);
    walk->lastFact = fact;

    return repl_expression_fact_should_descend(node, fact, walk);
}

void ZrCli_ReplSemanticFacts_WriteExpressionForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node) {
    SZrCliReplExpressionFactWalk walk;

    walk.state = state;
    walk.semanticContext = semanticContext;
    walk.lastFact = ZR_NULL;

    ZrCli_ReplSemanticExpressionWalk(node, repl_write_expression_fact_for_walk_node, &walk);
}

static TZrBool repl_write_ownership_fact_for_walk_node(SZrAstNode *node, void *userData) {
    SZrCliReplOwnershipFactWalk *walk = (SZrCliReplOwnershipFactWalk *)userData;
    const SZrSemanticOwnershipFact *fact;

    if (walk == ZR_NULL ||
        walk->state == ZR_NULL ||
        walk->semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return ZR_TRUE;
    }

    fact = ZrParser_SemanticFacts_FindOwnershipByNode(walk->semanticContext, node);
    if (fact == ZR_NULL || fact == walk->lastFact) {
        return ZR_TRUE;
    }

    ZrCli_ReplSemanticFacts_WriteOwnership(walk->state, fact);
    walk->lastFact = fact;

    return ZR_FALSE;
}

void ZrCli_ReplSemanticFacts_WriteOwnershipForExpression(SZrState *state,
                                                         SZrSemanticContext *semanticContext,
                                                         SZrAstNode *node) {
    SZrCliReplOwnershipFactWalk walk;

    walk.state = state;
    walk.semanticContext = semanticContext;
    walk.lastFact = ZR_NULL;

    ZrCli_ReplSemanticExpressionWalk(node, repl_write_ownership_fact_for_walk_node, &walk);
}

static TZrBool repl_write_reference_fact_for_walk_node(SZrAstNode *node, void *userData) {
    SZrCliReplReferenceFactWalk *walk = (SZrCliReplReferenceFactWalk *)userData;
    SZrFileRange range;

    if (walk == ZR_NULL ||
        walk->state == ZR_NULL ||
        walk->semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        const SZrSemanticReferenceFact *fact =
            ZrParser_SemanticFacts_FindReferenceAtPosition(walk->semanticContext, node->location);
        if (fact != ZR_NULL &&
            fact != walk->lastFact &&
            fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION) {
            ZrCli_ReplSemanticFacts_WriteReferenceAtRange(walk->state,
                                                          walk->semanticContext,
                                                          node->location);
            walk->lastFact = fact;
        }
        return ZR_FALSE;
    }

    if (node->type == ZR_AST_MEMBER_EXPRESSION) {
        repl_reference_fact_range_for_member(node, &range);
        const SZrSemanticReferenceFact *fact =
            ZrParser_SemanticFacts_FindReferenceAtPosition(walk->semanticContext, range);
        if (fact != ZR_NULL &&
            fact != walk->lastFact &&
            fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION) {
            ZrCli_ReplSemanticFacts_WriteReferenceAtRange(walk->state,
                                                          walk->semanticContext,
                                                          range);
            walk->lastFact = fact;
        }
        return node->data.memberExpression.computed;
    }

    return ZR_TRUE;
}

void ZrCli_ReplSemanticFacts_WriteReferencesForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node) {
    SZrCliReplReferenceFactWalk walk;

    walk.state = state;
    walk.semanticContext = semanticContext;
    walk.lastFact = ZR_NULL;

    ZrCli_ReplSemanticExpressionWalk(node, repl_write_reference_fact_for_walk_node, &walk);
}

static TZrBool repl_write_reachability_fact_for_walk_node(SZrAstNode *node, void *userData) {
    SZrCliReplReachabilityFactWalk *walk = (SZrCliReplReachabilityFactWalk *)userData;

    if (walk == ZR_NULL ||
        walk->state == ZR_NULL ||
        walk->semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCli_ReplSemanticFacts_WriteReachabilityAtRange(walk->state,
                                                     walk->semanticContext,
                                                     node->location,
                                                     &walk->lastFact);

    return ZR_TRUE;
}

void ZrCli_ReplSemanticFacts_WriteReachabilityForExpression(SZrState *state,
                                                            SZrSemanticContext *semanticContext,
                                                            SZrAstNode *node) {
    SZrCliReplReachabilityFactWalk walk;

    walk.state = state;
    walk.semanticContext = semanticContext;
    walk.lastFact = ZR_NULL;

    ZrCli_ReplSemanticExpressionWalk(node, repl_write_reachability_fact_for_walk_node, &walk);
}
