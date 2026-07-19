#include "compiler_out_definite_assignment_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TZrBool out_report_uninitialized_read(
        SZrOutFlowAnalysis *analysis,
        SZrOutPlaceRef place,
        SZrFileRange location) {
    const SZrOutParameterInfo *parameter =
            &analysis->tracked->parameters[place.parameterIndex];
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
    snprintf(message,
             sizeof(message),
             "out parameter '%s' cannot be read before it is initialized",
             out_string_native(parameter->name));
    ZrParser_Compiler_Error(analysis->compiler, message, location);
    return ZR_FALSE;
}

static EZrCallArgumentMarker out_call_marker_at(
        const SZrFunctionCall *call,
        TZrSize index) {
    const SZrCallArgumentSyntax *syntax;
    if (call == ZR_NULL || call->argumentMarkers == ZR_NULL ||
        index >= call->argumentMarkers->length) {
        return ZR_CALL_ARGUMENT_MARKER_NONE;
    }
    syntax = (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
            call->argumentMarkers, index);
    return syntax != ZR_NULL ? syntax->marker : ZR_CALL_ARGUMENT_MARKER_NONE;
}

static TZrBool out_analyze_expression(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        TZrBool *state,
        TZrBool isRead);

static void out_record_exception(
        SZrOutFlowAnalysis *analysis,
        const TZrBool *state) {
    if (analysis->exceptionState == ZR_NULL) {
        return;
    }
    if (analysis->hasExceptionState) {
        out_state_intersect(
                analysis->tracked, analysis->exceptionState, state);
    } else {
        out_state_copy(
                analysis->tracked, state, analysis->exceptionState);
        analysis->hasExceptionState = ZR_TRUE;
    }
}

static TZrBool out_analyze_call(
        SZrOutFlowAnalysis *analysis,
        SZrFunctionCall *call,
        TZrBool *state) {
    if (call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0u; index < call->args->count; index++) {
        SZrAstNode *argument = call->args->nodes[index];
        EZrCallArgumentMarker marker = out_call_marker_at(call, index);
        if (marker != ZR_CALL_ARGUMENT_MARKER_OUT &&
            !out_analyze_expression(
                           analysis, argument, state, ZR_TRUE)) {
            return ZR_FALSE;
        }
    }
    out_record_exception(analysis, state);
    for (TZrSize index = 0u; index < call->args->count; index++) {
        if (out_call_marker_at(call, index) == ZR_CALL_ARGUMENT_MARKER_OUT) {
            out_mark_place(
                    analysis->tracked,
                    state,
                    out_resolve_place(
                            analysis->tracked, call->args->nodes[index]));
        }
    }
    return ZR_TRUE;
}

