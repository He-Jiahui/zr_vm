#include "semantic/semantic_analyzer_internal.h"

static void semantic_control_record_logical_fact(SZrSemanticAnalyzer *analyzer,
                                                 SZrAstNode *node,
                                                 EZrSemanticLogicalFactKind kind,
                                                 TZrBool knownValue,
                                                 SZrAstNode *relatedNode) {
    SZrSemanticLogicalFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.kind = kind;
    fact.exactness = ZR_SEMANTIC_FACT_EXACT;
    fact.hasKnownValue = ZR_TRUE;
    fact.knownValue = knownValue;
    fact.relatedNode = relatedNode;
    ZrParser_SemanticFacts_AppendLogical(analyzer->semanticContext, &fact);
}

static void semantic_control_record_unreachable_fact(SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *node,
                                                     EZrSemanticReachabilityCause cause,
                                                     SZrAstNode *causeNode) {
    SZrSemanticReachabilityFact fact;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL) {
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    fact.state = ZR_SEMANTIC_REACHABILITY_UNREACHABLE;
    fact.cause = cause;
    fact.causeNode = causeNode;
    ZrParser_SemanticFacts_AppendReachability(analyzer->semanticContext, &fact);
}

EZrSemanticReachabilityCause ZrLanguageServer_SemanticAnalyzer_ReachabilityCauseForExitNode(
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN;
    }

    switch (node->type) {
        case ZR_AST_RETURN_STATEMENT:
            return ZR_SEMANTIC_REACHABILITY_AFTER_RETURN;
        case ZR_AST_THROW_STATEMENT:
            return ZR_SEMANTIC_REACHABILITY_AFTER_THROW;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return node->data.breakContinueStatement.isBreak
                   ? ZR_SEMANTIC_REACHABILITY_AFTER_BREAK
                   : ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE;
        case ZR_AST_IF_EXPRESSION:
        case ZR_AST_SWITCH_EXPRESSION:
            return ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH;
        case ZR_AST_WHILE_LOOP:
        case ZR_AST_FOR_LOOP:
            return ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP;
        default:
            return ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN;
    }
}

typedef struct SZrSemanticLoopExitFlow {
    TZrBool definitelyTerminates;
    TZrBool allPathsExitEnclosingFlow;
    TZrBool mayLoopLocalExit;
} SZrSemanticLoopExitFlow;

static SZrSemanticLoopExitFlow semantic_loop_flow_no_exit(void) {
    SZrSemanticLoopExitFlow result;

    memset(&result, 0, sizeof(result));
    return result;
}

static SZrSemanticLoopExitFlow semantic_loop_flow_enclosing_exit(void) {
    SZrSemanticLoopExitFlow result;

    memset(&result, 0, sizeof(result));
    result.definitelyTerminates = ZR_TRUE;
    result.allPathsExitEnclosingFlow = ZR_TRUE;
    return result;
}

static SZrSemanticLoopExitFlow semantic_loop_flow_local_exit(void) {
    SZrSemanticLoopExitFlow result;

    memset(&result, 0, sizeof(result));
    result.definitelyTerminates = ZR_TRUE;
    result.mayLoopLocalExit = ZR_TRUE;
    return result;
}

static TZrBool semantic_loop_condition_is_constant_true(SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *conditionNode);
static TZrBool semantic_for_loop_condition_is_constant_true_or_omitted(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *conditionNode);
static SZrSemanticLoopExitFlow semantic_statement_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node);

static SZrSemanticLoopExitFlow semantic_block_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrSemanticLoopExitFlow accumulated;

    accumulated = semantic_loop_flow_no_exit();
    if (node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return accumulated;
    }

    if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
        for (TZrSize index = 0; index < node->data.block.body->count; index++) {
            SZrSemanticLoopExitFlow current =
                semantic_statement_loop_exit_flow(analyzer, node->data.block.body->nodes[index]);

            accumulated.mayLoopLocalExit =
                accumulated.mayLoopLocalExit || current.mayLoopLocalExit;
            if (current.definitelyTerminates) {
                current.mayLoopLocalExit =
                    current.mayLoopLocalExit || accumulated.mayLoopLocalExit;
                if (current.mayLoopLocalExit) {
                    current.allPathsExitEnclosingFlow = ZR_FALSE;
                }
                return current;
            }
        }
    }

    return accumulated;
}

