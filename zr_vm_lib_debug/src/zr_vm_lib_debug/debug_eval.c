#include "debug_internal.h"

typedef struct ZrDebugEvalParser {
    ZrDebugAgent *agent;
    TZrUInt32 frame_id;
    const TZrChar *cursor;
    TZrChar *error_buffer;
    TZrSize error_buffer_size;
} ZrDebugEvalParser;

static void zr_debug_eval_set_error(ZrDebugEvalParser *parser, const TZrChar *message) {
    if (parser == ZR_NULL) {
        return;
    }
    zr_debug_copy_text(parser->error_buffer,
                       parser->error_buffer_size,
                       message != ZR_NULL ? message : "debug evaluate error");
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
                    zr_debug_eval_set_error(parser, "unsupported string escape in debug evaluate");
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
        zr_debug_eval_set_error(parser, "unterminated string literal in debug evaluate");
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
                zr_debug_eval_set_error(parser, "invalid numeric literal in debug evaluate");
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
            zr_debug_eval_set_error(parser, "missing ')' in debug evaluate expression");
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (zr_debug_eval_parse_string(parser, outValue) || zr_debug_eval_parse_number(parser, outValue)) {
        return ZR_TRUE;
    }

    if (!zr_debug_eval_parse_identifier(parser, identifier, sizeof(identifier))) {
        zr_debug_eval_set_error(parser, "expected expression in debug evaluate");
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

    return zr_debug_resolve_identifier_value(parser->agent,
                                             parser->frame_id,
                                             identifier,
                                             outValue,
                                             parser->error_buffer,
                                             parser->error_buffer_size);
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
                zr_debug_eval_set_error(parser, "expected member name after '.' in debug evaluate");
                return ZR_FALSE;
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
                zr_debug_eval_set_error(parser, "missing ']' in debug evaluate");
                return ZR_FALSE;
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
            continue;
        }

        zr_debug_eval_skip_ws(parser);
        if (parser->cursor != ZR_NULL && *parser->cursor == '(') {
            zr_debug_eval_set_error(parser, "function calls are not allowed in safe debug evaluate");
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
        if (!zr_debug_eval_to_number(&operand, &number, &isInteger)) {
            zr_debug_eval_set_error(parser, "unary '-' expects a numeric value");
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
        zr_debug_eval_set_error(parser, "numeric operator expects numeric operands");
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
            zr_debug_eval_set_error(parser, "division by zero in debug evaluate");
            return ZR_FALSE;
        }
        zr_debug_eval_assign_number(parser->agent->state, outValue, leftNumber / rightNumber, ZR_FALSE);
    } else if (strcmp(op, "%") == 0) {
        TZrInt64 leftInt = (TZrInt64)leftNumber;
        TZrInt64 rightInt = (TZrInt64)rightNumber;
        if (!leftIsInteger || !rightIsInteger || rightInt == 0) {
            zr_debug_eval_set_error(parser, "modulo expects non-zero integer operands");
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
            if (!zr_debug_eval_parse_unary(parser, &rhs) ||
                !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "*", &result)) {
                return ZR_FALSE;
            }
            *outValue = result;
            continue;
        }
        if (zr_debug_eval_match_text(parser, "/")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            if (!zr_debug_eval_parse_unary(parser, &rhs) ||
                !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "/", &result)) {
                return ZR_FALSE;
            }
            *outValue = result;
            continue;
        }
        if (zr_debug_eval_match_text(parser, "%")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            if (!zr_debug_eval_parse_unary(parser, &rhs) ||
                !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "%", &result)) {
                return ZR_FALSE;
            }
            *outValue = result;
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
            if (!zr_debug_eval_parse_multiplicative(parser, &rhs) ||
                !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "+", &result)) {
                return ZR_FALSE;
            }
            *outValue = result;
            continue;
        }
        if (zr_debug_eval_match_text(parser, "-")) {
            SZrTypeValue rhs;
            SZrTypeValue result;
            if (!zr_debug_eval_parse_multiplicative(parser, &rhs) ||
                !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, "-", &result)) {
                return ZR_FALSE;
            }
            *outValue = result;
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
        if (!zr_debug_eval_parse_additive(parser, &rhs) ||
            !zr_debug_eval_apply_numeric_binary(parser, outValue, &rhs, op, &result)) {
            return ZR_FALSE;
        }
        *outValue = result;
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
        if (!zr_debug_eval_parse_relational(parser, &rhs) ||
            !zr_debug_eval_equal_values(parser->agent->state, outValue, &rhs, &isEqual)) {
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
        ZrCore_Value_ResetAsNull(&rhs);
        if (!zr_debug_eval_parse_equality(parser, &rhs)) {
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsBool(parser->agent->state,
                                outValue,
                                (zr_debug_eval_truthy(outValue) && zr_debug_eval_truthy(&rhs)) ? ZR_TRUE : ZR_FALSE);
    }

    return ZR_TRUE;
}

static TZrBool zr_debug_eval_parse_expression(ZrDebugEvalParser *parser, SZrTypeValue *outValue) {
    if (!zr_debug_eval_parse_logical_and(parser, outValue)) {
        return ZR_FALSE;
    }

    while (zr_debug_eval_match_text(parser, "||")) {
        SZrTypeValue rhs;
        ZrCore_Value_ResetAsNull(&rhs);
        if (!zr_debug_eval_parse_logical_and(parser, &rhs)) {
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsBool(parser->agent->state,
                                outValue,
                                (zr_debug_eval_truthy(outValue) || zr_debug_eval_truthy(&rhs)) ? ZR_TRUE : ZR_FALSE);
    }

    return ZR_TRUE;
}

TZrBool zr_debug_evaluate_expression(ZrDebugAgent *agent,
                                     TZrUInt32 frameId,
                                     const TZrChar *expression,
                                     SZrTypeValue *outValue,
                                     TZrChar *errorBuffer,
                                     TZrSize errorBufferSize) {
    ZrDebugEvalParser parser;

    if (outValue != ZR_NULL) {
        ZrCore_Value_ResetAsNull(outValue);
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

    if (!zr_debug_eval_parse_expression(&parser, outValue)) {
        if (errorBuffer != ZR_NULL && errorBuffer[0] == '\0') {
            zr_debug_copy_text(errorBuffer, errorBufferSize, "failed to parse debug evaluate expression");
        }
        return ZR_FALSE;
    }

    zr_debug_eval_skip_ws(&parser);
    if (parser.cursor != ZR_NULL && *parser.cursor != '\0') {
        zr_debug_copy_text(errorBuffer, errorBufferSize, "unexpected trailing tokens in debug evaluate");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
