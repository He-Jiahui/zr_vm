#include "eval/debug_eval_internal.h"

/* 此文件的递归下降求值器只服务于显式允许的旧语法兼容路径；先尝试正式解析与能力判定。 */

void zr_debug_evaluate_failure_clear(ZrDebugEvaluateFailure *failure) {
    if (failure != ZR_NULL) {
        memset(failure, 0, sizeof(*failure));
    }
}

void zr_debug_evaluate_failure_set(ZrDebugEvaluateFailure *failure,
                                   EZrDebugEvaluateFailureKind kind,
                                   TZrUInt64 stateId,
                                   const TZrChar *code,
                                   const TZrChar *message) {
    if (failure == ZR_NULL) {
        return;
    }
    zr_debug_evaluate_failure_clear(failure);
    failure->kind = kind;
    failure->state_id = stateId;
    zr_debug_copy_text(failure->code, sizeof(failure->code), code);
    zr_debug_copy_text(failure->message, sizeof(failure->message), message);
}

static void zr_debug_eval_skip_ws(ZrDebugEvalParser *parser) {
    while (parser != ZR_NULL && parser->cursor != ZR_NULL &&
           (*parser->cursor == ' ' || *parser->cursor == '\t' || *parser->cursor == '\r' || *parser->cursor == '\n')) {
        parser->cursor++;
    }
}

static TZrBool zr_debug_eval_match_char(ZrDebugEvalParser *parser, TZrChar ch) {
    zr_debug_eval_skip_ws(parser);
    if (parser == ZR_NULL || parser->cursor == ZR_NULL || *parser->cursor != ch) {
        return ZR_FALSE;
    }

    parser->cursor++;
    return ZR_TRUE;
}

static TZrBool zr_debug_eval_match_text(ZrDebugEvalParser *parser, const TZrChar *text) {
    TZrSize length;

    zr_debug_eval_skip_ws(parser);
    if (parser == ZR_NULL || parser->cursor == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (strncmp(parser->cursor, text, length) != 0) {
        return ZR_FALSE;
    }

    parser->cursor += length;
    return ZR_TRUE;
}

static const TZrChar *zr_debug_eval_peek_non_ws(const ZrDebugEvalParser *parser) {
    const TZrChar *cursor;

    if (parser == ZR_NULL || parser->cursor == ZR_NULL) {
        return ZR_NULL;
    }

    cursor = parser->cursor;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor++;
    }
    return cursor;
}

static TZrBool zr_debug_eval_next_is_missing_consequent_boundary(const ZrDebugEvalParser *parser) {
    const TZrChar *cursor = zr_debug_eval_peek_non_ws(parser);

    return (TZrBool)(cursor == ZR_NULL || *cursor == '\0' || *cursor == ':' || *cursor == ')' || *cursor == ']');
}

static TZrBool zr_debug_eval_next_is_missing_alternate_boundary(const ZrDebugEvalParser *parser) {
    const TZrChar *cursor = zr_debug_eval_peek_non_ws(parser);

    return (TZrBool)(cursor == ZR_NULL || *cursor == '\0' || *cursor == ')' || *cursor == ']');
}