static TZrBool out_analyze_primary(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        TZrBool *state,
        TZrBool isRead) {
    SZrOutPlaceRef place = out_resolve_place(analysis->tracked, node);
    SZrPrimaryExpression *primary = &node->data.primaryExpression;

    if (place.parameterIndex != ZR_PARSER_I32_NONE) {
        if (isRead && !out_place_initialized(analysis->tracked, state, place)) {
            return out_report_uninitialized_read(analysis, place, node->location);
        }
        return ZR_TRUE;
    }
    if (!out_analyze_expression(
                analysis, primary->property, state, ZR_FALSE)) {
        return ZR_FALSE;
    }
    if (primary->members == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0u; index < primary->members->count; index++) {
        SZrAstNode *member = primary->members->nodes[index];
        if (member == ZR_NULL) {
            continue;
        }
        if (member->type == ZR_AST_FUNCTION_CALL) {
            if (!out_analyze_call(
                        analysis, &member->data.functionCall, state)) {
                return ZR_FALSE;
            }
        } else if (member->type == ZR_AST_MEMBER_EXPRESSION &&
                   member->data.memberExpression.computed &&
                   !out_analyze_expression(
                           analysis,
                           member->data.memberExpression.property,
                           state,
                           ZR_TRUE)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool out_analyze_expression(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        TZrBool *state,
        TZrBool isRead) {
    SZrOutPlaceRef place;
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    place = out_resolve_place(analysis->tracked, node);
    if (node->type == ZR_AST_IDENTIFIER_LITERAL &&
        place.parameterIndex != ZR_PARSER_I32_NONE) {
        if (isRead && !out_place_initialized(analysis->tracked, state, place)) {
            return out_report_uninitialized_read(analysis, place, node->location);
        }
        return ZR_TRUE;
    }
    switch (node->type) {
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (node->data.assignmentExpression.op.op != ZR_NULL &&
                strcmp(node->data.assignmentExpression.op.op, "=") != 0 &&
                !out_analyze_expression(
                        analysis,
                        node->data.assignmentExpression.left,
                        state,
                        ZR_TRUE)) {
                return ZR_FALSE;
            }
            if (!out_analyze_expression(
                        analysis,
                        node->data.assignmentExpression.right,
                        state,
                        ZR_TRUE)) {
                return ZR_FALSE;
            }
            out_mark_place(
                    analysis->tracked,
                    state,
                    out_resolve_place(
                            analysis->tracked,
                            node->data.assignmentExpression.left));
            return ZR_TRUE;
        case ZR_AST_PRIMARY_EXPRESSION:
            return out_analyze_primary(analysis, node, state, isRead);
        case ZR_AST_FUNCTION_CALL:
            return out_analyze_call(analysis, &node->data.functionCall, state);
        case ZR_AST_BINARY_EXPRESSION:
            return out_analyze_expression(
                           analysis,
                           node->data.binaryExpression.left,
                           state,
                           ZR_TRUE) &&
                   out_analyze_expression(
                           analysis,
                           node->data.binaryExpression.right,
                           state,
                           ZR_TRUE);
        case ZR_AST_LOGICAL_EXPRESSION:
        {
            TZrBool *rightState = out_state_new(analysis->tracked);
            TZrBool ok;
            if (rightState == ZR_NULL ||
                !out_analyze_expression(
                        analysis,
                        node->data.logicalExpression.left,
                        state,
                        ZR_TRUE)) {
                free(rightState);
                return ZR_FALSE;
            }
            out_state_copy(analysis->tracked, state, rightState);
            ok = out_analyze_expression(
                    analysis,
                    node->data.logicalExpression.right,
                    rightState,
                    ZR_TRUE);
            if (ok) {
                out_state_intersect(analysis->tracked, state, rightState);
            }
            free(rightState);
            return ok;
        }
        case ZR_AST_UNARY_EXPRESSION:
            return out_analyze_expression(
                    analysis,
                    node->data.unaryExpression.argument,
                    state,
                    ZR_TRUE);
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            TZrBool *thenState = out_state_new(analysis->tracked);
            TZrBool *elseState = out_state_new(analysis->tracked);
            TZrBool ok;
            if (thenState == ZR_NULL || elseState == ZR_NULL ||
                !out_analyze_expression(
                        analysis,
                        node->data.conditionalExpression.test,
                        state,
                        ZR_TRUE)) {
                free(thenState);
                free(elseState);
                return ZR_FALSE;
            }
            out_state_copy(analysis->tracked, state, thenState);
            out_state_copy(analysis->tracked, state, elseState);
            ok = out_analyze_expression(
                         analysis,
                         node->data.conditionalExpression.consequent,
                         thenState,
                         ZR_TRUE) &&
                 out_analyze_expression(
                         analysis,
                         node->data.conditionalExpression.alternate,
                         elseState,
                         ZR_TRUE);
            if (ok) {
                out_state_intersect(analysis->tracked, thenState, elseState);
                out_state_copy(analysis->tracked, thenState, state);
            }
            free(thenState);
            free(elseState);
            return ok;
        }
        default:
            return ZR_TRUE;
    }
}

static TZrBool out_is_true_literal(const SZrAstNode *node) {
    return node == ZR_NULL ||
           (node->type == ZR_AST_BOOLEAN_LITERAL &&
            node->data.booleanLiteral.value);
}

TZrBool out_analyze_statement(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        const TZrBool *before,
        TZrBool *after,
        TZrBool *continues);

static TZrBool out_analyze_block(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        const TZrBool *before,
        TZrBool *after,
        TZrBool *continues) {
    TZrBool *current = out_state_new(analysis->tracked);
    TZrBool *next = out_state_new(analysis->tracked);
    TZrBool currentContinues = ZR_TRUE;

    if (current == ZR_NULL || next == ZR_NULL) {
        free(current);
        free(next);
        return ZR_FALSE;
    }
    out_state_copy(analysis->tracked, before, current);
    if (node->data.block.body != ZR_NULL) {
        for (TZrSize index = 0u;
             index < node->data.block.body->count && currentContinues;
             index++) {
            if (!out_analyze_statement(
                        analysis,
                        node->data.block.body->nodes[index],
                        current,
                        next,
                        &currentContinues)) {
                free(current);
                free(next);
                return ZR_FALSE;
            }
            if (currentContinues) {
                out_state_copy(analysis->tracked, next, current);
            }
        }
    }
    out_state_copy(analysis->tracked, current, after);
    *continues = currentContinues;
    free(current);
    free(next);
    return ZR_TRUE;
}

static TZrBool out_analyze_if(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        const TZrBool *before,
        TZrBool *after,
        TZrBool *continues) {
    TZrBool *thenState = out_state_new(analysis->tracked);
    TZrBool *elseState = out_state_new(analysis->tracked);
    TZrBool thenContinues = ZR_TRUE;
    TZrBool elseContinues = ZR_TRUE;
    TZrBool ok = ZR_FALSE;

    if (thenState == ZR_NULL || elseState == ZR_NULL) {
        goto cleanup;
    }
    out_state_copy(analysis->tracked, before, thenState);
    if (!out_analyze_expression(
                analysis,
                node->data.ifExpression.condition,
                thenState,
                ZR_TRUE)) {
        goto cleanup;
    }
    out_state_copy(analysis->tracked, thenState, elseState);
    if (!out_analyze_statement(
                analysis,
                node->data.ifExpression.thenExpr,
                thenState,
                thenState,
                &thenContinues)) {
        goto cleanup;
    }
    if (node->data.ifExpression.elseExpr != ZR_NULL) {
        if (!out_analyze_statement(
                    analysis,
                    node->data.ifExpression.elseExpr,
                    elseState,
                    elseState,
                    &elseContinues)) {
            goto cleanup;
        }
    }
    if (thenContinues && elseContinues) {
        out_state_intersect(analysis->tracked, thenState, elseState);
        out_state_copy(analysis->tracked, thenState, after);
        *continues = ZR_TRUE;
    } else if (thenContinues) {
        out_state_copy(analysis->tracked, thenState, after);
        *continues = ZR_TRUE;
    } else if (elseContinues) {
        out_state_copy(analysis->tracked, elseState, after);
        *continues = ZR_TRUE;
    } else {
        out_state_copy(analysis->tracked, before, after);
        *continues = ZR_FALSE;
    }
    ok = ZR_TRUE;
cleanup:
    free(thenState);
    free(elseState);
    return ok;
}

static TZrBool out_analyze_loop(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *condition,
        SZrAstNode *body,
        SZrAstNode *step,
        const TZrBool *before,
        TZrBool *after,
        TZrBool *continues) {
    TZrBool *bodyState = out_state_new(analysis->tracked);
    TZrBool *zeroIterationState = out_state_new(analysis->tracked);
    TZrBool *loopBreakState = out_state_new(analysis->tracked);
    TZrBool *loopContinueState = out_state_new(analysis->tracked);
    TZrBool *savedBreakState = analysis->breakState;
    TZrBool savedHasBreakState = analysis->hasBreakState;
    TZrBool *savedContinueState = analysis->continueState;
    TZrBool savedHasContinueState = analysis->hasContinueState;
    TZrBool bodyContinues = ZR_TRUE;
    TZrBool ok = ZR_FALSE;

    if (bodyState == ZR_NULL || zeroIterationState == ZR_NULL ||
        loopBreakState == ZR_NULL || loopContinueState == ZR_NULL) {
        goto cleanup;
    }
    out_state_copy(analysis->tracked, before, bodyState);
    if (!out_analyze_expression(
                analysis, condition, bodyState, ZR_TRUE)) {
        goto cleanup;
    }
    out_state_copy(analysis->tracked, bodyState, zeroIterationState);
    analysis->breakState = loopBreakState;
    analysis->hasBreakState = ZR_FALSE;
    analysis->continueState = loopContinueState;
    analysis->hasContinueState = ZR_FALSE;
    if (!out_analyze_statement(
                analysis,
                body,
                bodyState,
                bodyState,
                &bodyContinues)) {
        goto cleanup;
    }
    if (analysis->hasContinueState) {
        if (bodyContinues) {
            out_state_intersect(
                    analysis->tracked, bodyState, loopContinueState);
        } else {
            out_state_copy(
                    analysis->tracked, loopContinueState, bodyState);
            bodyContinues = ZR_TRUE;
        }
    }
    if (bodyContinues &&
        !out_analyze_expression(analysis, step, bodyState, ZR_TRUE)) {
        goto cleanup;
    }
    if (out_is_true_literal(condition)) {
        if (analysis->hasBreakState) {
            out_state_copy(analysis->tracked, loopBreakState, after);
            *continues = ZR_TRUE;
        } else {
            out_state_copy(analysis->tracked, before, after);
            *continues = ZR_FALSE;
        }
    } else {
        out_state_copy(analysis->tracked, bodyState, after);
        out_state_intersect(analysis->tracked, after, zeroIterationState);
        *continues = ZR_TRUE;
    }
    ok = ZR_TRUE;
cleanup:
    analysis->breakState = savedBreakState;
    analysis->hasBreakState = savedHasBreakState;
    analysis->continueState = savedContinueState;
    analysis->hasContinueState = savedHasContinueState;
    free(bodyState);
    free(zeroIterationState);
    free(loopBreakState);
    free(loopContinueState);
    return ok;
}

static TZrBool out_analyze_try(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        const TZrBool *before,
        TZrBool *after,
        TZrBool *continues) {
    SZrTryCatchFinallyStatement *statement =
            &node->data.tryCatchFinallyStatement;
    TZrBool *joined = out_state_new(analysis->tracked);
    TZrBool *path = out_state_new(analysis->tracked);
    TZrBool *tryExceptions = out_state_new(analysis->tracked);
    TZrBool *escapingExceptions = out_state_new(analysis->tracked);
    TZrBool *finalExceptions = out_state_new(analysis->tracked);
    TZrBool *savedExceptionState = analysis->exceptionState;
    TZrBool savedHasExceptionState = analysis->hasExceptionState;
    TZrBool hasCatchClauses =
            statement->catchClauses != ZR_NULL &&
            statement->catchClauses->count > 0u;
    TZrBool tryHasExceptions = ZR_FALSE;
    TZrBool escapingHasExceptions = ZR_FALSE;
    TZrBool anyContinues = ZR_FALSE;
    TZrBool pathContinues = ZR_TRUE;
    TZrBool ok = ZR_FALSE;

    if (joined == ZR_NULL || path == ZR_NULL || tryExceptions == ZR_NULL ||
        escapingExceptions == ZR_NULL || finalExceptions == ZR_NULL) {
        goto cleanup;
    }
    analysis->exceptionState = tryExceptions;
    analysis->hasExceptionState = ZR_FALSE;
    out_state_copy(analysis->tracked, before, path);
    if (!out_analyze_statement(
                analysis,
                statement->block,
                path,
                path,
                &pathContinues)) {
        goto cleanup;
    }
    tryHasExceptions = analysis->hasExceptionState;
    analysis->exceptionState = escapingExceptions;
    analysis->hasExceptionState = ZR_FALSE;
    if (pathContinues) {
        out_state_copy(analysis->tracked, path, joined);
        anyContinues = ZR_TRUE;
    }
    if (hasCatchClauses && tryHasExceptions) {
        for (TZrSize index = 0u;
             index < statement->catchClauses->count;
             index++) {
            SZrAstNode *catchNode = statement->catchClauses->nodes[index];
            if (catchNode == ZR_NULL || catchNode->type != ZR_AST_CATCH_CLAUSE) {
                continue;
            }
            out_state_copy(analysis->tracked, tryExceptions, path);
            pathContinues = ZR_TRUE;
            if (!out_analyze_statement(
                        analysis,
                        catchNode->data.catchClause.block,
                        path,
                        path,
                        &pathContinues)) {
                goto cleanup;
            }
            if (pathContinues) {
                if (anyContinues) {
                    out_state_intersect(analysis->tracked, joined, path);
                } else {
                    out_state_copy(analysis->tracked, path, joined);
                    anyContinues = ZR_TRUE;
                }
            }
        }
    }
    escapingHasExceptions = analysis->hasExceptionState;
    if (!hasCatchClauses && tryHasExceptions) {
        if (escapingHasExceptions) {
            out_state_intersect(
                    analysis->tracked, escapingExceptions, tryExceptions);
        } else {
            out_state_copy(
                    analysis->tracked, tryExceptions, escapingExceptions);
            escapingHasExceptions = ZR_TRUE;
        }
    }

    if (statement->finallyBlock != ZR_NULL) {
        analysis->exceptionState = finalExceptions;
        analysis->hasExceptionState = ZR_FALSE;
        if (anyContinues) {
            pathContinues = ZR_TRUE;
            if (!out_analyze_statement(
                        analysis,
                        statement->finallyBlock,
                        joined,
                        joined,
                        &pathContinues)) {
                goto cleanup;
            }
            anyContinues = pathContinues;
        }
        if (escapingHasExceptions) {
            out_state_copy(
                    analysis->tracked, escapingExceptions, path);
            pathContinues = ZR_TRUE;
            if (!out_analyze_statement(
                        analysis,
                        statement->finallyBlock,
                        path,
                        path,
                        &pathContinues)) {
                goto cleanup;
            }
            if (pathContinues) {
                out_record_exception(analysis, path);
            }
        }
        escapingHasExceptions = analysis->hasExceptionState;
        if (escapingHasExceptions) {
            out_state_copy(
                    analysis->tracked, finalExceptions, escapingExceptions);
        }
    }
    analysis->exceptionState = savedExceptionState;
    analysis->hasExceptionState = savedHasExceptionState;
    if (escapingHasExceptions) {
        out_record_exception(analysis, escapingExceptions);
    }
    if (anyContinues) {
        out_state_copy(analysis->tracked, joined, after);
    } else {
        out_state_copy(analysis->tracked, before, after);
    }
    *continues = anyContinues;
    ok = ZR_TRUE;
cleanup:
    analysis->exceptionState = savedExceptionState;
    if (!ok) {
        analysis->hasExceptionState = savedHasExceptionState;
    }
    free(joined);
    free(path);
    free(tryExceptions);
    free(escapingExceptions);
    free(finalExceptions);
    return ok;
}

TZrBool out_analyze_statement(
        SZrOutFlowAnalysis *analysis,
        SZrAstNode *node,
        const TZrBool *before,
        TZrBool *after,
        TZrBool *continues) {
    out_state_copy(analysis->tracked, before, after);
    *continues = ZR_TRUE;
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (node->type) {
        case ZR_AST_BLOCK:
            return out_analyze_block(
                    analysis, node, before, after, continues);
        case ZR_AST_RETURN_STATEMENT:
            if (!out_analyze_expression(
                        analysis,
                        node->data.returnStatement.expr,
                        after,
                        ZR_TRUE)) {
                return ZR_FALSE;
            }
            *continues = ZR_FALSE;
            return out_report_incomplete(
                    analysis->compiler,
                    analysis->tracked,
                    after,
                    node->location);
        case ZR_AST_THROW_STATEMENT:
            if (!out_analyze_expression(
                        analysis,
                        node->data.throwStatement.expr,
                        after,
                        ZR_TRUE)) {
                return ZR_FALSE;
            }
            out_record_exception(analysis, after);
            *continues = ZR_FALSE;
            return ZR_TRUE;
        case ZR_AST_EXPRESSION_STATEMENT:
            return out_analyze_expression(
                    analysis,
                    node->data.expressionStatement.expr,
                    after,
                    ZR_TRUE);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return out_analyze_expression(
                    analysis, node, after, ZR_TRUE);
        case ZR_AST_VARIABLE_DECLARATION:
            return out_analyze_expression(
                    analysis,
                    node->data.variableDeclaration.value,
                    after,
                    ZR_TRUE);
        case ZR_AST_IF_EXPRESSION:
            return out_analyze_if(
                    analysis, node, before, after, continues);
        case ZR_AST_WHILE_LOOP:
            return out_analyze_loop(
                    analysis,
                    node->data.whileLoop.cond,
                    node->data.whileLoop.block,
                    ZR_NULL,
                    before,
                    after,
                    continues);
        case ZR_AST_FOR_LOOP: {
            TZrBool initContinues = ZR_TRUE;
            if (!out_analyze_statement(
                        analysis,
                        node->data.forLoop.init,
                        before,
                        after,
                        &initContinues)) {
                return ZR_FALSE;
            }
            if (!initContinues) {
                *continues = ZR_FALSE;
                return ZR_TRUE;
            }
            return out_analyze_loop(
                    analysis,
                    node->data.forLoop.cond,
                    node->data.forLoop.block,
                    node->data.forLoop.step,
                    after,
                    after,
                    continues);
        }
        case ZR_AST_FOREACH_LOOP:
            if (!out_analyze_expression(
                        analysis,
                        node->data.foreachLoop.expr,
                        after,
                        ZR_TRUE)) {
                return ZR_FALSE;
            }
            return out_analyze_loop(
                    analysis,
                    ZR_NULL,
                    node->data.foreachLoop.block,
                    ZR_NULL,
                    after,
                    after,
                    continues);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            *continues = ZR_FALSE;
            if (node->data.breakContinueStatement.isBreak &&
                analysis->breakState != ZR_NULL) {
                if (analysis->hasBreakState) {
                    out_state_intersect(
                            analysis->tracked,
                            analysis->breakState,
                            before);
                } else {
                    out_state_copy(
                            analysis->tracked,
                            before,
                            analysis->breakState);
                    analysis->hasBreakState = ZR_TRUE;
                }
            } else if (!node->data.breakContinueStatement.isBreak &&
                       analysis->continueState != ZR_NULL) {
                if (analysis->hasContinueState) {
                    out_state_intersect(
                            analysis->tracked,
                            analysis->continueState,
                            before);
                } else {
                    out_state_copy(
                            analysis->tracked,
                            before,
                            analysis->continueState);
                    analysis->hasContinueState = ZR_TRUE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return out_analyze_try(
                    analysis, node, before, after, continues);
        default:
            return ZR_TRUE;
    }
}