static SZrSemanticLoopExitFlow semantic_if_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrSemanticLoopExitFlow result;
    SZrSemanticLoopExitFlow thenFlow;
    SZrSemanticLoopExitFlow elseFlow;
    TZrBool conditionValue = ZR_FALSE;
    SZrAstNode *conditionEvidence = ZR_NULL;

    if (node == ZR_NULL) {
        return semantic_loop_flow_no_exit();
    }

    if (analyzer != ZR_NULL &&
        ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                analyzer,
                node->data.ifExpression.condition,
                &conditionValue,
                &conditionEvidence)) {
        SZrAstNode *selectedBranch = conditionValue
                                     ? node->data.ifExpression.thenExpr
                                     : node->data.ifExpression.elseExpr;
        ZR_UNUSED_PARAMETER(conditionEvidence);
        return semantic_statement_loop_exit_flow(analyzer, selectedBranch);
    }

    thenFlow = semantic_statement_loop_exit_flow(analyzer, node->data.ifExpression.thenExpr);
    if (node->data.ifExpression.elseExpr == ZR_NULL) {
        result = semantic_loop_flow_no_exit();
        result.mayLoopLocalExit = thenFlow.mayLoopLocalExit;
        return result;
    }

    elseFlow = semantic_statement_loop_exit_flow(analyzer, node->data.ifExpression.elseExpr);
    result = semantic_loop_flow_no_exit();
    result.definitelyTerminates = thenFlow.definitelyTerminates && elseFlow.definitelyTerminates;
    result.mayLoopLocalExit = thenFlow.mayLoopLocalExit || elseFlow.mayLoopLocalExit;
    result.allPathsExitEnclosingFlow = result.definitelyTerminates &&
                                       thenFlow.allPathsExitEnclosingFlow &&
                                       elseFlow.allPathsExitEnclosingFlow &&
                                       !result.mayLoopLocalExit;
    return result;
}

static SZrSemanticLoopExitFlow semantic_switch_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrSwitchExpression *switchExpression;
    SZrSemanticLoopExitFlow result;
    SZrSemanticLoopExitFlow armFlow;

    if (node == ZR_NULL || node->type != ZR_AST_SWITCH_EXPRESSION) {
        return semantic_loop_flow_no_exit();
    }

    switchExpression = &node->data.switchExpression;
    if (switchExpression->defaultCase == ZR_NULL ||
        switchExpression->defaultCase->type != ZR_AST_SWITCH_DEFAULT) {
        return semantic_loop_flow_no_exit();
    }

    result = semantic_statement_loop_exit_flow(analyzer,
                                               switchExpression->defaultCase->data.switchDefault.block);
    if (switchExpression->cases != ZR_NULL) {
        for (TZrSize index = 0; index < switchExpression->cases->count; index++) {
            SZrAstNode *caseNode = switchExpression->cases->nodes[index];
            if (caseNode == ZR_NULL || caseNode->type != ZR_AST_SWITCH_CASE) {
                result.definitelyTerminates = ZR_FALSE;
                result.allPathsExitEnclosingFlow = ZR_FALSE;
                continue;
            }

            armFlow = semantic_statement_loop_exit_flow(analyzer, caseNode->data.switchCase.block);
            result.mayLoopLocalExit = result.mayLoopLocalExit || armFlow.mayLoopLocalExit;
            result.definitelyTerminates = result.definitelyTerminates && armFlow.definitelyTerminates;
            result.allPathsExitEnclosingFlow = result.allPathsExitEnclosingFlow &&
                                               armFlow.allPathsExitEnclosingFlow;
        }
    }

    if (result.mayLoopLocalExit) {
        result.allPathsExitEnclosingFlow = ZR_FALSE;
    }
    return result;
}

