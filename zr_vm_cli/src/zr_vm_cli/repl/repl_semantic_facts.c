#include "repl/repl_semantic_facts.h"

#include "zr_vm_core/log.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_string_conf.h"

void ZrCli_ReplSemanticFacts_WriteNumeric(SZrState *state, const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return;
    }

    if (fact->hasRange) {
        if (fact->sourceType == ZR_VALUE_TYPE_DOUBLE || fact->targetType == ZR_VALUE_TYPE_DOUBLE ||
            fact->sourceType == ZR_VALUE_TYPE_FLOAT || fact->targetType == ZR_VALUE_TYPE_FLOAT) {
            ZrCore_Log_Resultf(state,
                               "Numeric range: %.17g..%.17g\n",
                               fact->minDoubleValue,
                               fact->maxDoubleValue);
        } else {
            ZrCore_Log_Resultf(state,
                               "Numeric range: %lld..%lld\n",
                               (long long)fact->minValue,
                               (long long)fact->maxValue);
        }
    }

    if (fact->hasUnsignedRange) {
        ZrCore_Log_Resultf(state,
                           "Unsigned range: %llu..%llu\n",
                           (unsigned long long)fact->minUnsignedValue,
                           (unsigned long long)fact->maxUnsignedValue);
    }

    if (fact->mayOverflow) {
        ZrCore_Log_Resultf(state, "Numeric warning: may overflow\n");
    }
}

void ZrCli_ReplSemanticFacts_WriteLogical(SZrState *state, const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return;
    }

    if (fact->hasKnownValue) {
        ZrCore_Log_Resultf(state, "Logical value: %s\n", fact->knownValue ? "true" : "false");
    }
    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT) {
        ZrCore_Log_Resultf(state, "Logical flow: short-circuits right operand\n");
    }
}

static const TZrChar *repl_ownership_fact_kind_text(EZrSemanticOwnershipFactKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION:
            return "declaration";
        case ZR_SEMANTIC_OWNERSHIP_FACT_BORROW:
            return "borrow";
        case ZR_SEMANTIC_OWNERSHIP_FACT_MOVE:
            return "move";
        case ZR_SEMANTIC_OWNERSHIP_FACT_COPY:
            return "copy";
        case ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE:
            return "release";
        case ZR_SEMANTIC_OWNERSHIP_FACT_ERROR:
            return "error";
        case ZR_SEMANTIC_OWNERSHIP_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

static const TZrChar *repl_ownership_qualifier_text(EZrOwnershipQualifier qualifier) {
    switch (qualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return "%unique";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "%shared";
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "%weak";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "%borrowed";
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return "%loaned";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return "plain";
    }
}

static const TZrChar *repl_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

void ZrCli_ReplSemanticFacts_WriteOwnership(SZrState *state, const SZrSemanticOwnershipFact *fact) {
    if (fact == ZR_NULL) {
        return;
    }

    if (fact->isViolation) {
        ZrCore_Log_Resultf(state,
                           "Ownership violation: %s\n",
                           fact->diagnosticMessage != ZR_NULL
                               ? repl_string_text(fact->diagnosticMessage)
                               : repl_ownership_qualifier_text(fact->qualifier));
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Ownership: %s %s\n",
                       repl_ownership_fact_kind_text(fact->kind),
                       repl_ownership_qualifier_text(fact->qualifier));
}

void ZrCli_ReplSemanticFacts_WriteExpression(SZrState *state, const SZrSemanticExpressionFact *fact) {
    const TZrChar *callTargetName;
    const TZrChar *memberName;

    if (fact == ZR_NULL) {
        return;
    }

    if (fact->hasCallInfo) {
        callTargetName = repl_string_text(fact->callTargetName);
        ZrCore_Log_Resultf(state,
                           "Call: %s args=%u\n",
                           callTargetName[0] != '\0' ? callTargetName : "unknown",
                           (unsigned)fact->argumentCount);
    }

    if (fact->hasMemberInfo) {
        memberName = repl_string_text(fact->memberName);
        ZrCore_Log_Resultf(state,
                           "Member: %s\n",
                           memberName[0] != '\0' ? memberName : "computed");
    }
}

static const TZrChar *repl_reference_kind_text(EZrSemanticReferenceKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_REFERENCE_DECLARATION:
            return "declaration";
        case ZR_SEMANTIC_REFERENCE_READ:
            return "read";
        case ZR_SEMANTIC_REFERENCE_WRITE:
            return "write";
        case ZR_SEMANTIC_REFERENCE_CALL:
            return "call";
        case ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS:
            return "member";
        case ZR_SEMANTIC_REFERENCE_MEMBER_WRITE:
            return "member write";
        case ZR_SEMANTIC_REFERENCE_UNKNOWN:
        default:
            return "unknown";
    }
}

