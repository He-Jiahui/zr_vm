#include "parser_internal.h"

/*
 * AST arrays are parser-owned resources rather than ordinary scratch lists.
 * Keep their accounting beside the parser helpers so every checked append
 * uses the same owner budget and failure policy.
 */
static SZrParserComptimeBudget *parser_array_budget(SZrParserState *ps) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL ||
        ps->lexer->ownership == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_Ownership_Budget(ps->lexer->ownership);
}

static void parser_report_array_resource_failure(
        SZrParserState *ps,
        SZrParserComptimeBudget *budget,
        const TZrChar *allocationMessage) {
    if (ps == ZR_NULL) {
        return;
    }
    ps->hasFatalError = ZR_TRUE;
    if (budget != ZR_NULL &&
        budget->exceededResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
        report_error(ps, "Parser AST array allocation budget exceeded");
    } else {
        report_error(ps, allocationMessage);
    }
}

SZrAstNodeArray *parser_ast_array_new(
        SZrParserState *ps,
        TZrSize initialCapacity) {
    SZrParserComptimeBudget *budget = parser_array_budget(ps);
    SZrAstNodeArray *array;

    if (ps == ZR_NULL || ps->state == ZR_NULL || budget == ZR_NULL) {
        parser_report_array_resource_failure(
                ps, budget, "Failed to initialize AST array budget");
        return ZR_NULL;
    }

    array = ZrParser_AstNodeArray_NewBudget(
            ps->state, initialCapacity, budget);
    if (array == ZR_NULL) {
        parser_report_array_resource_failure(
                ps, budget, "Failed to allocate AST node array");
    }
    return array;
}

TZrBool parser_ast_array_add(
        SZrParserState *ps,
        SZrAstNodeArray *array,
        SZrAstNode *node) {
    SZrParserComptimeBudget *budget = parser_array_budget(ps);

    if (ps == ZR_NULL || ps->state == ZR_NULL || array == ZR_NULL ||
        node == ZR_NULL || budget == ZR_NULL) {
        parser_report_array_resource_failure(
                ps, budget, "Failed to initialize AST array budget");
        return ZR_FALSE;
    }

    if (!ZrParser_AstNodeArray_TryAddBudget(
                ps->state, array, node, budget)) {
        parser_report_array_resource_failure(
                ps, budget, "Failed to grow AST node array");
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
