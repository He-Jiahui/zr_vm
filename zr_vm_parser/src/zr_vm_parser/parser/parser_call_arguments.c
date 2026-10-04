#include "parser_internal.h"

/* 将 ref/out 前缀保存为调用点标记；无前缀时保留 NONE 占位以对齐实参数组。 */
static SZrCallArgumentSyntax parse_call_argument_marker(SZrParserState *ps) {
    SZrCallArgumentSyntax syntax;
    memset(&syntax, 0, sizeof(syntax));
    if (ps->lexer->t.token == ZR_TK_REF) {
        syntax.marker = ZR_CALL_ARGUMENT_MARKER_REF;
        syntax.markerLocation = get_current_token_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
        return syntax;
    }
    if (ps->lexer->t.token == ZR_TK_OUT) {
        syntax.marker = ZR_CALL_ARGUMENT_MARKER_OUT;
        syntax.markerLocation = get_current_token_location(ps);
        ZrParser_Lexer_Next(ps->lexer);
        return syntax;
    }
    syntax.marker = ZR_CALL_ARGUMENT_MARKER_NONE;
    return syntax;
}

/* 展开语法先包成独立 AST 节点，后续编译阶段再判定位置及元素类型是否合法。 */
static SZrAstNode *parse_call_argument_expression(SZrParserState *ps) {
    SZrFileRange spreadLocation;
    SZrAstNode *expression;
    SZrAstNode *spread;

    if (ps->lexer->t.token != ZR_TK_PARAMS) {
        return parse_expression(ps);
    }

    spreadLocation = get_current_token_location(ps);
    ZrParser_Lexer_Next(ps->lexer);
    expression = parse_expression(ps);
    if (expression == ZR_NULL) {
        report_error(ps, "Expected an expression after '...'");
        return ZR_NULL;
    }

    spread = create_ast_node(
            ps,
            ZR_AST_SPREAD_ARGUMENT,
            ZrParser_FileRange_Merge(spreadLocation, expression->location));
    if (spread == ZR_NULL) {
        ZrParser_Ast_Free(ps->state, expression);
        return ZR_NULL;
    }
    spread->data.spreadArgument.expression = expression;
    return spread;
}