static void repl_write_reference_fact_for_range(SZrState *state,
                                                SZrSemanticContext *semanticContext,
                                                SZrFileRange range) {
    const SZrSemanticReferenceFact *fact;

    if (state == ZR_NULL ||
        semanticContext == ZR_NULL) {
        return;
    }

    fact = ZrParser_SemanticFacts_FindReferenceAtPosition(semanticContext, range);
    if (fact == ZR_NULL || fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION) {
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Reference: %s %s\n",
                       repl_reference_kind_text(fact->kind),
                       repl_string_text(fact->name));
    if (fact->isResolved &&
        (fact->declarationRange.source != ZR_NULL ||
         fact->declarationRange.start.line != 0 ||
         fact->declarationRange.start.column != 0 ||
         fact->declarationRange.start.offset != 0)) {
        ZrCore_Log_Resultf(state,
                           "Declared at: %d:%d\n",
                           fact->declarationRange.start.line,
                           fact->declarationRange.start.column);
    }
}

static void repl_write_reference_fact_for_node(SZrState *state,
                                               SZrSemanticContext *semanticContext,
                                               SZrAstNode *node) {
    if (node == ZR_NULL) {
        return;
    }

    repl_write_reference_fact_for_range(state, semanticContext, node->location);
}

static void repl_write_reference_fact_for_computed_member(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node) {
    SZrAstNode *property;
    SZrFileRange queryRange;

    if (node == ZR_NULL ||
        node->type != ZR_AST_MEMBER_EXPRESSION ||
        !node->data.memberExpression.computed) {
        return;
    }

    property = node->data.memberExpression.property;
    queryRange = node->location;
    if (property != ZR_NULL &&
        property->location.start.offset > node->location.start.offset) {
        queryRange = property->location;
        queryRange.start.offset -= 1;
        if (queryRange.start.column > 0) {
            queryRange.start.column -= 1;
        }
        queryRange.end = queryRange.start;
    }

    repl_write_reference_fact_for_range(state, semanticContext, queryRange);
}

void ZrCli_ReplSemanticFacts_WriteReferencesForExpression(SZrState *state,
                                                          SZrSemanticContext *semanticContext,
                                                          SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        repl_write_reference_fact_for_node(state, semanticContext, node);
        return;
    }

    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.binaryExpression.left);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.binaryExpression.right);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.logicalExpression.left);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.logicalExpression.right);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.unaryExpression.argument);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.typeCastExpression.expression);
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.assignmentExpression.left);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.assignmentExpression.right);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.conditionalExpression.test);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.conditionalExpression.consequent);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.conditionalExpression.alternate);
            break;
        case ZR_AST_IF_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.ifExpression.condition);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.ifExpression.thenExpr);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.ifExpression.elseExpr);
            break;
        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL) {
                for (index = 0; index < node->data.functionCall.args->count; ++index) {
                    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state,
                                                                         semanticContext,
                                                                         node->data.functionCall.args->nodes[index]);
                }
            }
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state,
                                                                 semanticContext,
                                                                 node->data.primaryExpression.property);
            if (node->data.primaryExpression.members != ZR_NULL) {
                for (index = 0; index < node->data.primaryExpression.members->count; ++index) {
                    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state,
                                                                         semanticContext,
                                                                         node->data.primaryExpression.members->nodes[index]);
                }
            }
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            repl_write_reference_fact_for_computed_member(state, semanticContext, node);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state,
                                                                 semanticContext,
                                                                 node->data.memberExpression.property);
            break;
        case ZR_AST_ARRAY_LITERAL:
            if (node->data.arrayLiteral.elements != ZR_NULL) {
                for (index = 0; index < node->data.arrayLiteral.elements->count; ++index) {
                    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state,
                                                                         semanticContext,
                                                                         node->data.arrayLiteral.elements->nodes[index]);
                }
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            if (node->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0; index < node->data.objectLiteral.properties->count; ++index) {
                    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state,
                                                                         semanticContext,
                                                                         node->data.objectLiteral.properties->nodes[index]);
                }
            }
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.keyValuePair.key);
            ZrCli_ReplSemanticFacts_WriteReferencesForExpression(state, semanticContext, node->data.keyValuePair.value);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (node->data.constructExpression.args != ZR_NULL) {
                for (index = 0; index < node->data.constructExpression.args->count; ++index) {
                    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(
                            state,
                            semanticContext,
                            node->data.constructExpression.args->nodes[index]);
                }
            }
            break;
        default:
            break;
    }
}

