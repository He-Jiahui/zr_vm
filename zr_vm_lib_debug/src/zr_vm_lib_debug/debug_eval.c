#include "debug_eval_internal.h"

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

TZrBool zr_debug_evaluate_expression(ZrDebugAgent *agent,
                                     TZrUInt32 frameId,
                                     const TZrChar *expression,
                                     SZrTypeValue *outValue,
                                     TZrChar *errorBuffer,
                                     TZrSize errorBufferSize,
                                     TZrChar *referenceBuffer,
                                     TZrSize referenceBufferSize) {
    ZrDebugEvalParser parser;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
    }
    if (referenceBuffer != ZR_NULL && referenceBufferSize > 0) {
        referenceBuffer[0] = '\0';
    }
    if (agent == ZR_NULL || expression == ZR_NULL || outValue == ZR_NULL) {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "invalid debug evaluate request");
        return ZR_FALSE;
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
        return ZR_FALSE;
    }

    zr_debug_eval_skip_ws(&parser);
    if (parser.cursor != ZR_NULL && *parser.cursor != '\0') {
        if (zr_debug_eval_cursor_starts_assignment(&parser)) {
            zr_debug_eval_set_assignment_error(&parser);
        } else {
            zr_debug_copy_text(errorBuffer, errorBufferSize, "unexpected trailing tokens in debug evaluate");
        }
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
