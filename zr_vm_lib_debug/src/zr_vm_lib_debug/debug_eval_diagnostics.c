#include "debug_eval_internal.h"

void zr_debug_eval_set_error(ZrDebugEvalParser *parser, const TZrChar *message) {
    if (parser == ZR_NULL) {
        return;
    }
    zr_debug_copy_text(parser->error_buffer,
                       parser->error_buffer_size,
                       message != ZR_NULL ? message : "debug evaluate error");
}

const TZrChar *zr_debug_eval_value_type_name(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return "value";
    }

    return zr_debug_value_type_name(value->type);
}

void zr_debug_eval_set_numeric_operand_error(ZrDebugEvalParser *parser,
                                             const TZrChar *op,
                                             const SZrTypeValue *left,
                                             const SZrTypeValue *right) {
    TZrChar message[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *operatorText = op != ZR_NULL ? op : "operator";

    if (parser == ZR_NULL) {
        return;
    }

    snprintf(message,
             sizeof(message),
             "Numeric operator '%s' expects numeric operands. Cause: left operand is %s and right operand is %s. "
             "Suggestion: Use numeric operands or convert values before applying '%s'.",
             operatorText,
             zr_debug_eval_value_type_name(left),
             zr_debug_eval_value_type_name(right),
             operatorText);
    message[sizeof(message) - 1u] = '\0';
    zr_debug_eval_set_error(parser, message);
}

void zr_debug_eval_set_division_by_zero_error(ZrDebugEvalParser *parser, const TZrChar *op) {
    TZrChar message[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *operatorText = op != ZR_NULL ? op : "/";

    if (parser == ZR_NULL) {
        return;
    }

    snprintf(message,
             sizeof(message),
             "division by zero in debug evaluate. Cause: operator '%s' received a zero right-hand operand. "
             "Suggestion: guard the divisor or use a non-zero value before evaluating the expression.",
             operatorText);
    message[sizeof(message) - 1u] = '\0';
    zr_debug_eval_set_error(parser, message);
}

void zr_debug_eval_set_missing_member_name_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Missing member name after '.'. Cause: debug evaluate or conditional breakpoint member access ended "
        "before an identifier. Suggestion: add the member name after '.' or remove the member access.");
}

void zr_debug_eval_set_missing_index_close_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Missing closing ']' in index access. Cause: debug evaluate or conditional breakpoint index access "
        "started with '[' but was not closed after the index expression. Suggestion: add ']' after the index "
        "expression or remove the index access.");
}

void zr_debug_eval_set_missing_group_close_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Missing closing ')' in grouped expression. Cause: debug evaluate or conditional breakpoint grouped "
        "expression started with '(' but was not closed after the inner expression. Suggestion: add ')' after "
        "the grouped expression or remove the opening '('.");
}

void zr_debug_eval_set_missing_conditional_separator_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Missing ':' in conditional expression. Cause: debug evaluate or conditional breakpoint conditional "
        "expression started with '?' but did not provide an alternate branch. Suggestion: add ': <expression>' "
        "or remove the conditional operator.");
}

void zr_debug_eval_set_missing_conditional_consequent_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Missing consequent expression in conditional expression. Cause: debug evaluate or conditional breakpoint "
        "conditional expression reached ':' or ended immediately after '?'. Suggestion: add the expression between "
        "'?' and ':' or remove the conditional operator.");
}

void zr_debug_eval_set_missing_conditional_alternate_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Missing alternate expression in conditional expression. Cause: debug evaluate or conditional breakpoint "
        "conditional expression ended immediately after ':'. Suggestion: add the alternate expression after ':' or "
        "remove the conditional operator.");
}

void zr_debug_eval_set_unterminated_string_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Unterminated string literal in debug evaluate. Cause: the string started with '\"' but reached the end "
        "of the expression before a closing quote. Suggestion: add the closing quote or remove the unfinished "
        "string literal.");
}

void zr_debug_eval_set_unsupported_string_escape_error(ZrDebugEvalParser *parser, TZrChar escapeChar) {
    TZrChar message[ZR_DEBUG_TEXT_CAPACITY];

    if (parser == ZR_NULL) {
        return;
    }

    snprintf(message,
             sizeof(message),
             "Unsupported string escape '\\%c' in debug evaluate. Cause: this escape is not part of the safe "
             "evaluate subset. Suggestion: use a supported escape such as \\\\, \\\", \\n, \\r, or \\t.",
             escapeChar != '\0' ? escapeChar : '?');
    message[sizeof(message) - 1u] = '\0';
    zr_debug_eval_set_error(parser, message);
}

void zr_debug_eval_set_invalid_numeric_literal_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Invalid numeric literal in debug evaluate. Cause: the literal contains more than one decimal point. "
        "Suggestion: use a single decimal point or split the expression into valid numeric operands.");
}

void zr_debug_eval_set_function_call_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Function calls are not allowed in safe debug evaluate. Cause: debug evaluate and conditional "
        "breakpoints run in a read-only expression subset, and calls may execute user code or mutate state. "
        "Suggestion: inspect a local, member, index, or pure literal expression instead of calling a function.");
}