static SZrSemanticLoopExitFlow semantic_try_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrTryCatchFinallyStatement *tryStatement;
    SZrSemanticLoopExitFlow result;
    SZrSemanticLoopExitFlow catchFlow;

    if (node == ZR_NULL || node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        return semantic_loop_flow_no_exit();
    }

    tryStatement = &node->data.tryCatchFinallyStatement;
    if (tryStatement->finallyBlock != ZR_NULL) {
        SZrSemanticLoopExitFlow finallyFlow =
            semantic_statement_loop_exit_flow(analyzer, tryStatement->finallyBlock);
        if (finallyFlow.definitelyTerminates) {
            return finallyFlow;
        }
    }

    result = semantic_statement_loop_exit_flow(analyzer, tryStatement->block);
    if (!result.definitelyTerminates) {
        return result;
    }

    if (tryStatement->catchClauses != ZR_NULL) {
        for (TZrSize index = 0; index < tryStatement->catchClauses->count; index++) {
            SZrAstNode *catchNode = tryStatement->catchClauses->nodes[index];
            if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE) {
                result.definitelyTerminates = ZR_FALSE;
                result.allPathsExitEnclosingFlow = ZR_FALSE;
                continue;
            }

            catchFlow = semantic_statement_loop_exit_flow(analyzer, catchNode->data.catchClause.block);
            result.mayLoopLocalExit = result.mayLoopLocalExit || catchFlow.mayLoopLocalExit;
            result.definitelyTerminates = result.definitelyTerminates && catchFlow.definitelyTerminates;
            result.allPathsExitEnclosingFlow = result.allPathsExitEnclosingFlow &&
                                               catchFlow.allPathsExitEnclosingFlow;
        }
    }

    if (result.mayLoopLocalExit) {
        result.allPathsExitEnclosingFlow = ZR_FALSE;
    }
    return result;
}

static SZrSemanticLoopExitFlow semantic_while_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_WHILE_LOOP) {
        return semantic_loop_flow_no_exit();
    }

    if (!semantic_loop_condition_is_constant_true(analyzer, node->data.whileLoop.cond)) {
        return semantic_loop_flow_no_exit();
    }

    return semantic_statement_loop_exit_flow(analyzer, node->data.whileLoop.block);
}

static SZrSemanticLoopExitFlow semantic_for_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_FOR_LOOP) {
        return semantic_loop_flow_no_exit();
    }

    if (!semantic_for_loop_condition_is_constant_true_or_omitted(analyzer, node->data.forLoop.cond)) {
        return semantic_loop_flow_no_exit();
    }

    return semantic_statement_loop_exit_flow(analyzer, node->data.forLoop.block);
}

static SZrSemanticLoopExitFlow semantic_statement_loop_exit_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (node == ZR_NULL) {
        return semantic_loop_flow_no_exit();
    }

    switch (node->type) {
        case ZR_AST_RETURN_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
            return semantic_loop_flow_enclosing_exit();

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_loop_flow_local_exit();

        case ZR_AST_BLOCK:
            return semantic_block_loop_exit_flow(analyzer, node);

        case ZR_AST_IF_EXPRESSION:
            return semantic_if_loop_exit_flow(analyzer, node);

        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_switch_loop_exit_flow(analyzer, node);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return semantic_try_loop_exit_flow(analyzer, node);

        case ZR_AST_WHILE_LOOP:
            return semantic_while_loop_exit_flow(analyzer, node);

        case ZR_AST_FOR_LOOP:
            return semantic_for_loop_exit_flow(analyzer, node);

        default:
            return semantic_loop_flow_no_exit();
    }
}

static TZrBool semantic_statement_exits_enclosing_flow(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrSemanticLoopExitFlow flow;

    flow = semantic_statement_loop_exit_flow(analyzer, node);
    return flow.definitelyTerminates && flow.allPathsExitEnclosingFlow && !flow.mayLoopLocalExit;
}

static TZrBool semantic_loop_condition_is_constant_true(SZrSemanticAnalyzer *analyzer, SZrAstNode *conditionNode) {
    TZrBool conditionValue = ZR_FALSE;
    SZrAstNode *conditionEvidence = ZR_NULL;

    return analyzer != ZR_NULL &&
           ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(analyzer,
                                                                                 conditionNode,
                                                                                 &conditionValue,
                                                                                 &conditionEvidence) &&
           conditionValue;
}

static TZrBool semantic_for_loop_condition_is_constant_true_or_omitted(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *conditionNode) {
    if (conditionNode == ZR_NULL) {
        return ZR_TRUE;
    }
    return semantic_loop_condition_is_constant_true(analyzer, conditionNode);
}