/* 返回由调用者接管的表达式数组，并按实参顺序并行返回名称与 ref/out 标记数组。 */
/* 语法错误可能留下部分列表；调用者须结合当前 token/错误状态递归释放已建 AST。 */
SZrAstNodeArray *parse_argument_list(
        SZrParserState *ps,
        SZrArray **argNames,
        SZrArray **argumentMarkers) {
    SZrAstNodeArray *args = ZrParser_AstNodeArray_New(ps->state, ZR_PARSER_INITIAL_CAPACITY_TINY);
    SZrArray *names = ZR_NULL;
    SZrArray *markers = ZR_NULL;
    TZrBool hasNamedArgs = ZR_FALSE;

    if (args == ZR_NULL) {
        if (argNames != ZR_NULL) {
            *argNames = ZR_NULL;
        }
        return ZR_NULL;
    }
    if (argNames != ZR_NULL) {
        *argNames = ZR_NULL;
    }
    /* TODO: 标记数组分配或初始化失败后仍继续解析；需用分配失败注入验证 ref/out 不会被误当普通表达式。 */
    if (argumentMarkers != ZR_NULL) {
        *argumentMarkers = ZR_NULL;
        markers = ZrCore_Memory_RawMallocWithType(
                ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (markers != ZR_NULL) {
            ZrCore_Array_Init(ps->state, markers, sizeof(SZrCallArgumentSyntax),
                              ZR_PARSER_INITIAL_CAPACITY_TINY);
        }
    }

    if (ps->lexer->t.token != ZR_TK_RPAREN) {
        TZrBool isNamed = ZR_FALSE;
        SZrString *paramName = ZR_NULL;

        if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
            paramName = ps->lexer->t.seminfo.stringValue;
            if (peek_token(ps) == ZR_TK_COLON) {
                isNamed = ZR_TRUE;
                ZrParser_Lexer_Next(ps->lexer);
                consume_token(ps, ZR_TK_COLON);
            }
        }

        if (isNamed) {
            hasNamedArgs = ZR_TRUE;
        }
        names = ZrCore_Memory_RawMallocWithType(
                ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (names != ZR_NULL) {
            SZrString *storedName = isNamed ? paramName : ZR_NULL;
            ZrCore_Array_Init(ps->state, names, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
            ZrCore_Array_Push(ps->state, names, &storedName);
        }

        if (markers != ZR_NULL) {
            SZrCallArgumentSyntax marker = parse_call_argument_marker(ps);
            ZrCore_Array_Push(ps->state, markers, &marker);
        }
        {
            SZrAstNode *first = parse_call_argument_expression(ps);
            if (first != ZR_NULL) {
                ZrParser_AstNodeArray_Add(ps->state, args, first);
            } else {
                if (names != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, names);
                    ZrCore_Memory_RawFreeWithType(
                            ps->state->global, names, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                if (markers != ZR_NULL) {
                    ZrCore_Array_Free(ps->state, markers);
                    ZrCore_Memory_RawFreeWithType(
                            ps->state->global, markers, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                return args;
            }
        }

        while (consume_token(ps, ZR_TK_COMMA)) {
            if (ps->lexer->t.token == ZR_TK_RPAREN) {
                break;
            }
            isNamed = ZR_FALSE;
            paramName = ZR_NULL;
            if (ps->lexer->t.token == ZR_TK_IDENTIFIER) {
                paramName = ps->lexer->t.seminfo.stringValue;
                if (peek_token(ps) == ZR_TK_COLON) {
                    isNamed = ZR_TRUE;
                    ZrParser_Lexer_Next(ps->lexer);
                    consume_token(ps, ZR_TK_COLON);
                }
            }
            if (isNamed) {
                hasNamedArgs = ZR_TRUE;
            } else if (hasNamedArgs) {
                report_error(ps, "Positional arguments cannot come after named arguments");
                break;
            }
            if (names == ZR_NULL) {
                names = ZrCore_Memory_RawMallocWithType(
                        ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
                if (names != ZR_NULL) {
                    ZrCore_Array_Init(ps->state, names, sizeof(SZrString *), args->count + 1u);
                    for (TZrSize index = 0u; index < args->count; index++) {
                        SZrString *nullName = ZR_NULL;
                        ZrCore_Array_Push(ps->state, names, &nullName);
                    }
                }
            }
            if (names != ZR_NULL) {
                SZrString *storedName = isNamed ? paramName : ZR_NULL;
                ZrCore_Array_Push(ps->state, names, &storedName);
            }
            if (markers != ZR_NULL) {
                SZrCallArgumentSyntax marker = parse_call_argument_marker(ps);
                ZrCore_Array_Push(ps->state, markers, &marker);
            }
            {
                SZrAstNode *arg = parse_call_argument_expression(ps);
                if (arg != ZR_NULL) {
                    /* BUG: 实参数达到当前容量后，增长分配失败会使 Add 静默保留原 count。
                     * 此处未确认登记成功，arg 会丢失清理入口，已追加的名称/标记与表达式列表也会失配。 */
                    ZrParser_AstNodeArray_Add(ps->state, args, arg);
                } else {
                    break;
                }
            }
        }
    }

    if (argNames != ZR_NULL) {
        *argNames = names;
    }
    if (argumentMarkers != ZR_NULL) {
        *argumentMarkers = markers;
    }
    return args;
}

/* TODO: 全仓当前只有声明和定义，无调用点；核查是否应接入 ref/out 校验或移除该查询。 */
TZrBool call_has_explicit_argument_marker(const SZrArray *markers) {
    if (markers == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0u; index < markers->length; index++) {
        const SZrCallArgumentSyntax *syntax =
                (const SZrCallArgumentSyntax *)ZrCore_Array_Get((SZrArray *)markers, index);
        if (syntax != ZR_NULL && syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}