void zr_debug_eval_set_assignment_error(ZrDebugEvalParser *parser) {
    if (parser == ZR_NULL) {
        return;
    }

    zr_debug_eval_set_error(
        parser,
        "Assignment is not allowed in safe debug evaluate. Cause: debug evaluate and conditional breakpoints are "
        "read-only and must not mutate program state. Suggestion: inspect an expression without '=' or change the "
        "program state from source code instead.");
}

TZrBool zr_debug_eval_cursor_starts_assignment(const ZrDebugEvalParser *parser) {
    const TZrChar *cursor;

    if (parser == ZR_NULL || parser->cursor == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = parser->cursor;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor++;
    }

    return (TZrBool)(cursor[0] == '=' && cursor[1] != '=');
}

void zr_debug_eval_set_modulo_operand_error(ZrDebugEvalParser *parser,
                                            const SZrTypeValue *left,
                                            const SZrTypeValue *right,
                                            TZrBool leftIsInteger,
                                            TZrBool rightIsInteger,
                                            TZrBool rightIsZero) {
    TZrChar message[ZR_DEBUG_TEXT_CAPACITY];

    if (parser == ZR_NULL) {
        return;
    }

    snprintf(message,
             sizeof(message),
             "Modulo operator '%%' expects non-zero integer operands. Cause: left operand is %s%s and right operand "
             "is %s%s%s. Suggestion: Use integer operands and guard the divisor before evaluating the expression.",
             zr_debug_eval_value_type_name(left),
             leftIsInteger ? "" : " (not an integer)",
             zr_debug_eval_value_type_name(right),
             rightIsInteger ? "" : " (not an integer)",
             rightIsZero ? " (zero)" : "");
    message[sizeof(message) - 1u] = '\0';
    zr_debug_eval_set_error(parser, message);
}

static TZrBool zr_debug_eval_error_is_expected_expression(const ZrDebugEvalParser *parser) {
    return (TZrBool)(parser != ZR_NULL &&
                     parser->error_buffer != ZR_NULL &&
                     strcmp(parser->error_buffer, "expected expression in debug evaluate") == 0);
}

static TZrBool zr_debug_eval_cursor_is_missing_expression_boundary(const ZrDebugEvalParser *parser) {
    const TZrChar *cursor;

    if (parser == ZR_NULL || parser->cursor == ZR_NULL) {
        return ZR_TRUE;
    }

    cursor = parser->cursor;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor++;
    }

    return (TZrBool)(*cursor == '\0' || *cursor == ')' || *cursor == ']');
}

void zr_debug_eval_refine_right_operand_error(ZrDebugEvalParser *parser, const TZrChar *op) {
    TZrChar message[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *operatorText = op != ZR_NULL ? op : "operator";

    if (parser == ZR_NULL ||
        (parser->error_buffer != ZR_NULL && parser->error_buffer[0] != '\0' &&
         !zr_debug_eval_error_is_expected_expression(parser))) {
        return;
    }

    if (zr_debug_eval_cursor_is_missing_expression_boundary(parser)) {
        snprintf(message,
                 sizeof(message),
                 "Missing expression after '%s'. Cause: debug evaluate or conditional breakpoint expression ended "
                 "after a binary/logical operator. Suggestion: add the right-hand expression or remove the operator.",
                 operatorText);
    } else {
        snprintf(message,
                 sizeof(message),
                 "Invalid expression after '%s'. Cause: the next token cannot start a debug evaluate or conditional "
                 "breakpoint expression. Suggestion: use a literal, variable, member access, index access, or "
                 "parenthesized expression.",
                 operatorText);
    }
    message[sizeof(message) - 1u] = '\0';
    zr_debug_eval_set_error(parser, message);
}

TZrBool zr_debug_eval_parse_right_operand(ZrDebugEvalParser *parser,
                                          const TZrChar *op,
                                          FZrDebugEvalParseValue parseValue,
                                          SZrTypeValue *outValue) {
    if (parseValue != ZR_NULL && parseValue(parser, outValue)) {
        return ZR_TRUE;
    }

    zr_debug_eval_refine_right_operand_error(parser, op);
    return ZR_FALSE;
}

TZrBool zr_debug_eval_parse_right_operand_with_skip(ZrDebugEvalParser *parser,
                                                    const TZrChar *op,
                                                    FZrDebugEvalParseValue parseValue,
                                                    SZrTypeValue *outValue,
                                                    TZrBool skipEvaluation) {
    TZrBool previousSkip;
    TZrBool parsed;

    if (parser == ZR_NULL || !skipEvaluation) {
        return zr_debug_eval_parse_right_operand(parser, op, parseValue, outValue);
    }

    previousSkip = parser->skip_evaluation;
    parser->skip_evaluation = ZR_TRUE;
    parsed = zr_debug_eval_parse_right_operand(parser, op, parseValue, outValue);
    parser->skip_evaluation = previousSkip;
    return parsed;
}
