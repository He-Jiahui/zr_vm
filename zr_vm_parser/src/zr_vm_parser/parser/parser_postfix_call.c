#include "parser_internal.h"

/* 失败路径递归释放实参 AST，同时释放只保存字符串引用的名称和标记数组。 */
void parser_call_metadata_free(SZrParserState *ps,
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

/* 普通/可选后缀调用共享此入口；成功时把三个实参数组交给函数调用 AST。 */
/* prototype 特例改写成构造表达式，名称和标记在转交前由本层处理。 */
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
    if (args == ZR_NULL || ps->hasError) {
        parser_call_metadata_free(ps, args, argNames, argumentMarkers);
        ZrParser_Ast_Free(ps->state, base);
        return ZR_NULL;
    }
    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        report_missing_call_close(ps, callOpenLocation);
        parser_call_metadata_free(ps, args, argNames, argumentMarkers);
        ZrParser_Ast_Free(ps->state, base);
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
            /* 拒绝函数已释放 argNames；此处只清理实参节点与标记，避免二次释放。 */
            parser_call_metadata_free(ps, args, ZR_NULL, argumentMarkers);
            ZrParser_Ast_Free(ps->state, base);
            return ZR_NULL;
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

        if (!ZrParser_MergeLocations(ps, chainStartLoc, get_current_location(ps), &fullLoc)) {
            parser_call_metadata_free(ps, args, ZR_NULL, ZR_NULL);
            ZrParser_Ast_Free(ps->state, base);
            return ZR_NULL;
        }
        /* 从 prototype 包装节点取走 target，再用构造节点接管它和 args。 */
        base->data.prototypeReferenceExpression.target = ZR_NULL;
        ZrParser_Ast_Free(ps->state, base);
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
            free_ast_node_array_with_elements(ps->state, args);
            ZrParser_Ast_Free(ps->state, target);
        }
        return constructNode;
    }

    SZrFileRange callLocation;
    if (!ZrParser_MergeLocations(ps, segmentStartLoc, callCloseLocation, &callLocation) ||
        (callNode = create_ast_node(ps, ZR_AST_FUNCTION_CALL, callLocation)) == ZR_NULL) {
        parser_call_metadata_free(ps, args, argNames, argumentMarkers);
        ZrParser_Ast_Free(ps->state, base);
        return ZR_NULL;
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

    /* append_primary_member owns both inputs on failure and returns NULL. */
    return append_primary_member(ps, base, callNode, chainStartLoc);
}
