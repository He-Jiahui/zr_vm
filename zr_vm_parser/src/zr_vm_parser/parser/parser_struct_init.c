#include "parser_internal.h"

static void free_argument_syntax_array(SZrParserState *ps, SZrArray *array) {
    if (ps == ZR_NULL || array == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(ps->state, array);
    ZrCore_Memory_RawFreeWithType(ps->state->global,
                                  array,
                                  sizeof(SZrArray),
                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
}

static TZrBool argument_names_have_named_entry(const SZrArray *argNames) {
    if (argNames == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < argNames->length; index++) {
        SZrString **name = (SZrString **)ZrCore_Array_Get((SZrArray *)argNames, index);
        if (name != ZR_NULL && *name != ZR_NULL) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void free_struct_init_parts(SZrParserState *ps,
                                   SZrType *typeInfo,
                                   SZrAstNodeArray *args,
                                   SZrArray *argNames,
                                   SZrArray *argumentMarkers) {
    if (ps == ZR_NULL) {
        return;
    }
    free_owned_type(ps->state, typeInfo);
    free_ast_node_array_with_elements(ps->state, args);
    free_argument_syntax_array(ps, argNames);
    free_argument_syntax_array(ps, argumentMarkers);
}

SZrAstNode *parse_struct_init_expression(SZrParserState *ps) {
    SZrFileRange startLocation;
    SZrFileRange closeLocation;
    SZrType *typeInfo;
    SZrAstNodeArray *args;
    SZrArray *argNames = ZR_NULL;
    SZrArray *argumentMarkers = ZR_NULL;
    SZrAstNode *node;

    if (ps == ZR_NULL || !current_identifier_equals(ps, "init")) {
        return ZR_NULL;
    }

    startLocation = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    typeInfo = parse_type(ps);
    if (typeInfo == ZR_NULL) {
        report_error(ps, "Expected type after 'init'");
        return ZR_NULL;
    }
    if (ps->lexer->t.token != ZR_TK_LPAREN) {
        report_error(ps, "Expected '(' after init type");
        free_owned_type(ps->state, typeInfo);
        return ZR_NULL;
    }

    ZrParser_Lexer_Next(ps->lexer);
    args = parse_argument_list(ps, &argNames, &argumentMarkers);
    if (args == ZR_NULL) {
        free_struct_init_parts(ps, typeInfo, ZR_NULL, argNames, argumentMarkers);
        return ZR_NULL;
    }
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_call_close(ps, startLocation);
        free_struct_init_parts(ps, typeInfo, args, argNames, argumentMarkers);
        return ZR_NULL;
    }
    closeLocation = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);

    node = create_ast_node(ps,
                           ZR_AST_STRUCT_INIT_EXPRESSION,
                           ZrParser_FileRange_Merge(startLocation, closeLocation));
    if (node == ZR_NULL) {
        free_struct_init_parts(ps, typeInfo, args, argNames, argumentMarkers);
        return ZR_NULL;
    }
    node->data.structInitExpression.typeInfo = typeInfo;
    node->data.structInitExpression.args = args;
    node->data.structInitExpression.argNames = argNames;
    node->data.structInitExpression.argumentMarkers = argumentMarkers;
    node->data.structInitExpression.hasNamedArgs = argument_names_have_named_entry(argNames);
    return node;
}
