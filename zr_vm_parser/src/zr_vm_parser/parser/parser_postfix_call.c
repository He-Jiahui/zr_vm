#include "parser_internal.h"

/* 失败路径递归释放实参 AST，同时释放只保存字符串引用的名称和标记数组。 */
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
            /* 拒绝函数已释放 argNames；此处只清理实参节点与标记，避免二次释放。 */
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

        /* 从 prototype 包装节点取走 target，再用构造节点接管它和 args。 */
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
            /* BUG: 构造节点分配失败只释放数组容器，已解析的实参节点不随之释放。 */
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

    /* BUG: append_primary_member 分配失败仅返回 base，不释放刚交给它的 callNode 与实参。 */
    return append_primary_member(ps, base, callNode, chainStartLoc);
}