static const TZrChar *repl_reachability_cause_text(EZrSemanticReachabilityCause cause) {
    switch (cause) {
        case ZR_SEMANTIC_REACHABILITY_AFTER_RETURN:
            return "after return";
        case ZR_SEMANTIC_REACHABILITY_AFTER_THROW:
            return "after throw";
        case ZR_SEMANTIC_REACHABILITY_AFTER_BREAK:
            return "after break";
        case ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE:
            return "after continue";
        case ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE:
            return "because the condition is false";
        case ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH:
            return "because a constant branch skips evaluation";
        case ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT:
            return "because short-circuit skips evaluation";
        case ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH:
            return "after exhaustive branch";
        case ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP:
            return "after non-fallthrough loop";
        case ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN:
        default:
            return "for an unknown reason";
    }
}

static void repl_write_reachability_fact_for_node(SZrState *state,
                                                  SZrSemanticContext *semanticContext,
                                                  SZrAstNode *node,
                                                  const SZrSemanticReachabilityFact **lastFact) {
    const SZrSemanticReachabilityFact *fact;

    if (state == ZR_NULL ||
        semanticContext == ZR_NULL ||
        node == ZR_NULL) {
        return;
    }

    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(semanticContext, node->location);
    if (fact == ZR_NULL ||
        fact == *lastFact ||
        fact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE) {
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Reachability: unreachable %s\n",
                       repl_reachability_cause_text(fact->cause));
    *lastFact = fact;
}

static void repl_write_reachability_for_expression_recursive(
        SZrState *state,
        SZrSemanticContext *semanticContext,
        SZrAstNode *node,
        const SZrSemanticReachabilityFact **lastFact) {
    TZrSize index;

    if (node == ZR_NULL) {
        return;
    }

    repl_write_reachability_fact_for_node(state, semanticContext, node, lastFact);

    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.binaryExpression.left,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.binaryExpression.right,
                                                             lastFact);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.logicalExpression.left,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.logicalExpression.right,
                                                             lastFact);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.unaryExpression.argument,
                                                             lastFact);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.typeCastExpression.expression,
                                                             lastFact);
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.assignmentExpression.left,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.assignmentExpression.right,
                                                             lastFact);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.conditionalExpression.test,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.conditionalExpression.consequent,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.conditionalExpression.alternate,
                                                             lastFact);
            break;
        case ZR_AST_IF_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.ifExpression.condition,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.ifExpression.thenExpr,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.ifExpression.elseExpr,
                                                             lastFact);
            break;
        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL) {
                for (index = 0; index < node->data.functionCall.args->count; ++index) {
                    repl_write_reachability_for_expression_recursive(
                            state,
                            semanticContext,
                            node->data.functionCall.args->nodes[index],
                            lastFact);
                }
            }
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.primaryExpression.property,
                                                             lastFact);
            if (node->data.primaryExpression.members != ZR_NULL) {
                for (index = 0; index < node->data.primaryExpression.members->count; ++index) {
                    repl_write_reachability_for_expression_recursive(
                            state,
                            semanticContext,
                            node->data.primaryExpression.members->nodes[index],
                            lastFact);
                }
            }
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            if (node->data.memberExpression.computed) {
                repl_write_reachability_for_expression_recursive(state,
                                                                 semanticContext,
                                                                 node->data.memberExpression.property,
                                                                 lastFact);
            }
            break;
        case ZR_AST_ARRAY_LITERAL:
            if (node->data.arrayLiteral.elements != ZR_NULL) {
                for (index = 0; index < node->data.arrayLiteral.elements->count; ++index) {
                    repl_write_reachability_for_expression_recursive(
                            state,
                            semanticContext,
                            node->data.arrayLiteral.elements->nodes[index],
                            lastFact);
                }
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            if (node->data.objectLiteral.properties != ZR_NULL) {
                for (index = 0; index < node->data.objectLiteral.properties->count; ++index) {
                    repl_write_reachability_for_expression_recursive(
                            state,
                            semanticContext,
                            node->data.objectLiteral.properties->nodes[index],
                            lastFact);
                }
            }
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.keyValuePair.key,
                                                             lastFact);
            repl_write_reachability_for_expression_recursive(state,
                                                             semanticContext,
                                                             node->data.keyValuePair.value,
                                                             lastFact);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (node->data.constructExpression.args != ZR_NULL) {
                for (index = 0; index < node->data.constructExpression.args->count; ++index) {
                    repl_write_reachability_for_expression_recursive(
                            state,
                            semanticContext,
                            node->data.constructExpression.args->nodes[index],
                            lastFact);
                }
            }
            break;
        default:
            break;
    }
}

void ZrCli_ReplSemanticFacts_WriteReachabilityForExpression(SZrState *state,
                                                            SZrSemanticContext *semanticContext,
                                                            SZrAstNode *node) {
    const SZrSemanticReachabilityFact *lastFact = ZR_NULL;

    repl_write_reachability_for_expression_recursive(state, semanticContext, node, &lastFact);
}
