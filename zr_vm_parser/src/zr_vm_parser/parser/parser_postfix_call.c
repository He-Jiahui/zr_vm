#include "parser_internal.h"

static void free_call_metadata(SZrParserState *ps,
                               SZrAstNodeArray *args,
                               SZrArray *argNames,
                               SZrArray *argumentMarkers) {
    if (args != ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, args);
    }
    if (argNames != ZR_NULL) {
        ZrCore_Array_Free(ps->state, argNames);
        ZrCore_Memory_RawFreeWithType(
                ps->state->global,
                argNames,
                sizeof(SZrArray),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (argumentMarkers != ZR_NULL) {
        ZrCore_Array_Free(ps->state, argumentMarkers);
        ZrCore_Memory_RawFreeWithType(
                ps->state->global,
                argumentMarkers,
                sizeof(SZrArray),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
}

SZrAstNode *parse_postfix_call_segment(SZrParserState *ps,
                                       SZrAstNode *base,
                                       SZrFileRange chainStartLoc,
                                       SZrFileRange segmentStartLoc,
                                       EZrPostfixAccessMode accessMode) {
    SZrFileRange callOpenLocation;
    SZrFileRange callCloseLocation;
    SZrArray *argNames = ZR_NULL;
    SZrArray *argumentMarkers = ZR_NULL;
    SZrAstNodeArray *args;
    SZrAstNode *callNode;

    if (ps == ZR_NULL || base == ZR_NULL || ps->lexer->t.token != ZR_TK_LPAREN) {
        return ZR_NULL;
    }

    callOpenLocation = get_current_token_location(ps);
    consume_token(ps, ZR_TK_LPAREN);
    args = parse_argument_list(ps, &argNames, &argumentMarkers);
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_call_close(ps, callOpenLocation);
        free_call_metadata(ps, args, argNames, argumentMarkers);
        return ZR_NULL;
    }
    callCloseLocation = get_current_token_location(ps);
    consume_token(ps, ZR_TK_RPAREN);

    if (accessMode == ZR_POSTFIX_ACCESS_DIRECT &&
        base->type == ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION) {
        SZrAstNode *target = base->data.prototypeReferenceExpression.target;
        SZrFileRange fullLoc;
        SZrAstNode *constructNode;

        if (!reject_named_construct_arguments(ps, argNames, chainStartLoc)) {
            free_call_metadata(ps, args, ZR_NULL, argumentMarkers);
            return base;
        }
        argNames = ZR_NULL;
        if (argumentMarkers != ZR_NULL) {
            ZrCore_Array_Free(ps->state, argumentMarkers);
            ZrCore_Memory_RawFreeWithType(
                    ps->state->global,
                    argumentMarkers,
                    sizeof(SZrArray),
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
            argumentMarkers = ZR_NULL;
        }

        base->data.prototypeReferenceExpression.target = ZR_NULL;
        ZrCore_Memory_RawFreeWithType(
                ps->state->global,
                base,
                sizeof(SZrAstNode),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        fullLoc = ZrParser_FileRange_Merge(chainStartLoc, get_current_location(ps));
        constructNode = create_construct_expression_node(
                ps,
                target,
                args,
                ZR_OWNERSHIP_QUALIFIER_NONE,
                ZR_FALSE,
                ZR_FALSE,
                ZR_OWNERSHIP_BUILTIN_KIND_NONE,
                fullLoc);
        if (constructNode == ZR_NULL) {
            if (args != ZR_NULL) {
                ZrParser_AstNodeArray_Free(ps->state, args);
            }
            ZrParser_Ast_Free(ps->state, target);
        }
        return constructNode;
    }

    callNode = create_ast_node(
            ps,
            ZR_AST_FUNCTION_CALL,
            ZrParser_FileRange_Merge(segmentStartLoc, callCloseLocation));
    if (callNode == ZR_NULL) {
        free_call_metadata(ps, args, argNames, argumentMarkers);
        return base;
    }
    callNode->data.functionCall.args = args;
    callNode->data.functionCall.argNames = argNames;
    callNode->data.functionCall.genericArguments = ZR_NULL;
    callNode->data.functionCall.argumentMarkers = argumentMarkers;
    callNode->data.functionCall.hasNamedArgs = ZR_FALSE;
    callNode->data.functionCall.accessMode = accessMode;
    if (argNames != ZR_NULL && argNames->length > 0u) {
        for (TZrSize index = 0u; index < argNames->length; index++) {
            SZrString **name = (SZrString **)ZrCore_Array_Get(argNames, index);

            if (name != ZR_NULL && *name != ZR_NULL) {
                callNode->data.functionCall.hasNamedArgs = ZR_TRUE;
                break;
            }
        }
    }

    return append_primary_member(ps, base, callNode, chainStartLoc);
}