TZrBool ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_RETURN_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return ZR_TRUE;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                                analyzer,
                                node->data.block.body->nodes[index])) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_IF_EXPRESSION: {
            TZrBool conditionValue = ZR_FALSE;
            SZrAstNode *conditionEvidence = ZR_NULL;

            if (analyzer != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                        analyzer,
                        node->data.ifExpression.condition,
                        &conditionValue,
                        &conditionEvidence)) {
                SZrAstNode *selectedBranch = conditionValue
                                             ? node->data.ifExpression.thenExpr
                                             : node->data.ifExpression.elseExpr;
                ZR_UNUSED_PARAMETER(conditionEvidence);
                return ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(analyzer, selectedBranch);
            }

            return node->data.ifExpression.thenExpr != ZR_NULL &&
                   node->data.ifExpression.elseExpr != ZR_NULL &&
                   ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                           analyzer,
                           node->data.ifExpression.thenExpr) &&
                   ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                           analyzer,
                           node->data.ifExpression.elseExpr);
        }

        case ZR_AST_SWITCH_EXPRESSION: {
            SZrSwitchExpression *switchExpression = &node->data.switchExpression;

            if (switchExpression->defaultCase == ZR_NULL ||
                switchExpression->defaultCase->type != ZR_AST_SWITCH_DEFAULT ||
                !ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                        analyzer,
                        switchExpression->defaultCase->data.switchDefault.block)) {
                return ZR_FALSE;
            }

            if (switchExpression->cases != ZR_NULL) {
                for (TZrSize index = 0; index < switchExpression->cases->count; index++) {
                    SZrAstNode *caseNode = switchExpression->cases->nodes[index];
                    if (caseNode == ZR_NULL ||
                        caseNode->type != ZR_AST_SWITCH_CASE ||
                        !ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                                analyzer,
                                caseNode->data.switchCase.block)) {
                        return ZR_FALSE;
                    }
                }
            }

            return ZR_TRUE;
        }

        case ZR_AST_WHILE_LOOP:
            return semantic_loop_condition_is_constant_true(analyzer, node->data.whileLoop.cond) &&
                   semantic_statement_exits_enclosing_flow(analyzer, node->data.whileLoop.block);

        case ZR_AST_FOR_LOOP:
            return semantic_for_loop_condition_is_constant_true_or_omitted(
                           analyzer,
                           node->data.forLoop.cond) &&
                   semantic_statement_exits_enclosing_flow(analyzer, node->data.forLoop.block);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *tryStatement = &node->data.tryCatchFinallyStatement;
            TZrBool allCatchClausesExit = ZR_TRUE;

            if (tryStatement->finallyBlock != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                        analyzer,
                        tryStatement->finallyBlock)) {
                return ZR_TRUE;
            }

            if (!ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                        analyzer,
                        tryStatement->block)) {
                return ZR_FALSE;
            }

            if (tryStatement->catchClauses != ZR_NULL) {
                for (TZrSize index = 0; index < tryStatement->catchClauses->count; index++) {
                    SZrAstNode *catchNode = tryStatement->catchClauses->nodes[index];
                    if (catchNode == ZR_NULL ||
                        catchNode->type != ZR_AST_CATCH_CLAUSE ||
                        !ZrLanguageServer_SemanticAnalyzer_StatementDefinitelyExits(
                                analyzer,
                                catchNode->data.catchClause.block)) {
                        allCatchClausesExit = ZR_FALSE;
                        break;
                    }
                }
            }

            return allCatchClausesExit;
        }

        default:
            return ZR_FALSE;
    }
}

void ZrLanguageServer_SemanticAnalyzer_RecordConstantLoopConditionFacts(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *conditionNode,
        SZrAstNode *bodyNode) {
    TZrBool conditionValue = ZR_FALSE;
    SZrAstNode *conditionEvidence = ZR_NULL;
    SZrAstNode *logicalEvidence;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        conditionNode == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
                analyzer,
                conditionNode,
                &conditionValue,
                &conditionEvidence)) {
        return;
    }

    logicalEvidence = conditionEvidence != ZR_NULL ? conditionEvidence : bodyNode;
    semantic_control_record_logical_fact(analyzer,
                                         conditionNode,
                                         conditionValue
                                             ? ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE
                                             : ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE,
                                         conditionValue,
                                         logicalEvidence);

    if (!conditionValue && bodyNode != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(
                state,
                analyzer,
                ZR_DIAGNOSTIC_WARNING,
                bodyNode->location,
                "Loop body is statically unreachable because the condition is false",
                "unreachable_loop_body");
        semantic_control_record_unreachable_fact(analyzer,
                                                 bodyNode,
                                                 ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE,
                                                 conditionNode);
    }
}
