#include "parser_internal.h"

// declarationKind selects the diagnostic for a missing declaration-body close.
// A successful block owns its statement array and all appended statements.
static SZrAstNode *parse_block_contents(SZrParserState *ps, const TZrChar *declarationKind) {
    SZrFileRange startLoc = get_current_token_location(ps);
    SZrFileRange endLoc;
    expect_token(ps, ZR_TK_LBRACE);
    ZrParser_Lexer_Next(ps->lexer);

    SZrAstNodeArray *statements = parser_ast_array_new(
            ps, ZR_PARSER_INITIAL_CAPACITY_SMALL);
    if (statements == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token != ZR_TK_RBRACE && ps->lexer->t.token != ZR_TK_EOS) {
        SZrParserComptimeBudget *ownerBudget =
                ZrParser_Ownership_Budget(ps->lexer->ownership);
        const TZrChar *priorMessage = ps->errorMessage;
        TZrBool isolateError = ps->hasError &&
                !ps->hasFatalError && !ps->hasNestingError &&
                !ps->lexer->hasSourceEvidenceError && ownerBudget != ZR_NULL &&
                ownerBudget->exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE;
        SZrAstNode *stmt;

        /* Isolate earlier recoverable errors from this statement's checks.
         * Fresh errors still reach nested parsers and the original callbacks. */
        if (isolateError) {
            ps->hasError = ZR_FALSE;
            ps->errorMessage = ZR_NULL;
        }
        stmt = parse_statement(ps);
        if (isolateError) {
            if (!ps->hasError && !ps->hasFatalError && !ps->hasNestingError &&
                !ps->lexer->hasSourceEvidenceError &&
                ownerBudget->exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE) {
                ps->errorMessage = priorMessage;
            }
            ps->hasError = ZR_TRUE;
        }
        if (stmt != ZR_NULL) {
            if (!parser_ast_array_add(ps, statements, stmt)) {
                ZrParser_Ast_Free(ps->state, stmt);
                free_ast_node_array_with_elements(ps->state, statements);
                return ZR_NULL;
            }
        } else {
            break;
        }
    }

    if (ps->lexer->t.token != ZR_TK_RBRACE) {
        if (ps->lexer->t.token == ZR_TK_EOS) {
            if (declarationKind != ZR_NULL) {
                report_missing_declaration_body_close(ps, declarationKind, startLoc);
            } else {
                report_missing_block_close(ps, startLoc);
            }
        } else {
            expect_token(ps, ZR_TK_RBRACE);
        }
        free_ast_node_array_with_elements(ps->state, statements);
        return ZR_NULL;
    }

    endLoc = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RBRACE);
    SZrFileRange blockLoc;

    SZrAstNode *node;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &blockLoc) ||
        (node = create_ast_node(ps, ZR_AST_BLOCK, blockLoc)) == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, statements);
        return ZR_NULL;
    }

    node->data.block.body = statements;
    node->data.block.isStatement = ZR_TRUE;
    return node;
}

static SZrAstNode *parse_block_impl(SZrParserState *ps, const TZrChar *declarationKind) {
    SZrAstNode *node;
    if (!parser_enter_recursion(ps)) return ZR_NULL;
    node = parse_block_contents(ps, declarationKind);
    --ps->recursionDepth;
    return node;
}

// Statements, control flow and functions enter at the opening brace.
SZrAstNode *parse_block(SZrParserState *ps) {
    return parse_block_impl(ps, ZR_NULL);
}

// Declarations retain their specific missing-close diagnostic.
SZrAstNode *parse_declaration_body_block(SZrParserState *ps, const TZrChar *declarationKind) {
    return parse_block_impl(ps, declarationKind);
}