static TZrBool zr_debug_eval_is_ident_start(TZrChar ch) {
    return (TZrBool)((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_');
}

static TZrBool zr_debug_eval_is_ident_part(TZrChar ch) {
    return (TZrBool)(zr_debug_eval_is_ident_start(ch) || (ch >= '0' && ch <= '9'));
}

static TZrBool zr_debug_eval_parse_identifier(ZrDebugEvalParser *parser, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize length = 0;

    zr_debug_eval_skip_ws(parser);
    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (parser == ZR_NULL || parser->cursor == ZR_NULL || !zr_debug_eval_is_ident_start(*parser->cursor)) {
        return ZR_FALSE;
    }

    /* 光标始终消费整个标识符，缓冲区容量只限制传给暂停帧解析器的名称长度。 */
    while (zr_debug_eval_is_ident_part(*parser->cursor)) {
        if (buffer != ZR_NULL && bufferSize > 0 && length + 1 < bufferSize) {
            buffer[length] = *parser->cursor;
        }
        length++;
        parser->cursor++;
    }

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[length < bufferSize ? length : bufferSize - 1u] = '\0';
    }
    return ZR_TRUE;
}

static void zr_debug_eval_append_reference_suffix(ZrDebugEvalParser *parser, const TZrChar *suffix) {
    TZrSize currentLength;

    if (parser == ZR_NULL || parser->reference_buffer == ZR_NULL || parser->reference_buffer_size == 0 ||
        suffix == ZR_NULL || suffix[0] == '\0') {
        return;
    }

    currentLength = strlen(parser->reference_buffer);
    if (currentLength == 0 || currentLength >= parser->reference_buffer_size) {
        return;
    }

    snprintf(parser->reference_buffer + currentLength,
             parser->reference_buffer_size - currentLength,
             ", %s",
             suffix);
    parser->reference_buffer[parser->reference_buffer_size - 1u] = '\0';
}

static void zr_debug_eval_append_reference_summary(ZrDebugEvalParser *parser, const TZrChar *summary) {
    TZrSize currentLength;

    if (parser == ZR_NULL || parser->reference_buffer == ZR_NULL || parser->reference_buffer_size == 0 ||
        summary == ZR_NULL || summary[0] == '\0') {
        return;
    }

    currentLength = strlen(parser->reference_buffer);
    if (currentLength == 0) {
        snprintf(parser->reference_buffer, parser->reference_buffer_size, "%s", summary);
    } else if (currentLength < parser->reference_buffer_size) {
        snprintf(parser->reference_buffer + currentLength,
                 parser->reference_buffer_size - currentLength,
                 ", %s",
                 summary);
    }
    parser->reference_buffer[parser->reference_buffer_size - 1u] = '\0';
}

static void zr_debug_eval_append_member_reference_suffix(ZrDebugEvalParser *parser, const TZrChar *memberName) {
    TZrChar suffix[ZR_DEBUG_NAME_CAPACITY + 16u];

    if (memberName == ZR_NULL || memberName[0] == '\0') {
        return;
    }

    snprintf(suffix, sizeof(suffix), "member %s", memberName);
    suffix[sizeof(suffix) - 1u] = '\0';
    zr_debug_eval_append_reference_suffix(parser, suffix);
}

static TZrBool zr_debug_eval_is_integer_type(EZrValueType type) {
    return (TZrBool)(type == ZR_VALUE_TYPE_INT8 || type == ZR_VALUE_TYPE_INT16 || type == ZR_VALUE_TYPE_INT32 ||
                     type == ZR_VALUE_TYPE_INT64 || type == ZR_VALUE_TYPE_UINT8 || type == ZR_VALUE_TYPE_UINT16 ||
                     type == ZR_VALUE_TYPE_UINT32 || type == ZR_VALUE_TYPE_UINT64);
}

static TZrBool zr_debug_eval_truthy(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_FALSE;
        case ZR_VALUE_TYPE_BOOL:
            return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            return value->value.nativeObject.nativeInt64 != 0 ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return value->value.nativeObject.nativeDouble != 0.0 ? ZR_TRUE : ZR_FALSE;
        case ZR_VALUE_TYPE_STRING:
            return value->value.object != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        default:
            return value->value.object != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
}

static TZrBool zr_debug_eval_to_number(const SZrTypeValue *value, TZrFloat64 *outNumber, TZrBool *outIsInteger) {
    if (outNumber != ZR_NULL) {
        *outNumber = 0.0;
    }
    if (outIsInteger != ZR_NULL) {
        *outIsInteger = ZR_FALSE;
    }
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_debug_eval_is_integer_type(value->type)) {
        if (outNumber != ZR_NULL) {
            *outNumber = (TZrFloat64)value->value.nativeObject.nativeInt64;
        }
        if (outIsInteger != ZR_NULL) {
            *outIsInteger = ZR_TRUE;
        }
        return ZR_TRUE;
    }

    if (value->type == ZR_VALUE_TYPE_FLOAT || value->type == ZR_VALUE_TYPE_DOUBLE) {
        if (outNumber != ZR_NULL) {
            *outNumber = value->value.nativeObject.nativeDouble;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void zr_debug_eval_assign_number(SZrState *state, SZrTypeValue *outValue, TZrFloat64 number, TZrBool isInteger) {
    if (outValue == ZR_NULL) {
        return;
    }

    if (isInteger) {
        ZrCore_Value_InitAsInt(state, outValue, (TZrInt64)number);
    } else {
        ZrCore_Value_InitAsFloat(state, outValue, number);
    }
}

static TZrBool zr_debug_eval_equal_values(SZrState *state, const SZrTypeValue *left, const SZrTypeValue *right, TZrBool *outEqual) {
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;
    TZrBool leftIsInteger;
    TZrBool rightIsInteger;

    if (outEqual != ZR_NULL) {
        *outEqual = ZR_FALSE;
    }
    if (left == ZR_NULL || right == ZR_NULL || outEqual == ZR_NULL) {
        return ZR_FALSE;
    }

    if (zr_debug_eval_to_number(left, &leftNumber, &leftIsInteger) &&
        zr_debug_eval_to_number(right, &rightNumber, &rightIsInteger)) {
        *outEqual = leftNumber == rightNumber ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    if (left->type == ZR_VALUE_TYPE_BOOL && right->type == ZR_VALUE_TYPE_BOOL) {
        *outEqual = left->value.nativeObject.nativeBool == right->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    if (left->type == ZR_VALUE_TYPE_NULL || right->type == ZR_VALUE_TYPE_NULL) {
        *outEqual = (left->type == ZR_VALUE_TYPE_NULL && right->type == ZR_VALUE_TYPE_NULL) ? ZR_TRUE : ZR_FALSE;
        return ZR_TRUE;
    }

    if (left->type == ZR_VALUE_TYPE_STRING && right->type == ZR_VALUE_TYPE_STRING &&
        left->value.object != ZR_NULL && right->value.object != ZR_NULL) {
        *outEqual = strcmp(ZrCore_String_GetNativeString(ZR_CAST_STRING(state, left->value.object)),
                           ZrCore_String_GetNativeString(ZR_CAST_STRING(state, right->value.object))) == 0
                            ? ZR_TRUE
                            : ZR_FALSE;
        return ZR_TRUE;
    }

    *outEqual = (left->type == right->type && left->value.object == right->value.object) ? ZR_TRUE : ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_expression(ZrDebugEvalParser *parser, SZrTypeValue *outValue);

static TZrBool zr_debug_eval_parse_string(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    TZrChar buffer[ZR_DEBUG_TEXT_CAPACITY];
    TZrSize length = 0;
    SZrString *stringObject;

    zr_debug_eval_skip_ws(parser);
    if (parser == ZR_NULL || parser->cursor == ZR_NULL || *parser->cursor != '"') {
        return ZR_FALSE;
    }

    parser->cursor++;
    while (*parser->cursor != '\0' && *parser->cursor != '"') {
        TZrChar ch = *parser->cursor++;
        if (ch == '\\') {
            ch = *parser->cursor++;
            switch (ch) {
                case 'n':
                    ch = '\n';
                    break;
                case 'r':
                    ch = '\r';
                    break;
                case 't':
                    ch = '\t';
                    break;
                case '\\':
                case '"':
                    break;
                default:
                    zr_debug_eval_set_unsupported_string_escape_error(parser, ch);
                    return ZR_FALSE;
            }
        }
        if (length + 1 >= sizeof(buffer)) {
            zr_debug_eval_set_error(parser, "string literal is too long for debug evaluate");
            return ZR_FALSE;
        }
        buffer[length++] = ch;
    }

    if (*parser->cursor != '"') {
        zr_debug_eval_set_unterminated_string_error(parser);
        return ZR_FALSE;
    }

    parser->cursor++;
    buffer[length] = '\0';
    stringObject = ZrCore_String_CreateFromNative(parser->agent->state, buffer);
    if (stringObject == ZR_NULL) {
        zr_debug_eval_set_error(parser, "failed to allocate string during debug evaluate");
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(parser->agent->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
    outValue->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_number(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    TZrFloat64 value = 0.0;
    TZrFloat64 fraction = 0.1;
    TZrBool has_dot = ZR_FALSE;

    zr_debug_eval_skip_ws(parser);
    if (parser == ZR_NULL || parser->cursor == ZR_NULL ||
        !((*parser->cursor >= '0' && *parser->cursor <= '9') ||
          (*parser->cursor == '.' && parser->cursor[1] >= '0' && parser->cursor[1] <= '9'))) {
        return ZR_FALSE;
    }

    while ((*parser->cursor >= '0' && *parser->cursor <= '9') || *parser->cursor == '.') {
        if (*parser->cursor == '.') {
            if (has_dot) {
                zr_debug_eval_set_invalid_numeric_literal_error(parser);
                return ZR_FALSE;
            }
            has_dot = ZR_TRUE;
            parser->cursor++;
            continue;
        }

        if (has_dot) {
            value += fraction * (TZrFloat64)(*parser->cursor - '0');
            fraction *= 0.1;
        } else {
            value = value * 10.0 + (TZrFloat64)(*parser->cursor - '0');
        }
        parser->cursor++;
    }

    zr_debug_eval_assign_number(parser->agent->state, outValue, value, has_dot ? ZR_FALSE : ZR_TRUE);
    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_primary(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    TZrChar identifier[ZR_DEBUG_NAME_CAPACITY];

    zr_debug_eval_skip_ws(parser);
    if (zr_debug_eval_match_char(parser, '(')) {
        if (!zr_debug_eval_parse_expression(parser, outValue)) {
            return ZR_FALSE;
        }
        if (!zr_debug_eval_match_char(parser, ')')) {
            zr_debug_eval_set_missing_group_close_error(parser);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (zr_debug_eval_parse_string(parser, outValue) || zr_debug_eval_parse_number(parser, outValue)) {
        return ZR_TRUE;
    }

    if (!zr_debug_eval_parse_identifier(parser, identifier, sizeof(identifier))) {
        if (parser->error_buffer == ZR_NULL || parser->error_buffer[0] == '\0') {
            zr_debug_eval_set_error(parser, "expected expression in debug evaluate");
        }
        return ZR_FALSE;
    }

    zr_debug_eval_skip_ws(parser);
    if (zr_debug_eval_cursor_starts_assignment(parser)) {
        zr_debug_eval_set_assignment_error(parser);
        return ZR_FALSE;
    }

    if (strcmp(identifier, "true") == 0) {
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, ZR_TRUE);
        return ZR_TRUE;
    }
    if (strcmp(identifier, "false") == 0) {
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, ZR_FALSE);
        return ZR_TRUE;
    }
    if (strcmp(identifier, "null") == 0) {
        ZrCore_Value_ResetAsNull(outValue);
        return ZR_TRUE;
    }

    if (parser->cursor != ZR_NULL && *parser->cursor == '(') {
        zr_debug_eval_set_function_call_error(parser);
        return ZR_FALSE;
    }

    /* 短路分支仍在上面完成语法和调用/赋值限制检查，但不读取暂停帧变量。 */
    if (parser->skip_evaluation) {
        ZrCore_Value_ResetAsNull(outValue);
        return ZR_TRUE;
    }

    if (!zr_debug_resolve_identifier_value(parser->agent,
                                           parser->frame_id,
                                           identifier,
                                           outValue,
                                           parser->error_buffer,
                                           parser->error_buffer_size)) {
        return ZR_FALSE;
    }

    if (parser->reference_buffer != ZR_NULL && parser->reference_buffer_size > 0) {
        TZrChar referenceSummary[ZR_DEBUG_TEXT_CAPACITY];
        referenceSummary[0] = '\0';
        if (zr_debug_identifier_reference_summary(parser->agent,
                                                  parser->frame_id,
                                                  identifier,
                                                  referenceSummary,
                                                  sizeof(referenceSummary))) {
            zr_debug_eval_append_reference_summary(parser, referenceSummary);
        }
    }
    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_postfix(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_primary(parser, outValue)) {
        return ZR_FALSE;
    }

    for (;;) {
        if (zr_debug_eval_match_char(parser, '.')) {
            TZrChar memberName[ZR_DEBUG_NAME_CAPACITY];
            SZrTypeValue memberValue;

            if (!zr_debug_eval_parse_identifier(parser, memberName, sizeof(memberName))) {
                zr_debug_eval_set_missing_member_name_error(parser);
                return ZR_FALSE;
            }

            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
                continue;
            }

            ZrCore_Value_ResetAsNull(&memberValue);
            if (!zr_debug_safe_get_member_value(parser->agent,
                                                outValue,
                                                memberName,
                                                &memberValue,
                                                parser->error_buffer,
                                                parser->error_buffer_size)) {
                return ZR_FALSE;
            }
            *outValue = memberValue;
            zr_debug_eval_append_member_reference_suffix(parser, memberName);
            continue;
        }

        if (zr_debug_eval_match_char(parser, '[')) {
            SZrTypeValue keyValue;
            SZrTypeValue indexedValue;

            ZrCore_Value_ResetAsNull(&keyValue);
            ZrCore_Value_ResetAsNull(&indexedValue);
            if (!zr_debug_eval_parse_expression(parser, &keyValue)) {
                return ZR_FALSE;
            }
            if (!zr_debug_eval_match_char(parser, ']')) {
                zr_debug_eval_set_missing_index_close_error(parser);
                return ZR_FALSE;
            }
            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
                continue;
            }
            if (!zr_debug_safe_get_index_value(parser->agent,
                                               outValue,
                                               &keyValue,
                                               &indexedValue,
                                               parser->error_buffer,
                                               parser->error_buffer_size)) {
                return ZR_FALSE;
            }
            *outValue = indexedValue;
            zr_debug_eval_append_reference_suffix(parser, "index access");
            continue;
        }

        zr_debug_eval_skip_ws(parser);
        if (parser->cursor != ZR_NULL && *parser->cursor == '(') {
            zr_debug_eval_set_function_call_error(parser);
            return ZR_FALSE;
        }

        break;
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_unary(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (zr_debug_eval_match_char(parser, '!')) {
        SZrTypeValue operand;
        if (!zr_debug_eval_parse_unary(parser, &operand)) {
            return ZR_FALSE;
        }
        if (parser->skip_evaluation) {
            ZrCore_Value_ResetAsNull(outValue);
            return ZR_TRUE;
        }
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, zr_debug_eval_truthy(&operand) ? ZR_FALSE : ZR_TRUE);
        return ZR_TRUE;
    }

    if (zr_debug_eval_match_char(parser, '-')) {
        SZrTypeValue operand;
        TZrFloat64 number;
        TZrBool isInteger;
        if (!zr_debug_eval_parse_unary(parser, &operand)) {
            return ZR_FALSE;
        }
        if (parser->skip_evaluation) {
            ZrCore_Value_ResetAsNull(outValue);
            return ZR_TRUE;
        }
        if (!zr_debug_eval_to_number(&operand, &number, &isInteger)) {
            TZrChar message[ZR_DEBUG_TEXT_CAPACITY];
            snprintf(message,
                     sizeof(message),
                     "Unary '-' expects a numeric value. Cause: operand is %s. Suggestion: Use a numeric operand "
                     "or convert the value before applying unary '-'.",
                     zr_debug_eval_value_type_name(&operand));
            message[sizeof(message) - 1u] = '\0';
            zr_debug_eval_set_error(parser, message);
            return ZR_FALSE;
        }
        zr_debug_eval_assign_number(parser->agent->state, outValue, -number, isInteger);
        return ZR_TRUE;
    }

    return zr_debug_eval_parse_postfix(parser, outValue);
}

static TZrBool zr_debug_eval_apply_numeric_binary(ZrDebugEvalParser *parser,
                                                  const SZrTypeValue *left,
                                                  const SZrTypeValue *right,
                                                  const TZrChar *op,
                                                  SZrTypeValue *outValue) {
    TZrFloat64 leftNumber;
    TZrFloat64 rightNumber;
    TZrBool leftIsInteger;
    TZrBool rightIsInteger;

    if (!zr_debug_eval_to_number(left, &leftNumber, &leftIsInteger) ||
        !zr_debug_eval_to_number(right, &rightNumber, &rightIsInteger)) {
        zr_debug_eval_set_numeric_operand_error(parser, op, left, right);
        return ZR_FALSE;
    }

    if (strcmp(op, "+") == 0) {
        zr_debug_eval_assign_number(parser->agent->state, outValue, leftNumber + rightNumber, leftIsInteger && rightIsInteger);
    } else if (strcmp(op, "-") == 0) {
        zr_debug_eval_assign_number(parser->agent->state, outValue, leftNumber - rightNumber, leftIsInteger && rightIsInteger);
    } else if (strcmp(op, "*") == 0) {
        zr_debug_eval_assign_number(parser->agent->state, outValue, leftNumber * rightNumber, leftIsInteger && rightIsInteger);
    } else if (strcmp(op, "/") == 0) {
        if (rightNumber == 0.0) {
            zr_debug_eval_set_division_by_zero_error(parser, op);
            return ZR_FALSE;
        }
        zr_debug_eval_assign_number(parser->agent->state, outValue, leftNumber / rightNumber, ZR_FALSE);
    } else if (strcmp(op, "%") == 0) {
        TZrInt64 leftInt = (TZrInt64)leftNumber;
        TZrInt64 rightInt = (TZrInt64)rightNumber;
        if (!leftIsInteger || !rightIsInteger || rightInt == 0) {
            zr_debug_eval_set_modulo_operand_error(parser,
                                                   left,
                                                   right,
                                                   leftIsInteger,
                                                   rightIsInteger,
                                                   rightInt == 0 ? ZR_TRUE : ZR_FALSE);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsInt(parser->agent->state, outValue, leftInt % rightInt);
    } else if (strcmp(op, "<") == 0) {
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, leftNumber < rightNumber ? ZR_TRUE : ZR_FALSE);
    } else if (strcmp(op, "<=") == 0) {
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, leftNumber <= rightNumber ? ZR_TRUE : ZR_FALSE);
    } else if (strcmp(op, ">") == 0) {
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, leftNumber > rightNumber ? ZR_TRUE : ZR_FALSE);
    } else if (strcmp(op, ">=") == 0) {
        ZrCore_Value_InitAsBool(parser->agent->state, outValue, leftNumber >= rightNumber ? ZR_TRUE : ZR_FALSE);
    } else {
        zr_debug_eval_set_error(parser, "unsupported numeric operator");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_multiplicative(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_unary(parser, outValue)) {
        return ZR_FALSE;
    }

    for (;;) {
        if (zr_debug_eval_match_text(parser, "*")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            ZrCore_Value_ResetAsNull(&result);
            if (!zr_debug_eval_parse_right_operand(parser, "*", zr_debug_eval_parse_unary, &rhs) ||
                (!parser->skip_evaluation && !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "*", &result))) {
                return ZR_FALSE;
            }
            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
            } else {
                *outValue = result;
            }
            continue;
        }
        if (zr_debug_eval_match_text(parser, "/")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            ZrCore_Value_ResetAsNull(&result);
            if (!zr_debug_eval_parse_right_operand(parser, "/", zr_debug_eval_parse_unary, &rhs) ||
                (!parser->skip_evaluation && !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "/", &result))) {
                return ZR_FALSE;
            }
            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
            } else {
                *outValue = result;
            }
            continue;
        }
        if (zr_debug_eval_match_text(parser, "%")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            ZrCore_Value_ResetAsNull(&result);
            if (!zr_debug_eval_parse_right_operand(parser, "%", zr_debug_eval_parse_unary, &rhs) ||
                (!parser->skip_evaluation && !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "%", &result))) {
                return ZR_FALSE;
            }
            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
            } else {
                *outValue = result;
            }
            continue;
        }
        break;
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_additive(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_multiplicative(parser, outValue)) {
        return ZR_FALSE;
    }

    for (;;) {
        if (zr_debug_eval_match_text(parser, "+")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            ZrCore_Value_ResetAsNull(&result);
            if (!zr_debug_eval_parse_right_operand(parser, "+", zr_debug_eval_parse_multiplicative, &rhs) ||
                (!parser->skip_evaluation && !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "+", &result))) {
                return ZR_FALSE;
            }
            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
            } else {
                *outValue = result;
            }
            continue;
        }
        if (zr_debug_eval_match_text(parser, "-")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            ZrCore_Value_ResetAsNull(&result);
            if (!zr_debug_eval_parse_right_operand(parser, "-", zr_debug_eval_parse_multiplicative, &rhs) ||
                (!parser->skip_evaluation && !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "-", &result))) {
                return ZR_FALSE;
            }
            if (parser->skip_evaluation) {
                ZrCore_Value_ResetAsNull(outValue);
            } else {
                *outValue = result;
            }
            continue;
        }
        break;
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_relational(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_additive(parser, outValue)) {
        return ZR_FALSE;
    }

    for (;;) {
        const TZrChar *op = ZR_NULL;
        if (zr_debug_eval_match_text(parser, "<=")) {
            op = "<=";
        } else if (zr_debug_eval_match_text(parser, ">=")) {
            op = ">=";
        } else if (zr_debug_eval_match_text(parser, "<")) {
            op = "<";
        } else if (zr_debug_eval_match_text(parser, ">")) {
            op = ">";
        }

        if (op == ZR_NULL) {
            break;
        }

        SZrTypeValue rhs;
        SZrTypeValue result;
        ZrCore_Value_ResetAsNull(&result);
        if (!zr_debug_eval_parse_right_operand(parser, op, zr_debug_eval_parse_additive, &rhs) ||
            (!parser->skip_evaluation && !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, op, &result))) {
            return ZR_FALSE;
        }
        if (parser->skip_evaluation) {
            ZrCore_Value_ResetAsNull(outValue);
        } else {
            *outValue = result;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_equality(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_relational(parser, outValue)) {
        return ZR_FALSE;
    }

    for (;;) {
        TZrBool expect_equal;
        if (zr_debug_eval_match_text(parser, "==")) {
            expect_equal = ZR_TRUE;
        } else if (zr_debug_eval_match_text(parser, "!=")) {
            expect_equal = ZR_FALSE;
        } else {
            break;
        }

        SZrTypeValue rhs;
        TZrBool isEqual;
        if (!zr_debug_eval_parse_right_operand(parser,
                                               expect_equal ? "==" : "!=",
                                               zr_debug_eval_parse_relational,
                                               &rhs)) {
            return ZR_FALSE;
        }
        if (parser->skip_evaluation) {
            ZrCore_Value_ResetAsNull(outValue);
            continue;
        }
        if (!zr_debug_eval_equal_values(parser->agent->state, outValue, &rhs, &isEqual)) {
            zr_debug_eval_set_error(parser, "failed to compare values in debug evaluate");
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsBool(parser->agent->state, outValue, (isEqual == expect_equal) ? ZR_TRUE : ZR_FALSE);
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_logical_and(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_equality(parser, outValue)) {
        return ZR_FALSE;
    }

    while (zr_debug_eval_match_text(parser, "&&")) {
        SZrTypeValue rhs;
        TZrBool leftTruthy;
        TZrBool skipRhs;

        ZrCore_Value_ResetAsNull(&rhs);
        leftTruthy = zr_debug_eval_truthy(outValue);
        skipRhs = (TZrBool)(parser->skip_evaluation || !leftTruthy);
        if (!zr_debug_eval_parse_right_operand_with_skip(parser, "&&", zr_debug_eval_parse_equality, &rhs, skipRhs)) {
            return ZR_FALSE;
        }
        if (parser->skip_evaluation) {
            ZrCore_Value_ResetAsNull(outValue);
        } else if (!leftTruthy) {
            ZrCore_Value_InitAsBool(parser->agent->state, outValue, ZR_FALSE);
        } else {
            ZrCore_Value_InitAsBool(parser->agent->state, outValue, zr_debug_eval_truthy(&rhs) ? ZR_TRUE : ZR_FALSE);
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_logical_or(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_logical_and(parser, outValue)) {
        return ZR_FALSE;
    }

    while (zr_debug_eval_match_text(parser, "||")) {
        SZrTypeValue rhs;
        TZrBool leftTruthy;
        TZrBool skipRhs;

        ZrCore_Value_ResetAsNull(&rhs);
        leftTruthy = zr_debug_eval_truthy(outValue);
        skipRhs = (TZrBool)(parser->skip_evaluation || leftTruthy);
        if (!zr_debug_eval_parse_right_operand_with_skip(parser, "||", zr_debug_eval_parse_logical_and, &rhs, skipRhs)) {
            return ZR_FALSE;
        }
        if (parser->skip_evaluation) {
            ZrCore_Value_ResetAsNull(outValue);
        } else if (leftTruthy) {
            ZrCore_Value_InitAsBool(parser->agent->state, outValue, ZR_TRUE);
        } else {
            ZrCore_Value_InitAsBool(parser->agent->state, outValue, zr_debug_eval_truthy(&rhs) ? ZR_TRUE : ZR_FALSE);
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_expression(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    SZrTypeValue consequent;
    SZrTypeValue alternate;
    TZrBool conditionTruthy;
    TZrBool skipConsequent;
    TZrBool skipAlternate;

    if (!zr_debug_eval_parse_logical_or(parser, outValue)) {
        return ZR_FALSE;
    }

    if (!zr_debug_eval_match_char(parser, '?')) {
        return ZR_TRUE;
    }

    ZrCore_Value_ResetAsNull(&consequent);
    ZrCore_Value_ResetAsNull(&alternate);
    conditionTruthy = zr_debug_eval_truthy(outValue);
    skipConsequent = (TZrBool)(parser->skip_evaluation || !conditionTruthy);
    if (zr_debug_eval_next_is_missing_consequent_boundary(parser)) {
        zr_debug_eval_set_missing_conditional_consequent_error(parser);
        return ZR_FALSE;
    }
    if (!zr_debug_eval_parse_right_operand_with_skip(parser,
                                                     "?",
                                                     zr_debug_eval_parse_expression,
                                                     &consequent,
                                                     skipConsequent)) {
        return ZR_FALSE;
    }
    if (!zr_debug_eval_match_char(parser, ':')) {
        zr_debug_eval_set_missing_conditional_separator_error(parser);
        return ZR_FALSE;
    }

    skipAlternate = (TZrBool)(parser->skip_evaluation || conditionTruthy);
    if (zr_debug_eval_next_is_missing_alternate_boundary(parser)) {
        zr_debug_eval_set_missing_conditional_alternate_error(parser);
        return ZR_FALSE;
    }
    if (!zr_debug_eval_parse_right_operand_with_skip(parser,
                                                     ":",
                                                     zr_debug_eval_parse_expression,
                                                     &alternate,
                                                     skipAlternate)) {
        return ZR_FALSE;
    }

    if (parser->skip_evaluation) {
        ZrCore_Value_ResetAsNull(outValue);
    } else if (conditionTruthy) {
        *outValue = consequent;
    } else {
        *outValue = alternate;
    }
    return ZR_TRUE;
}

TZrBool zr_debug_evaluate_expression_with_capabilities(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        TZrUInt32 allowedEffectFlags,
        TZrBool allowLegacyCompatibility,
        SZrTypeValue *outValue,
        TZrUInt32 *outCanonicalTypeId,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize,
        TZrChar *referenceBuffer,
        TZrSize referenceBufferSize) {
    ZrDebugEvalParser parser;
    TZrBool formalHandled = ZR_FALSE;
    TZrBool formalParserFailure = ZR_FALSE;

    zr_debug_evaluate_failure_clear(outFailure);
    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (outCanonicalTypeId != ZR_NULL) {
        *outCanonicalTypeId = ZR_DEBUG_CANONICAL_TYPE_ID_INVALID;
    }
    if (referenceBuffer != ZR_NULL && referenceBufferSize > 0) {
        referenceBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || expression == ZR_NULL || outValue == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid debug evaluate request");
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_REQUEST,
                                      agent != ZR_NULL ? agent->stopStateId : 0u,
                                      "debug_evaluation_invalid_request",
                                      "invalid debug evaluate request");
        return ZR_FALSE;
    }
    /* 先使用语义事实执行；只有调用方授权，且正式解析失败或未处理时才尝试兼容求值。 */
    if (!zr_debug_formal_evaluate_expression(agent,
                                             frameId,
                                             expression,
                                             allowedEffectFlags,
                                             outValue,
                                             outCanonicalTypeId,
                                             outFailure,
                                             errorBuffer,
                                             errorBufferSize,
                                             &formalHandled,
                                             &formalParserFailure)) {
        if (!allowLegacyCompatibility || !formalParserFailure) {
            return ZR_FALSE;
        }
        zr_debug_evaluate_failure_clear(outFailure);
        if (outFailure != ZR_NULL) {
            outFailure->state_id = agent->stopStateId;
        }
        if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
            errorBuffer[0] = '\0';
        }
    }
    if (formalHandled) {
        return ZR_TRUE;
    }
    if (outCanonicalTypeId != ZR_NULL) {
        *outCanonicalTypeId = ZR_DEBUG_CANONICAL_TYPE_ID_INVALID;
    }
    if (!allowLegacyCompatibility) {
        if (errorBuffer == ZR_NULL || errorBufferSize == 0u || errorBuffer[0] == '\0') {
            zr_debug_copy_text(errorBuffer,
                               errorBufferSize,
                               "formal execution is unavailable for the authorized debug evaluation");
        }
        if (outFailure != ZR_NULL && outFailure->kind == ZR_DEBUG_EVALUATE_FAILURE_NONE) {
            zr_debug_evaluate_failure_set(outFailure,
                                          ZR_DEBUG_EVALUATE_FAILURE_FORMAL_EXECUTION,
                                          agent->stopStateId,
                                          "debug_evaluation_formal_execution_unavailable",
                                          "formal execution is unavailable for the authorized debug evaluation");
        }
        return ZR_FALSE;
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }

    memset(&parser, 0, sizeof(parser));
    parser.agent = agent;
    parser.frame_id = frameId;
    parser.cursor = expression;
    parser.error_buffer = errorBuffer;
    parser.error_buffer_size = errorBufferSize;
    parser.reference_buffer = referenceBuffer;
    parser.reference_buffer_size = referenceBufferSize;

    if (!zr_debug_eval_parse_expression(&parser, outValue)) {
        if (errorBuffer != ZR_NULL && errorBuffer[0] == '\0') {
            zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to parse debug evaluate expression");
        }
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_LEGACY_COMPATIBILITY,
                                      agent->stopStateId,
                                      "debug_evaluation_legacy_parse_failure",
                                      "legacy debug evaluate parsing failed");
        return ZR_FALSE;
    }

    zr_debug_eval_skip_ws(&parser);
    if (parser.cursor != ZR_NULL && *parser.cursor != '\0') {
        if (zr_debug_eval_cursor_starts_assignment(&parser)) {
            zr_debug_eval_set_assignment_error(&parser);
        } else {
            zr_debug_copy_text(errorBuffer, errorBufferSize, "unexpected trailing tokens in debug evaluate");
        }
        zr_debug_evaluate_failure_set(outFailure,
                                      ZR_DEBUG_EVALUATE_FAILURE_LEGACY_COMPATIBILITY,
                                      agent->stopStateId,
                                      "debug_evaluation_legacy_parse_failure",
                                      "legacy debug evaluate parsing failed");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool zr_debug_evaluate_expression(ZrDebugAgent *agent,
                                     TZrUInt32 frameId,
                                     const TZrChar *expression,
                                     SZrTypeValue *outValue,
                                     TZrChar *errorBuffer,
                                     TZrSize errorBufferSize,
                                     TZrChar *referenceBuffer,
                                     TZrSize referenceBufferSize) {
    return zr_debug_evaluate_expression_with_capabilities(
            agent,
            frameId,
            expression,
            ZR_DEBUG_EVALUATION_EFFECT_NONE,
            ZR_TRUE,
            outValue,
            ZR_NULL,
            ZR_NULL,
            errorBuffer,
            errorBufferSize,
            referenceBuffer,
            referenceBufferSize);
}

/**
 * @fn void zr_debug_evaluate_failure_clear(ZrDebugEvaluateFailure *failure)
 *
 * ### Summary
 * 为新一次求值或重新归类失败清空整个失败详情，避免沿用先前的错误种类、状态编号和诊断文字。
 *
 * ### Parameters
 * @param[out] failure 可空的调用方失败详情；非空时覆盖整个结构，不保留旧字段。
 *
 * ### Returns
 * 无返回值；空指针不产生写入。
 *
 * ### Constraints and lifecycle
 * 仅写调用方结构，不分配或持有资源。该结构及其内联数组不因清空而获得线程同步；并发访问由所属请求协调。
 *
 * ### References
 * - zr_debug_evaluate_failure_set，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:11
 * - zr_debug_evaluate_expression_with_capabilities，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:932
 * - zr_debug_evaluate_with_policy，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2490
 */

/**
 * @fn void zr_debug_evaluate_failure_set(ZrDebugEvaluateFailure *failure, EZrDebugEvaluateFailureKind kind, TZrUInt64 stateId, const TZrChar *code, const TZrChar *message)
 *
 * ### Summary
 * 在一个已知失败点记录稳定的失败分类、暂停状态编号、错误码和可展示文字，先清除旧内容再填写本次详情。
 *
 * ### Parameters
 * @param[out] failure 可空的失败详情；非空时先整体清零。
 * @param[in] kind 本次失败分类，按调用方给定枚举原样记录。
 * @param[in] stateId 产生本次诊断的暂停状态编号，不在此验证是否仍有效。
 * @param[in] code 可空的 NUL 结尾错误码；复制到内联有界数组。
 * @param[in] message 可空的 NUL 结尾说明；复制到内联有界数组。
 *
 * ### Returns
 * 无返回值；failure 为空时不读取其它文本参数。
 *
 * ### Constraints and lifecycle
 * 文本只在调用期间借用，不转移其存储；有界复制可截断，详情是诊断快照而非活动状态句柄。函数不分配或建立异常恢复点。
 *
 * ### References
 * - zr_debug_evaluate_failure_clear，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:5
 * - zr_debug_copy_text，zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:34
 * - zr_debug_formal_evaluate_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:361
 */

/**
 * @fn static void zr_debug_eval_skip_ws(ZrDebugEvalParser *parser)
 *
 * ### Summary
 * 统一消费兼容语法识别前的四种 ASCII 空白，使各优先级解析器从下一个候选字符继续。
 *
 * ### Parameters
 * @param[in,out] parser 借用解析状态；非空且 cursor 非空时推进其 cursor。
 *
 * ### Returns
 * 无返回值；无解析器或光标时直接返回。
 *
 * ### Constraints and lifecycle
 * 只跳过空格、制表、CR、LF，不按区域设置识别其它空白。有效 cursor 必须指向仍存活的 NUL 结尾输入；不保留起始位置或提供回滚。
 *
 * ### References
 * - zr_debug_eval_match_char，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:33
 * - zr_debug_eval_parse_identifier，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:94
 * - zr_debug_evaluate_expression_with_capabilities，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:932
 */

/**
 * @fn static TZrBool zr_debug_eval_match_char(ZrDebugEvalParser *parser, TZrChar ch)
 *
 * ### Summary
 * 在忽略前导空白后匹配并消费一个指定语法字符，供括号、后缀和条件运算的分隔符识别复用。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；先消费空白，匹配成功后再消费一个字符。
 * @param[in] ch 要匹配的单个字符；调用点用于非 NUL 分隔符。
 *
 * ### Returns
 * @return 匹配并消费返回 TRUE；无有效光标或字符不符返回 FALSE。
 *
 * ### Constraints and lifecycle
 * FALSE 仍可能已越过前导空白，不是探测后自动回滚。输入在整个递归解析期间保持有效；本函数不验证分隔符对应关系。
 *
 * ### References
 * - zr_debug_eval_skip_ws，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:26
 * - zr_debug_eval_parse_primary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388
 * - zr_debug_eval_parse_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:874
 */

/**
 * @fn static TZrBool zr_debug_eval_match_text(ZrDebugEvalParser *parser, const TZrChar *text)
 *
 * ### Summary
 * 按调用顺序尝试一个运算符文本，使同一优先级可以先识别长运算符再识别其前缀。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；先跳过空白，成功后按文本长度推进光标。
 * @param[in] text 调用期间有效的 NUL 文本；现有调用均传入非空运算符常量。
 *
 * ### Returns
 * @return 前缀字节相同返回 TRUE；参数无效或前缀不同返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 不检查词法边界。FALSE 可能消费空白；空文本会成功而不再前进，故循环调用方必须使用非空运算符。<=、>= 在 <、> 前匹配的顺序由上层保持。
 *
 * ### References
 * - zr_debug_eval_skip_ws，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:26
 * - zr_debug_eval_parse_relational，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:742
 * - zr_debug_eval_parse_equality，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:780
 */

/**
 * @fn static const TZrChar *zr_debug_eval_peek_non_ws(const ZrDebugEvalParser *parser)
 *
 * ### Summary
 * 为条件运算缺项诊断观察下一个非空白字符，同时保留真实解析光标。
 *
 * ### Parameters
 * @param[in] parser 可空的借用状态；函数仅复制 cursor 后扫描，不修改状态。
 *
 * ### Returns
 * @return 有效输入返回指向其中下一个非空白字符或末尾 NUL 的借用指针；无解析器或光标返回 NULL。
 *
 * ### Constraints and lifecycle
 * 返回指针的有效期受输入存储约束，不能释放或在输入失效后保留。仅跳过与 skip_ws 相同的四种空白，不验证后续表达式。
 *
 * ### References
 * - zr_debug_eval_next_is_missing_consequent_boundary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:74
 * - zr_debug_eval_next_is_missing_alternate_boundary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:80
 * - zr_debug_eval_skip_ws，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:26
 */

/**
 * @fn static TZrBool zr_debug_eval_next_is_missing_consequent_boundary(const ZrDebugEvalParser *parser)
 *
 * ### Summary
 * 在已消费问号后识别明显缺失的真分支，令诊断区分条件表达式缺项与更深层普通语法错误。
 *
 * ### Parameters
 * @param[in] parser 供只读观察的解析状态。
 *
 * ### Returns
 * @return 无可读光标、输入结束或下一个非空白字符为冒号、右圆括号、右方括号时返回 TRUE；其它字符返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 只作边界分类，不推进光标；FALSE 不保证后续表达式有效。冒号在此表示真分支缺失，其含义与假分支边界不同。
 *
 * ### References
 * - zr_debug_eval_peek_non_ws，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:60
 * - zr_debug_eval_parse_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:893
 */

/**
 * @fn static TZrBool zr_debug_eval_next_is_missing_alternate_boundary(const ZrDebugEvalParser *parser)
 *
 * ### Summary
 * 在已消费冒号后识别明显缺失的假分支，以免把结束分隔符交给普通主表达式诊断。
 *
 * ### Parameters
 * @param[in] parser 供只读观察的解析状态。
 *
 * ### Returns
 * @return 无可读光标、输入结束或下一个非空白字符为右圆括号、右方括号时返回 TRUE；其它字符返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 不推进光标，FALSE 不证明语法有效；冒号不在本函数的缺失边界集合内，留给递归表达式判定。
 *
 * ### References
 * - zr_debug_eval_peek_non_ws，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:60
 * - zr_debug_eval_parse_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:910
 */

/**
 * @fn static TZrBool zr_debug_eval_is_ident_start(TZrChar ch)
 *
 * ### Summary
 * 限定兼容解析器名称的 ASCII 首字符集合，避免将任意字节或数字当作暂停帧名称开头。
 *
 * ### Parameters
 * @param[in] ch 待分类的一个字符值。
 *
 * ### Returns
 * @return ASCII 字母或下划线返回 TRUE；其它字符返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 不按区域设置或 Unicode 标识符规则扩展，不能用作主语言词法器的等价替代。
 *
 * ### References
 * - zr_debug_eval_is_ident_part，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:90
 * - zr_debug_eval_parse_identifier，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:94
 */

/**
 * @fn static TZrBool zr_debug_eval_is_ident_part(TZrChar ch)
 *
 * ### Summary
 * 在已识别名称开头后允许 ASCII 数字继续出现，供兼容名称扫描消费完整词。
 *
 * ### Parameters
 * @param[in] ch 待分类的一个字符值。
 *
 * ### Returns
 * @return 标识符首字符集合或 ASCII 数字返回 TRUE；其它字符返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 仅约束兼容表达式名称，不进行编码解码、规范化或主语言关键字分类。
 *
 * ### References
 * - zr_debug_eval_is_ident_start，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:86
 * - zr_debug_eval_parse_identifier，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:94
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_identifier(ZrDebugEvalParser *parser, TZrChar *buffer, TZrSize bufferSize)
 *
 * ### Summary
 * 消费完整 ASCII 标识符并在可用缓冲区中保存有界名称，保证截断不会把剩余名称误当成新语法。
 *
 * ### Parameters
 * @param[in,out] parser 借用输入状态；跳过空白并在成功时消费整个名称。
 * @param[out] buffer 可空输出；容量非零时先写空串，再保存最多 bufferSize-1 字节。
 * @param[in] bufferSize buffer 的可写容量；零或空缓冲区时仍可只消费名称。
 *
 * ### Returns
 * @return 有合法首字符并完成扫描返回 TRUE，即使名称输出截断；无有效输入或首字符不合法返回 FALSE。
 *
 * ### Constraints and lifecycle
 * TRUE 不证明名称完整装入缓冲区，也不进行名称解析。FALSE 可能已消费前导空白；输出 NUL 保障以缓冲区非空且容量非零为前提。
 *
 * ### References
 * - zr_debug_eval_parse_identifier 中完整消费约束，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:105
 * - zr_debug_eval_parse_primary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388
 * - zr_debug_eval_parse_postfix，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:467
 */

/**
 * @fn static void zr_debug_eval_append_reference_suffix(ZrDebugEvalParser *parser, const TZrChar *suffix)
 *
 * ### Summary
 * 仅在已有引用摘要后追加访问后缀，使展示文本保留先前名称来源而不凭空生成来源。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；只可能修改其中指向的引用摘要缓冲区。
 * @param[in] suffix 调用期间有效的非空 NUL 后缀；空或 NULL 时不追加。
 *
 * ### Returns
 * 无返回值；没有缓冲区、容量、已有摘要或可用空间时保持现有内容。
 *
 * ### Constraints and lifecycle
 * 现有摘要必须在可读存储内 NUL 结尾，strlen 不是有界探测。追加使用剩余容量并强制末字节 NUL；截断不另返回状态，摘要不能作为精确身份或可重新求值的表达式。
 *
 * ### References
 * - zr_debug_eval_append_member_reference_suffix，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:160
 * - zr_debug_eval_parse_postfix，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:467
 * - ZrDebugEvalParser.reference_buffer，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_internal.h:15
 */

/**
 * @fn static void zr_debug_eval_append_reference_summary(ZrDebugEvalParser *parser, const TZrChar *summary)
 *
 * ### Summary
 * 把一个已取得的名称来源摘要加入请求输出，多个来源用逗号分隔以辅助显示表达式读取了什么。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；修改其引用摘要输出，不改变输入光标。
 * @param[in] summary 调用期间有效的 NUL 摘要；NULL 或空串忽略。
 *
 * ### Returns
 * 无返回值；无可用输出时不写入。
 *
 * ### Constraints and lifecycle
 * 原输出必须为有效 NUL 文本。缓冲区归调用方，有界追加可能截断但不报告截断；不分配、固定对象或延长被描述值的生命周期。
 *
 * ### References
 * - zr_debug_eval_parse_primary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388
 * - zr_debug_eval_append_reference_suffix，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:120
 * - zr_debug_try_evaluate_index_window，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2373
 */

/**
 * @fn static void zr_debug_eval_append_member_reference_suffix(ZrDebugEvalParser *parser, const TZrChar *memberName)
 *
 * ### Summary
 * 把成员名称格式化为展示性访问后缀，再沿已有引用摘要的追加规则记录该访问。
 *
 * ### Parameters
 * @param[in,out] parser 借用解析状态及其摘要输出。
 * @param[in] memberName 调用期间有效的非空 NUL 成员名；NULL 或空串忽略。
 *
 * ### Returns
 * 无返回值；是否有摘要、剩余空间及最终截断由下层追加规则决定。
 *
 * ### Constraints and lifecycle
 * 临时格式化缓冲区在栈上，名称可能先在此截断；不解析或读取该成员，不转移 memberName 或摘要缓冲区所有权。
 *
 * ### References
 * - zr_debug_eval_append_reference_suffix，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:120
 * - zr_debug_eval_parse_postfix，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:467
 */

/**
 * @fn static TZrBool zr_debug_eval_is_integer_type(EZrValueType type)
 *
 * ### Summary
 * 按运行时类型标签识别八种有符号及无符号整数类别，为兼容数值转换保留整数来源标记。
 *
 * ### Parameters
 * @param[in] type 待分类的 EZrValueType 标签。
 *
 * ### Returns
 * @return INT8/16/32/64 或 UINT8/16/32/64 返回 TRUE，其余标签返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 仅检查标签，不读取载荷或证明其范围适于有符号 int64；无符号标签在后续兼容转换中仍按 nativeInt64 读取。
 *
 * ### References
 * - zr_debug_eval_to_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:207
 */

/**
 * @fn static TZrBool zr_debug_eval_truthy(const SZrTypeValue *value)
 *
 * ### Summary
 * 按兼容求值约定计算逻辑真值，为否定、逻辑短路和条件分支选择提供同一规则。
 *
 * ### Parameters
 * @param[in] value 可空借用值；对象及字符串载荷在调用期间须保持有效。
 *
 * ### Returns
 * @return NULL 指针或 NULL 值为 FALSE；布尔、整数和浮点按载荷是否非零判断；字符串及其它对象按对象指针是否非空判断。
 *
 * ### Constraints and lifecycle
 * 不调用用户转换或重载；字符串真值不看内容长度。该规则只属于当前兼容窗口，不能据此声明与正式语言全部类型的真值规则等价。
 *
 * ### References
 * - zr_debug_eval_parse_unary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:543
 * - zr_debug_eval_parse_logical_and，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:818
 * - zr_debug_eval_parse_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:874
 */

/**
 * @fn static TZrBool zr_debug_eval_to_number(const SZrTypeValue *value, TZrFloat64 *outNumber, TZrBool *outIsInteger)
 *
 * ### Summary
 * 把兼容运算接受的数字载荷投影为 double，并单独记录其原标签是否属于整数家族。
 *
 * ### Parameters
 * @param[in] value 可空的借用值；仅整数与 FLOAT/DOUBLE 标签可转换。
 * @param[out] outNumber 可空输出；进入时先写 0，成功时写转换后的 double。
 * @param[out] outIsInteger 可空输出；进入时先写 FALSE，整数标签成功时置 TRUE。
 *
 * ### Returns
 * @return 接受该数值标签返回 TRUE，即使两个输出均为空；NULL 或其它标签返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 整数路径读取 nativeInt64 后转换为 double，可能丢失精度；不检查无符号范围、有限性或整数精确可表示性。只写调用方输出，不保留 value。
 *
 * ### References
 * - zr_debug_eval_is_integer_type，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:172
 * - zr_debug_eval_equal_values，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:250
 * - zr_debug_eval_apply_numeric_binary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:586
 */

/**
 * @fn static void zr_debug_eval_assign_number(SZrState *state, SZrTypeValue *outValue, TZrFloat64 number, TZrBool isInteger)
 *
 * ### Summary
 * 把兼容运算的 double 中间结果按整数来源标志写回 VM 值，集中当前路径的数值结果表示。
 *
 * ### Parameters
 * @param[in] state 传给核心值初始化器的借用上下文；当前两个初始化器不使用该参数。
 * @param[out] outValue 可空目标；非空时覆盖为 INT64 或 DOUBLE 值。
 * @param[in] number 待写入的 double 结果。
 * @param[in] isInteger 为 TRUE 时先强制转换为 TZrInt64，否则保存为浮点。
 *
 * ### Returns
 * 无返回值；目标为空时不执行转换或写入。
 *
 * ### Constraints and lifecycle
 * 这里不是受检转换：整数路径没有 isfinite 或 int64 范围守门，调用路径必须审视转换前提，不能据 TRUE 标志推断结果一定可表示。不拥有 state，不分配对象或建立恢复点。
 *
 * ### References
 * - zr_debug_eval_parse_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:352
 * - zr_debug_eval_apply_numeric_binary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:586
 * - ZrCore_Value_InitAsInt，zr_vm_core/src/zr_vm_core/value/value.c:79
 * - ZrCore_Value_InitAsFloat，zr_vm_core/src/zr_vm_core/value/value.c:103
 */

/**
 * @fn static TZrBool zr_debug_eval_equal_values(SZrState *state, const SZrTypeValue *left, const SZrTypeValue *right, TZrBool *outEqual)
 *
 * ### Summary
 * 为兼容的 == 和 != 比较数值、布尔、空值、字符串文本或同类型对象身份，不分派用户运算符。
 *
 * ### Parameters
 * @param[in] state 用于核心字符串转换宏的借用 VM 上下文。
 * @param[in] left 必须非空且载荷有效的左值。
 * @param[in] right 必须非空且载荷有效的右值。
 * @param[out] outEqual 必须非空的布尔输出；进入时先置 FALSE。
 *
 * ### Returns
 * @return 三个必需值/输出指针有效时按当前比较规则返回 TRUE；任一个无效返回 FALSE。比较结果由 outEqual 表示。
 *
 * ### Constraints and lifecycle
 * 数字先转 double，因此大整数/无符号精度不是严格整数等值保证。字符串使用 NUL 文本 strcmp，不按完整字节长度比较嵌入 NUL；对象须仍属于有效 VM/GC 生命周期，函数不增加根或释放对象。
 *
 * ### References
 * - zr_debug_eval_to_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:207
 * - zr_debug_eval_parse_equality，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:780
 * - ZrCore_String_GetNativeString，zr_vm_core/include/zr_vm_core/string.h:218
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_string(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 消费兼容双引号字面量和有限转义，并把形成的 NUL 文本交给核心字符串创建入口。
 *
 * ### Parameters
 * @param[in,out] parser 借用解析状态；消费输入、写失败诊断，并通过 agent->state 创建字符串。
 * @param[out] outValue 有效目标；成功时写入 STRING 值，失败时不承诺保持原值或统一清空。
 *
 * ### Returns
 * @return 完整闭合、长度可装入临时缓冲区且对象创建成功返回 TRUE；无该字面量、语法/长度失败或普通 NULL 创建结果返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 仅接受 n/r/t/反斜线/双引号转义，长度限 ZR_DEBUG_TEXT_CAPACITY-1；不是二进制字符串入口。skip_evaluation 也会走到创建，所以未选分支仍可分配。核心创建可非本地 Throw；本函数不捕获，不添加 GC 根，返回对象继续由 VM 管理。
 *
 * ### References
 * - zr_debug_eval_parse_primary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388
 * - zr_debug_eval_parse_string 的创建调用，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:341
 * - ZrCore_String_CreateFromNative，zr_vm_core/include/zr_vm_core/string.h:195
 * - string_create_short 的异常边界，zr_vm_core/src/zr_vm_core/string/string.c:671
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_number(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 把兼容的十进制整数或带一个小数点的字面量累积为 double，再按是否出现小数点决定结果标签。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；跳过空白并消费数字，非法重复小数点时写诊断。
 * @param[out] outValue 由数值赋值器覆盖的有效输出；失败不作全程回滚。
 *
 * ### Returns
 * @return 至少识别合法数字开头并完成当前扫描返回 TRUE；不是数字或遇到第二个小数点返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 不支持指数或其它进制；符号由一元层处理。累积可能失去精度或溢出为非有限值；无小数点时后续直接转 int64，没有范围检查。skip_evaluation 仍进行此转换，不能把未选分支称为纯词法扫描。
 *
 * ### References
 * - zr_debug_eval_assign_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:238
 * - zr_debug_eval_parse_primary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388
 * - zr_debug_eval_parse_unary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:543
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_primary(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 识别分组、字面量和兼容名称，并在需要读取值之前拒绝赋值与函数调用语法。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态、暂停帧及诊断/摘要输出；按语法推进 cursor。
 * @param[out] outValue 有效输出；短路名称写 NULL，正常名称读取或字面量成功时写结果。
 *
 * ### Returns
 * @return 主表达式完成返回 TRUE；缺少括号/名称、禁止语法或下层解析/读取失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * skip_evaluation 在名称读取前生效，不读取暂停帧；此前字符串仍可能分配、数字仍可能转换。值与引用摘要借用 VM/调用方存储，失败不回滚 cursor 或先前写入。本层失败诊断沿用错误缓冲区读取的既有 DED-BUG-001 边界，不能接受非 NUL 错误数组并据容量零推断安全。
 *
 * ### References
 * - zr_debug_eval_parse_primary 的短路块，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:438
 * - zr_debug_resolve_identifier_value，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1991
 * - DED-BUG-001 既有静态路径，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_internal.h:82
 * - zr_debug_eval_parse_string，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:294
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_postfix(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 在主表达式后继续解析成员和索引访问，正常路径经调试安全访问器取值，短路路径只消费其语法。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态及 VM/暂停帧、诊断和引用摘要。
 * @param[out] outValue 有效的当前接收者/最终结果存储；后缀成功时覆盖为借用值副本。
 *
 * ### Returns
 * @return 所有已出现后缀解析成功返回 TRUE；缺失成员/索引分隔符、访问失败或函数调用语法返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 方括号内递归解析完整表达式；skip_evaluation 不调用成员/索引读取器，但不免除内部字面量分配。安全访问不等于无分配，核心 Throw 可传播；没有新建 GC 根或失败事务，摘要可截断。
 *
 * ### References
 * - zr_debug_eval_parse_primary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:388
 * - zr_debug_safe_get_member_value，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2073
 * - zr_debug_safe_get_index_value，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2154
 * - zr_debug_eval_parse_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:874
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_unary(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 按右递归优先级处理逻辑否定与数值负号，再把其它输入交给后缀解析。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；读取 skip_evaluation，并推进输入或记录数值诊断。
 * @param[out] outValue 有效结果存储；出现 ! 或 - 时按跳过状态写 NULL 或相应布尔/数值结果；无一元运算符时原样保留后缀层结果。
 *
 * ### Returns
 * @return 操作数与运算完成返回 TRUE；递归失败或负号操作数不是数值返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 短路路径仍解析操作数，但跳过本层数值运算。负号经 double 中间值与原整数标志重新赋值，没有另行溢出/范围检查；不提供非本地异常恢复。
 *
 * ### References
 * - zr_debug_eval_parse_unary 的递归定义，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:543
 * - zr_debug_eval_truthy，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:178
 * - zr_debug_eval_to_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:207
 * - zr_debug_eval_assign_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:238
 */

/**
 * @fn static TZrBool zr_debug_eval_apply_numeric_binary(ZrDebugEvalParser *parser, const SZrTypeValue *left, const SZrTypeValue *right, const TZrChar *op, SZrTypeValue *outValue)
 *
 * ### Summary
 * 在兼容的二元数值层执行算术或大小比较，并为类型不符和零除数提供具体诊断。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；写错误文本并为结果初始化器提供 VM 上下文。
 * @param[in] left 有效借用左操作数，不转移其所有权。
 * @param[in] right 有效借用右操作数，不转移其所有权。
 * @param[in] op 当前调用点给定的非空 NUL 运算符常量。
 * @param[out] outValue 有效结果目标；成功时写数字或布尔值。
 *
 * ### Returns
 * @return 已支持运算及前置类型检查完成返回 TRUE；类型不符、除/余数零值或未知运算符返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 转换与比较基于 double，不保证精确整数运算。模运算先把两个 double 转为 int64，之后才检查整数标志和零除数，且没有 INT64_MIN/-1 守门；这里不能称为完整受检算术。失败只记录局部诊断，不回滚上层输入。
 *
 * ### References
 * - zr_debug_eval_to_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:207
 * - zr_debug_eval_assign_number，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:238
 * - zr_debug_eval_parse_multiplicative，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:643
 * - zr_debug_eval_parse_relational，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:742
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_multiplicative(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 以左结合顺序处理乘、除、模这一层，把每个右操作数交给一元层及统一诊断包装器。
 *
 * ### Parameters
 * @param[in,out] parser 借用解析状态；同步回调解析右操作数并推进输入。
 * @param[out] outValue 有效中间/最终值；处理已出现的 *、/、% 时按跳过状态写 NULL 或数值结果；没有本层运算符时保留下层结果。
 *
 * ### Returns
 * @return 本层及所有出现的右操作数完成返回 TRUE；下层语法、诊断包装或数值运算失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 传递的函数指针仅在调用栈内同步调用，不保存或异步发布。skip_evaluation 抑制本层运算但不免除下层字面量动作；无回滚或异常恢复点。
 *
 * ### References
 * - zr_debug_eval_parse_unary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:543
 * - zr_debug_eval_parse_right_operand，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:286
 * - zr_debug_eval_apply_numeric_binary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:586
 * - zr_debug_eval_parse_additive，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:700
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_additive(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 以左结合顺序组合乘法层结果，遇到加减号时复用右操作数诊断并更新累计值。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；读取运算符并同步调用乘法层。
 * @param[out] outValue 有效中间/最终值；处理已出现的 +、- 时按跳过状态写 NULL 或运算结果；没有本层运算符时保留下层结果。
 *
 * ### Returns
 * @return 加减层完成返回 TRUE；右侧解析或数值应用失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 不改变低层数字表示与范围前提；FALSE 可能已有光标和诊断变化。解析回调不离开本次同步调用，未建立清理/longjmp 恢复边界。
 *
 * ### References
 * - zr_debug_eval_parse_multiplicative，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:643
 * - zr_debug_eval_parse_right_operand，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:286
 * - zr_debug_eval_apply_numeric_binary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:586
 * - zr_debug_eval_parse_relational，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:742
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_relational(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 先读加减层，再按最长运算符优先处理大小关系，保持兼容解析器当前的左结合行为。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；消费 <=、>=、<、> 及右操作数。
 * @param[out] outValue 有效累计值；处理已出现的 <=、>=、<、> 时按跳过状态写 NULL 或布尔值；没有关系运算符时保留下层结果。
 *
 * ### Returns
 * @return 关系层完成返回 TRUE；操作数语法或数值应用失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 先匹配双字符符号防止前缀误消费。连续关系会把上次布尔结果作为下一次左值，后续数值检查可能拒绝；不能宣称实现数学式链式比较。无输入或结果事务。
 *
 * ### References
 * - zr_debug_eval_match_text，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:43
 * - zr_debug_eval_parse_additive，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:700
 * - zr_debug_eval_apply_numeric_binary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:586
 * - zr_debug_eval_parse_right_operand，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:286
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_equality(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 在关系层之后处理 == 与 !=，使用兼容值比较器形成布尔结果或细化缺失右值诊断。
 *
 * ### Parameters
 * @param[in,out] parser 借用解析状态；推进输入并可能写错误文本。
 * @param[out] outValue 有效累计/最终值；处理已出现的 ==、!= 时按跳过状态写 NULL 或布尔值；没有相等运算符时保留下层结果。
 *
 * ### Returns
 * @return 相等层完成返回 TRUE；右侧解析或值比较前置条件失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 比较规则由 equal_values 决定，包含 double 精度与 NUL 文本边界；不分派用户 equality 回调。未选路径仍解析两侧，只有本层比较被跳过。
 *
 * ### References
 * - zr_debug_eval_parse_relational，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:742
 * - zr_debug_eval_equal_values，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:250
 * - zr_debug_eval_parse_right_operand，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:286
 * - zr_debug_eval_parse_logical_and，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:818
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_logical_and(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 在 && 链中依据累计左值选择是否读取右侧运行时值，同时仍要求每个右操作数满足兼容语法限制。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；传递并临时叠加 skip_evaluation。
 * @param[out] outValue 有效累计/最终值；处理已出现的 && 后，外层跳过时写 NULL，否则写布尔值；没有 && 时保留下层结果。
 *
 * ### Returns
 * @return 与表达式完成返回 TRUE；右侧语法/诊断失败返回 FALSE，即使该侧未被选中求值。
 *
 * ### Constraints and lifecycle
 * 左假或外层已跳过时，包装器暂设跳过标志；正常 C 返回才恢复，非本地 Throw 不由此处恢复。跳过不等于无分配：下层字符串/数字字面量仍可能工作。
 *
 * ### References
 * - zr_debug_eval_truthy，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:178
 * - zr_debug_eval_parse_equality，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:780
 * - zr_debug_eval_parse_right_operand_with_skip，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:298
 * - zr_debug_eval_parse_logical_or，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:846
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_logical_or(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 在 || 链中由累计左值决定是否抑制右侧运行时读取，并保留未选分支的语法和禁止调用检查。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；推进输入并通过包装器临时叠加跳过标志。
 * @param[out] outValue 有效累计/最终值；处理已出现的 || 后，外层跳过时写 NULL，否则写布尔值；没有 || 时保留下层结果。
 *
 * ### Returns
 * @return 或表达式完成返回 TRUE；右侧语法/诊断失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 左真或外层跳过使右侧处于跳过状态，普通返回恢复先前标志；不承诺 Throw 时恢复。字面量解析仍可能分配/转换，未选语法错误仍会失败。
 *
 * ### References
 * - zr_debug_eval_truthy，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:178
 * - zr_debug_eval_parse_logical_and，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:818
 * - zr_debug_eval_parse_right_operand_with_skip，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:298
 * - zr_debug_eval_parse_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:874
 */

/**
 * @fn static TZrBool zr_debug_eval_parse_expression(ZrDebugEvalParser *parser, SZrTypeValue *outValue)
 *
 * ### Summary
 * 在逻辑或层之上解析条件运算符，以专门缺项诊断和递归分支解析完成兼容表达式。
 *
 * ### Parameters
 * @param[in,out] parser 借用状态；消费条件及两分支并临时传递跳过状态。
 * @param[out] outValue 有效结果目标；出现 ? 并完成分支解析后，按跳过状态写 NULL 或复制选中分支；没有 ? 时保留逻辑或层结果。
 *
 * ### Returns
 * @return 整个当前表达式完成返回 TRUE；缺真/假分支、冒号或任何下层解析失败返回 FALSE。
 *
 * ### Constraints and lifecycle
 * 两分支均递归解析，未选分支抑制帧/成员/索引读取及受守门的运算，但仍可能分配字符串或转换数字。临时标志仅正常返回恢复，值副本不新增 GC 根；分隔符、cursor 和输出都无整体事务。
 *
 * ### References
 * - zr_debug_eval_parse_logical_or，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:846
 * - zr_debug_eval_next_is_missing_consequent_boundary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:74
 * - zr_debug_eval_next_is_missing_alternate_boundary，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:80
 * - zr_debug_eval_parse_right_operand_with_skip，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_diagnostics.c:298
 */

/**
 * @fn TZrBool zr_debug_evaluate_expression_with_capabilities( ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, TZrUInt32 allowedEffectFlags, TZrBool allowLegacyCompatibility, SZrTypeValue *outValue, TZrUInt32 *outCanonicalTypeId, ZrDebugEvaluateFailure *outFailure, TZrChar *errorBuffer, TZrSize errorBufferSize, TZrChar *referenceBuffer, TZrSize referenceBufferSize)
 *
 * ### Summary
 * 作为内部能力入口先请求正式求值，再按调用方授予的兼容许可和正式层报告决定是否进入旧语法窗口，并归类失败。
 *
 * ### Parameters
 * @param[in,out] agent 有效调试代理；其 VM/暂停帧由调用方维持，正式准备及兼容字符串访问可能分配。
 * @param[in] frameId 正式/兼容名称解析使用的帧编号；本函数不独立检查暂停状态。
 * @param[in] expression 整个同步求值期间有效的 NUL 表达式文本。
 * @param[in] allowedEffectFlags 交给正式层的允许效果位，不由此函数扩大权限。
 * @param[in] allowLegacyCompatibility 仅 TRUE 时允许正式层未处理或报告 parserFailure 的入口进入兼容路径。
 * @param[out] outValue 必需结果目标；入口先置 NULL，之后失败不承诺回滚下层写入。
 * @param[out] outCanonicalTypeId 可空；入口和兼容前置无效值，正式成功可给出规范类型编号。
 * @param[out] outFailure 可空的本次失败详情；入口清空，随后按正式/兼容失败点填写。
 * @param[out] errorBuffer 可空错误文本缓冲区；容量须真实，现有读取路径还要求非空指针可读 NUL 文本。
 * @param[in] errorBufferSize errorBuffer 可写容量；零容量不豁免既有失败路径的读取约束。
 * @param[out] referenceBuffer 可空有界引用摘要；非空且容量非零时入口清空，后续可截断。
 * @param[in] referenceBufferSize referenceBuffer 的实际可写容量。
 *
 * ### Returns
 * @return 正式层成功且 handled，或获准兼容层完整消费输入时返回 TRUE；非法请求、被拒正式失败/未处理、兼容失败或剩余 token 返回 FALSE。非本地 Throw 可直接传播。
 *
 * ### Constraints and lifecycle
 * formal-first 块按实际 outParserFailure 分类，不将所有正式语义失败都回退；断点条件/日志点传 FALSE。暂停与有效帧约束由上层入口负责。没有本地恢复点、根/pin或失败事务；输出初始化不是全部失败回滚。BUG: DED-BUG-001 既有族：非空零容量且未 NUL 结尾错误数组仍会被失败判断/诊断比较读取；完整证据见内部解析状态文档，未经本轮运行复现。
 *
 * ### References
 * - zr_debug_formal_evaluate_expression，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_formal_evaluation.c:307
 * - zr_debug_evaluate_with_policy，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2476
 * - DED-BUG-001 既有路径，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval_internal.h:82
 * - zr_debug_breakpoint_condition_evaluate，zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.c:57
 * - zr_debug_evaluate_expression_with_capabilities 的兼容收尾，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:1025
 */

/**
 * @fn TZrBool zr_debug_evaluate_expression(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, SZrTypeValue *outValue, TZrChar *errorBuffer, TZrSize errorBufferSize, TZrChar *referenceBuffer, TZrSize referenceBufferSize)
 *
 * ### Summary
 * 为内部索引窗口辅助路径保留无正式副作用能力的兼容调用包装，省略规范类型和结构化失败输出。
 *
 * ### Parameters
 * @param[in,out] agent 借用调试代理及 VM；与能力入口具有相同的有效暂停/帧生命周期前提。
 * @param[in] frameId 供下层解析名称的帧编号。
 * @param[in] expression 同步调用期间有效的 NUL 表达式。
 * @param[out] outValue 必需输出；初始化与局部失败边界由能力入口决定。
 * @param[out] errorBuffer 可空错误输出；非空时仍须符合下层 NUL 可读约束。
 * @param[in] errorBufferSize errorBuffer 的实际可写容量。
 * @param[out] referenceBuffer 可空的调用方引用摘要缓冲区。
 * @param[in] referenceBufferSize referenceBuffer 的实际可写容量。
 *
 * ### Returns
 * @return 原样返回能力入口结果；TRUE 表示其正式或兼容求值成功，FALSE 保留其失败边界，Throw 不在此捕获。
 *
 * ### Constraints and lifecycle
 * 固定 allowedEffectFlags=NONE、allowLegacyCompatibility=TRUE，不是对外公开 ABI 或全局无分配证明。当前可检索实际调用来自索引窗口基值及两边界子表达式；不新增根或延长返回对象寿命。
 *
 * ### References
 * - zr_debug_evaluate_expression_with_capabilities，zr_vm_lib_debug/src/zr_vm_lib_debug/eval/debug_eval.c:932
 * - zr_debug_try_evaluate_index_window 的三个内部调用，zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2409
 * - 内部求值声明，zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:420
 */
