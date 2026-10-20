#include "parser_internal.h"

/* Call metadata is parser-owned storage, so its header, backing allocation,
 * and growth must use the same owner budget as the argument AST. */
static SZrParserComptimeBudget *call_metadata_budget(SZrParserState *ps) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL ||
        ps->lexer->ownership == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_Ownership_Budget(ps->lexer->ownership);
}

static TZrBool call_metadata_size(
        TZrSize elementSize, TZrSize capacity, TZrSize *outBytes) {
    if (outBytes == ZR_NULL || elementSize == 0U || capacity == 0U ||
        capacity > ZR_MAX_SIZE / elementSize) {
        return ZR_FALSE;
    }
    *outBytes = elementSize * capacity;
    return ZR_TRUE;
}

static void call_metadata_report_failure(
        SZrParserState *ps, const TZrChar *message) {
    if (ps == ZR_NULL) {
        return;
    }
    ps->hasFatalError = ZR_TRUE;
    report_error(ps, message);
}

static TZrBool call_argument_resource_failed(SZrParserState *ps) {
    SZrParserComptimeBudget *budget = call_metadata_budget(ps);

    if (ps == ZR_NULL || ps->hasFatalError) {
        return ZR_TRUE;
    }
    /* Native token decoding reports allocation failure through the lexer;
     * preserve its shared-budget rejection before syntax recovery runs. */
    if (budget != ZR_NULL &&
        budget->exceededResource != ZR_PARSER_COMPTIME_BUDGET_NONE) {
        call_metadata_report_failure(ps, "Call argument parsing budget exceeded");
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static SZrArray *call_metadata_array_new(
        SZrParserState *ps, TZrSize elementSize, TZrSize initialCapacity) {
    SZrParserComptimeBudget *budget = call_metadata_budget(ps);
    SZrParserComptimeBudgetTransaction transaction;
    SZrArray *array;
    TZrSize capacity = initialCapacity != 0U
                               ? initialCapacity
                               : ZR_PARSER_INITIAL_CAPACITY_SMALL;
    TZrSize bytes;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL ||
        budget == ZR_NULL || !call_metadata_size(elementSize, capacity, &bytes) ||
        bytes > ZR_MAX_SIZE - sizeof(SZrArray)) {
        call_metadata_report_failure(ps, "Failed to initialize call metadata");
        return ZR_NULL;
    }
    ZrParser_ComptimeBudget_BeginTransaction(budget, &transaction);
    if (!ZrParser_ComptimeBudget_TryConsumeTransaction(
                &transaction, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                (TZrUInt64)(sizeof(SZrArray) + bytes))) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        call_metadata_report_failure(ps, "Call metadata allocation budget exceeded");
        return ZR_NULL;
    }
    array = (SZrArray *)ZrCore_Memory_RawMallocWithType(
            ps->state->global, sizeof(SZrArray), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (array == ZR_NULL) {
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        call_metadata_report_failure(ps, "Failed to allocate call metadata");
        return ZR_NULL;
    }
    array->head = (TZrUInt8 *)ZrCore_Memory_RawMallocWithType(
            ps->state->global, bytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (array->head == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                ps->state->global, array, sizeof(SZrArray),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        call_metadata_report_failure(ps, "Failed to allocate call metadata");
        return ZR_NULL;
    }
    if (!ZrParser_ComptimeBudget_CommitTransaction(&transaction)) {
        ZrCore_Memory_RawFreeWithType(
                ps->state->global, array->head, bytes,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrCore_Memory_RawFreeWithType(
                ps->state->global, array, sizeof(SZrArray),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_ComptimeBudget_AbortTransaction(&transaction);
        call_metadata_report_failure(ps, "Call metadata allocation budget exceeded");
        return ZR_NULL;
    }
    array->elementSize = elementSize;
    array->length = 0U;
    array->capacity = capacity;
    array->isValid = ZR_TRUE;
    return array;
}

static TZrBool call_metadata_array_add(
        SZrParserState *ps, SZrArray *array, const void *element) {
    TZrSize newCapacity;
    TZrSize newBytes;
    TZrUInt8 *newHead;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL || array == ZR_NULL ||
        !array->isValid || array->head == ZR_NULL || element == ZR_NULL ||
        array->length > array->capacity) {
        call_metadata_report_failure(ps, "Invalid call metadata array");
        return ZR_FALSE;
    }
    if (array->length < array->capacity) {
        memcpy(
                array->head + array->length * array->elementSize,
                element,
                array->elementSize);
        array->length++;
        return ZR_TRUE;
    }
    if (array->capacity > ZR_MAX_SIZE / ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR) {
        call_metadata_report_failure(ps, "Call metadata array is too large");
        return ZR_FALSE;
    }
    newCapacity = array->capacity * ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR;
    if (newCapacity <= array->capacity ||
        !call_metadata_size(array->elementSize, newCapacity, &newBytes)) {
        call_metadata_report_failure(ps, "Call metadata array is too large");
        return ZR_FALSE;
    }
    newHead = (TZrUInt8 *)ZrParser_Ownership_Allocate(
            ps->lexer->ownership,
            newBytes,
            ZR_MEMORY_NATIVE_TYPE_ARRAY,
            ZR_FALSE);
    if (newHead == ZR_NULL) {
        call_metadata_report_failure(ps, "Call metadata allocation budget exceeded");
        return ZR_FALSE;
    }
    if (array->length != 0U) {
        memcpy(
                newHead,
                array->head,
                array->length * array->elementSize);
    }
    ZrCore_Memory_RawFreeWithType(
            ps->state->global,
            array->head,
            array->capacity * array->elementSize,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    array->head = newHead;
    array->capacity = newCapacity;
    memcpy(
            array->head + array->length * array->elementSize,
            element,
            array->elementSize);
    array->length++;
    return ZR_TRUE;
}

static void call_metadata_array_free(SZrParserState *ps, SZrArray **array) {
    if (ps == ZR_NULL || ps->state == ZR_NULL || array == ZR_NULL ||
        *array == ZR_NULL) {
        return;
    }
    if ((*array)->head != ZR_NULL && (*array)->capacity != 0U) {
        ZrCore_Memory_RawFreeWithType(
                ps->state->global,
                (*array)->head,
                (*array)->capacity * (*array)->elementSize,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Memory_RawFreeWithType(
            ps->state->global,
            *array,
            sizeof(SZrArray),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    *array = ZR_NULL;
}

static void call_argument_list_fail(
        SZrParserState *ps,
        SZrAstNodeArray **args,
        SZrArray **names,
        SZrArray **markers) {
    if (ps == ZR_NULL) {
        return;
    }
    if (args != ZR_NULL && *args != ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, *args);
        *args = ZR_NULL;
    }
    call_metadata_array_free(ps, names);
    call_metadata_array_free(ps, markers);
}

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
    TZrBool isSpread;

    if (call_argument_resource_failed(ps)) {
        return ZR_NULL;
    }
    spreadLocation = get_current_token_location(ps);
    isSpread = ps->lexer->t.token == ZR_TK_PARAMS;
    if (isSpread) {
        ZrParser_Lexer_Next(ps->lexer);
        if (call_argument_resource_failed(ps)) {
            return ZR_NULL;
        }
    }
    expression = parse_expression(ps);
    if (call_argument_resource_failed(ps)) {
        ZrParser_Ast_Free(ps->state, expression);
        return ZR_NULL;
    }
    if (!isSpread) {
        return expression;
    }
    if (expression == ZR_NULL) {
        report_error(ps, "Expected an expression after '...'");
        return ZR_NULL;
    }

    SZrFileRange spreadRange;
    if (!ZrParser_MergeLocations(ps, spreadLocation, expression->location, &spreadRange) ||
        (spread = create_ast_node(ps, ZR_AST_SPREAD_ARGUMENT, spreadRange)) == ZR_NULL) {
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
    SZrAstNodeArray *args;
    SZrArray *names = ZR_NULL;
    SZrArray *markers = ZR_NULL;
    TZrBool hasNamedArgs = ZR_FALSE;

    if (argNames != ZR_NULL) {
        *argNames = ZR_NULL;
    }
    if (argumentMarkers != ZR_NULL) {
        *argumentMarkers = ZR_NULL;
    }
    if (call_argument_resource_failed(ps)) {
        return ZR_NULL;
    }
    args = parser_ast_array_new(ps, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (args == ZR_NULL) {
        return ZR_NULL;
    }
    if (argumentMarkers != ZR_NULL) {
        markers = call_metadata_array_new(
                ps,
                sizeof(SZrCallArgumentSyntax),
                ZR_PARSER_INITIAL_CAPACITY_TINY);
        if (markers == ZR_NULL) {
            call_argument_list_fail(ps, &args, &names, &markers);
            return ZR_NULL;
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
        if (argNames != ZR_NULL) {
            names = call_metadata_array_new(
                    ps, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
            if (names == ZR_NULL) {
                call_argument_list_fail(ps, &args, &names, &markers);
                return ZR_NULL;
            }
            {
                SZrString *storedName = isNamed ? paramName : ZR_NULL;
                if (!call_metadata_array_add(ps, names, &storedName)) {
                    call_argument_list_fail(ps, &args, &names, &markers);
                    return ZR_NULL;
                }
            }
        }

        if (markers != ZR_NULL) {
            SZrCallArgumentSyntax marker = parse_call_argument_marker(ps);
            if (!call_metadata_array_add(ps, markers, &marker)) {
                call_argument_list_fail(ps, &args, &names, &markers);
                return ZR_NULL;
            }
        }
        {
            SZrAstNode *first = parse_call_argument_expression(ps);
            if (first != ZR_NULL) {
                if (!parser_ast_array_add(ps, args, first)) {
                    ZrParser_Ast_Free(ps->state, first);
                    call_argument_list_fail(ps, &args, &names, &markers);
                    return ZR_NULL;
                }
            } else {
                if (ps->hasFatalError) {
                    call_argument_list_fail(ps, &args, &names, &markers);
                    return ZR_NULL;
                }
                call_metadata_array_free(ps, &names);
                call_metadata_array_free(ps, &markers);
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
            if (names != ZR_NULL) {
                SZrString *storedName = isNamed ? paramName : ZR_NULL;
                if (!call_metadata_array_add(ps, names, &storedName)) {
                    call_argument_list_fail(ps, &args, &names, &markers);
                    return ZR_NULL;
                }
            }
            if (markers != ZR_NULL) {
                SZrCallArgumentSyntax marker = parse_call_argument_marker(ps);
                if (!call_metadata_array_add(ps, markers, &marker)) {
                    call_argument_list_fail(ps, &args, &names, &markers);
                    return ZR_NULL;
                }
            }
            {
                SZrAstNode *arg = parse_call_argument_expression(ps);
                if (arg != ZR_NULL) {
                    if (!parser_ast_array_add(ps, args, arg)) {
                        ZrParser_Ast_Free(ps->state, arg);
                        call_argument_list_fail(ps, &args, &names, &markers);
                        return ZR_NULL;
                    }
                } else {
                    if (ps->hasFatalError) {
                        call_argument_list_fail(ps, &args, &names, &markers);
                        return ZR_NULL;
                    }
                    break;
                }
            }
        }
    }

    if (call_argument_resource_failed(ps)) {
        call_argument_list_fail(ps, &args, &names, &markers);
        return ZR_NULL;
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
