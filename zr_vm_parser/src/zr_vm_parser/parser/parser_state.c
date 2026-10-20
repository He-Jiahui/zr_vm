#include "parser_internal.h"
#include "parser_source_projection.h"

void parser_report_nesting_error(SZrParserState *ps) {
    if (ps->hasNestingError && !ps->nestingErrorReported &&
        (ps->errorCallback != ZR_NULL || !ps->suppressErrorOutput)) {
        /* A speculative syntax probe may have suppressed the first report. */
        ps->hasNestingError = ZR_FALSE;
        report_error(ps, "Source nesting exceeds the parser recursion limit");
        ps->hasNestingError = ZR_TRUE;
        ps->nestingErrorReported = ZR_TRUE;
    }
}

TZrBool parser_enter_recursion(SZrParserState *ps) {
    if (ps == ZR_NULL || ps->hasNestingError) return ZR_FALSE;
    if (ps->recursionDepth >= ZR_PARSER_MAX_RECURSION_DEPTH) {
        ps->hasError = ZR_TRUE;
        ps->hasNestingError = ZR_TRUE;
        ps->hasFatalError = ZR_TRUE;
        ps->errorMessage = "Source nesting exceeds the parser recursion limit";
        parser_report_nesting_error(ps);
        return ZR_FALSE;
    }
    ++ps->recursionDepth;
    return ZR_TRUE;
}

void expect_token(SZrParserState *ps, EZrToken expected) {
    if (ps->lexer->t.token != expected) {
        const TZrChar *expectedStr = ZrParser_Lexer_TokenToString(ps->lexer, expected);
        const TZrChar *actualStr = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
        TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(errorMsg, sizeof(errorMsg), "期望 '%s'，但遇到 '%s'", expectedStr, actualStr);
        report_error_with_token(ps, errorMsg, ps->lexer->t.token);
    }
}

// 消费 token（如果匹配）

TZrBool consume_token(SZrParserState *ps, EZrToken token) {
    if (ps->lexer->t.token == token) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

// 查看下一个 token（不消费）

EZrToken peek_token(SZrParserState *ps) { return ZrParser_Lexer_Lookahead(ps->lexer); }

void save_parser_cursor(SZrParserState *ps, SZrParserCursor *cursor) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || cursor == ZR_NULL) {
        return;
    }

    ZrParser_TokenSource_Save(ps->lexer, &cursor->tokenSource);
    cursor->currentPos = ps->lexer->currentPos;
    cursor->currentChar = ps->lexer->currentChar;
    cursor->lineNumber = ps->lexer->lineNumber;
    cursor->lastLine = ps->lexer->lastLine;
    cursor->currentLineStartOffset = ps->lexer->currentLineStartOffset;
    cursor->tokenStartOffset = ps->lexer->tokenStartOffset;
    cursor->tokenStartLineStart = ps->lexer->tokenStartLineStart;
    cursor->tokenStartLine = ps->lexer->tokenStartLine;
    /* token 与 lookahead 负载沿用 lexer/state 的寿命，回滚不复制字符串。 */
    cursor->token = ps->lexer->t;
    cursor->lookahead = ps->lexer->lookahead;
    cursor->lookaheadPos = ps->lexer->lookaheadPos;
    cursor->lookaheadChar = ps->lexer->lookaheadChar;
    cursor->lookaheadLine = ps->lexer->lookaheadLine;
    cursor->lookaheadLastLine = ps->lexer->lookaheadLastLine;
    cursor->lookaheadCurrentLineStartOffset = ps->lexer->lookaheadCurrentLineStartOffset;
    cursor->lookaheadTokenStartOffset = ps->lexer->lookaheadTokenStartOffset;
    cursor->lookaheadTokenStartLineStart = ps->lexer->lookaheadTokenStartLineStart;
    cursor->lookaheadTokenStartLine = ps->lexer->lookaheadTokenStartLine;
    cursor->hasError = ps->hasError;
    cursor->hasFatalError = ps->hasFatalError;
    cursor->errorMessage = ps->errorMessage;
}

void restore_parser_cursor(SZrParserState *ps, const SZrParserCursor *cursor) {
    TZrBool fatal;
    const TZrChar *fatalMessage;
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || cursor == ZR_NULL) {
        return;
    }

    fatal = ps->hasFatalError || ps->hasNestingError || ps->lexer->hasSourceEvidenceError;
    fatalMessage = ps->errorMessage;

    ZrParser_TokenSource_Restore(ps->lexer, &cursor->tokenSource);
    ps->lexer->currentPos = cursor->currentPos;
    ps->lexer->currentChar = cursor->currentChar;
    ps->lexer->lineNumber = cursor->lineNumber;
    ps->lexer->lastLine = cursor->lastLine;
    ps->lexer->currentLineStartOffset = cursor->currentLineStartOffset;
    ps->lexer->tokenStartOffset = cursor->tokenStartOffset;
    ps->lexer->tokenStartLineStart = cursor->tokenStartLineStart;
    ps->lexer->tokenStartLine = cursor->tokenStartLine;
    ps->lexer->t = cursor->token;
    ps->lexer->lookahead = cursor->lookahead;
    ps->lexer->lookaheadPos = cursor->lookaheadPos;
    ps->lexer->lookaheadChar = cursor->lookaheadChar;
    ps->lexer->lookaheadLine = cursor->lookaheadLine;
    ps->lexer->lookaheadLastLine = cursor->lookaheadLastLine;
    ps->lexer->lookaheadCurrentLineStartOffset = cursor->lookaheadCurrentLineStartOffset;
    ps->lexer->lookaheadTokenStartOffset = cursor->lookaheadTokenStartOffset;
    ps->lexer->lookaheadTokenStartLineStart = cursor->lookaheadTokenStartLineStart;
    ps->lexer->lookaheadTokenStartLine = cursor->lookaheadTokenStartLine;
    /* Speculative probes preserve both nesting and source-evidence failures. */
    ps->hasError = fatal || cursor->hasError;
    ps->hasFatalError = fatal || cursor->hasFatalError;
    ps->errorMessage = fatal ? fatalMessage : cursor->errorMessage;
}

TZrBool current_identifier_equals(SZrParserState *ps, const TZrChar *text) {
    SZrString *identName;
    TZrNativeString nameStr;

    if (ps == ZR_NULL || text == ZR_NULL || ps->lexer->t.token != ZR_TK_IDENTIFIER) {
        return ZR_FALSE;
    }

    identName = ps->lexer->t.seminfo.stringValue;
    if (identName == ZR_NULL) {
        return ZR_FALSE;
    }

    nameStr = ZrCore_String_GetNativeString(identName);
    return nameStr != ZR_NULL && strcmp(nameStr, text) == 0;
}

/* 旧百分号写法仅用于迁移诊断；表项不参与生产语法接受。 */
typedef struct SZrRemovedPercentSyntaxRule {
    const TZrChar *name;
    const TZrChar *suggestion;
} SZrRemovedPercentSyntaxRule;

#define ZR_REMOVED_PERCENT_NAME_CAPACITY 32U

static const SZrRemovedPercentSyntaxRule k_removed_percent_syntax_rules[] = {
        {"module", "Write `module path;`."},
        {"import", "Use a module-scope immutable binding: `let alias = import(\"literal.path\");`."},
        {"async", "Write `async fn name(...): zr.task.Task<T>`."},
        {"await", "Write `await expression`."},
        {"extern", "Write `native extern(\"library\") { ... }`."},
        {"test", "Attach `#zr.testing.test#` to an ordinary `fn(...): void`."},
        {"compileTime", "Use `comptime fn`, `comptime if`, a comptime block, or `const` data."},
        {"func", "Write callable types as `fn(...) -> R`."},
        {"owned", "Write `resource class Name`."},
        {"release", "Use the canonical `drop(value)` ownership operation."},
        {"upgrade", "Use wake(weak), the typed weak-reference ownership intrinsic."},
        {"weak", "Use the typed owner projection API."},
        {"shared", "Use `share(owner)` or an explicit `Shared<T>` contract."},
        {"detach", "Use the typed ownership bridge API after proving the source owner kind."},
        {"unique", "Construct a resource with `own Type(...)` or declare `Unique<T>` explicitly."},
        {"in", "Write the parameter as `name: in T`."},
        {"ref", "Write the parameter as `name: ref T` and mark the call argument with `ref`."},
        {"out", "Write the parameter as `name: out T` and mark the call argument with `out`."},
        {"borrow", "Use a canonical `ref` binding or reborrow expression."},
        {"loan", "Use a canonical scoped reference contract."},
        {"borrowed", "Use a canonical reference TypeRef."},
        {"loaned", "Use a canonical scoped reference TypeRef."},
        {"type", "Use `typeof(expr)` for a runtime descriptor or `typeid(TypeRef)` for identity."},
        {"using", "Use the ordinary `using` statement only for the supported resource lifetime form."},
};

/* 仅识别已登记的旧拼写；生产入口将其标为 fatal，迁移入口保留诊断。 */
TZrBool report_removed_percent_syntax(SZrParserState *ps) {
    const TZrChar *source;
    TZrSize sourceLength;
    SZrFileRange location;
    TZrSize start;
    TZrSize end;
    TZrChar name[ZR_REMOVED_PERCENT_NAME_CAPACITY];
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *suggestion = ZR_NULL;
    TZrSize ruleIndex;
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL ||
        ps->lexer->t.token != ZR_TK_PERCENT || ps->lexer->source == ZR_NULL) {
        return ZR_FALSE;
    }

    source = ps->lexer->source;
    sourceLength = ps->lexer->sourceLength;
    location = get_current_token_location(ps);
    start = location.start.offset;
    if (start + 1U >= sourceLength ||
        !((source[start + 1U] >= 'A' && source[start + 1U] <= 'Z') ||
          (source[start + 1U] >= 'a' && source[start + 1U] <= 'z') ||
          source[start + 1U] == '_')) {
        return ZR_FALSE;
    }

    end = start + 1U;
    while (end < sourceLength &&
           ((source[end] >= 'A' && source[end] <= 'Z') ||
            (source[end] >= 'a' && source[end] <= 'z') ||
            (source[end] >= '0' && source[end] <= '9') ||
            source[end] == '_')) {
        end++;
    }
    if (end - (start + 1U) >= sizeof(name)) {
        return ZR_FALSE;
    }
    memcpy(name, source + start + 1U, end - (start + 1U));
    name[end - (start + 1U)] = '\0';

    for (ruleIndex = 0U;
         ruleIndex < sizeof(k_removed_percent_syntax_rules) / sizeof(k_removed_percent_syntax_rules[0]);
         ruleIndex++) {
        if (strcmp(name, k_removed_percent_syntax_rules[ruleIndex].name) == 0) {
            suggestion = k_removed_percent_syntax_rules[ruleIndex].suggestion;
            break;
        }
    }
    if (suggestion == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ps->enableLegacyMigrationParsing) {
        ps->hasFatalError = ZR_TRUE;
    }

    location = ZrParser_FileRange_WithoutEvidence(location);
    location.end.offset = end;
    location.end.column = location.start.column + (TZrInt32)(end - start);
    snprintf(message,
             sizeof(message),
             "Legacy syntax '%%%s' was removed from the production parser",
             name);
    if (!ZrParser_DiagnosticBuilder_Build(
                ps->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "legacy_syntax_removed",
                message,
                "The one-time syntax cutover keeps legacy recognition only for migration diagnostics.",
                suggestion)) {
        report_error_with_token(ps, message, ps->lexer->t.token);
        return ZR_TRUE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT)) {
        ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
        report_error_with_token(ps, message, ps->lexer->t.token);
        return ZR_TRUE;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);

    ZrParser_Lexer_Next(ps->lexer);
    if (ps->lexer->t.token != ZR_TK_EOS) {
        ZrParser_Lexer_Next(ps->lexer);
    }
    return ZR_TRUE;
}

void report_removed_legacy_syntax_at(SZrParserState *ps,
                                     SZrFileRange location,
                                     EZrToken token,
                                     const TZrChar *spelling,
                                     const TZrChar *suggestion) {
    SZrStructuredDiagnostic diagnostic;
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (ps == ZR_NULL || ps->state == ZR_NULL ||
        spelling == ZR_NULL || suggestion == ZR_NULL) {
        return;
    }

    if (!ps->enableLegacyMigrationParsing) {
        ps->hasFatalError = ZR_TRUE;
    }

    snprintf(message,
             sizeof(message),
             "Legacy syntax '%s' was removed from the production parser",
             spelling);
    if (!ZrParser_DiagnosticBuilder_Build(
                ps->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "legacy_syntax_removed",
                message,
                "The one-time syntax cutover keeps legacy recognition only for migration diagnostics.",
                suggestion)) {
        report_error_with_token(ps, message, token);
        return;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_INSUFFICIENT_CONTEXT)) {
        ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
        report_error_with_token(ps, message, token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_removed_legacy_syntax(SZrParserState *ps,
                                  const TZrChar *spelling,
                                  const TZrChar *suggestion) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    report_removed_legacy_syntax_at(ps,
                                    get_current_token_location(ps),
                                    ps->lexer->t.token,
                                    spelling,
                                    suggestion);
    ZrParser_Lexer_Next(ps->lexer);
}

TZrBool is_module_path_segment_token(EZrToken token) { return token == ZR_TK_IDENTIFIER || token == ZR_TK_TEST; }

TZrBool is_type_modifier_token(EZrToken token) {
    return token == ZR_TK_ABSTRACT || token == ZR_TK_FINAL;
}

TZrBool is_member_modifier_token(EZrToken token) {
    return token == ZR_TK_ABSTRACT || token == ZR_TK_VIRTUAL || token == ZR_TK_OVERRIDE ||
           token == ZR_TK_FINAL || token == ZR_TK_SHADOW;
}

TZrUInt32 token_to_declaration_modifier_flag(EZrToken token) {
    switch (token) {
        case ZR_TK_ABSTRACT:
            return ZR_DECLARATION_MODIFIER_ABSTRACT;
        case ZR_TK_VIRTUAL:
            return ZR_DECLARATION_MODIFIER_VIRTUAL;
        case ZR_TK_OVERRIDE:
            return ZR_DECLARATION_MODIFIER_OVERRIDE;
        case ZR_TK_FINAL:
            return ZR_DECLARATION_MODIFIER_FINAL;
        case ZR_TK_SHADOW:
            return ZR_DECLARATION_MODIFIER_SHADOW;
        default:
            return ZR_DECLARATION_MODIFIER_NONE;
    }
}

TZrUInt32 parse_declaration_modifier_flags(SZrParserState *ps, TZrUInt32 allowedFlags) {
    TZrUInt32 flags = ZR_DECLARATION_MODIFIER_NONE;

    if (ps == ZR_NULL) {
        return ZR_DECLARATION_MODIFIER_NONE;
    }

    while (ps->lexer != ZR_NULL) {
        TZrUInt32 flag = token_to_declaration_modifier_flag(ps->lexer->t.token);
        if (flag == ZR_DECLARATION_MODIFIER_NONE || (allowedFlags & flag) == 0) {
            break;
        }

        if ((flags & flag) != 0) {
            report_error(ps, "Duplicate declaration modifier");
        }

        flags |= flag;
        ZrParser_Lexer_Next(ps->lexer);
    }

    return flags;
}

void skip_balanced_after_open_paren(SZrParserState *ps) {
    TZrInt32 depth = 1;

    while (ps != ZR_NULL && ps->lexer->t.token != ZR_TK_EOS && depth > 0) {
        if (ps->lexer->t.token == ZR_TK_LPAREN) {
            depth++;
        } else if (ps->lexer->t.token == ZR_TK_RPAREN) {
            depth--;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }
}

void skip_to_semicolon_or_eos(SZrParserState *ps) {
    while (ps != ZR_NULL && ps->lexer->t.token != ZR_TK_EOS) {
        if (ps->lexer->t.token == ZR_TK_SEMICOLON) {
            ZrParser_Lexer_Next(ps->lexer);
            break;
        }
        ZrParser_Lexer_Next(ps->lexer);
    }
}

SZrAstNode *parse_normalized_dotted_module_path(SZrParserState *ps, const TZrChar *directiveName) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize length = 0;
    SZrFileRange startLoc;
    SZrFileRange endLoc;
    SZrString *pathString;
    TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    if (!is_module_path_segment_token(ps->lexer->t.token)) {
        snprintf(errorMsg, sizeof(errorMsg), "Expected module path after %s", directiveName);
        report_error(ps, errorMsg);
        return ZR_NULL;
    }

    startLoc = get_current_token_location(ps);
    endLoc = startLoc;

    /* 字符串由 VM 管理，返回的 AST 节点引用它；先约束缓冲区再创建。 */
    while (is_module_path_segment_token(ps->lexer->t.token)) {
        EZrToken segmentToken = ps->lexer->t.token;
        SZrString *segment = ps->lexer->t.seminfo.stringValue;
        TZrNativeString nativeSegment = segmentToken == ZR_TK_TEST
                                                ? "test"
                                                : (segment != ZR_NULL ? ZrCore_String_GetNativeString(segment)
                                                                     : ZR_NULL);
        TZrSize segmentLength = nativeSegment != ZR_NULL ? ZrCore_NativeString_Length(nativeSegment) : 0;

        endLoc = get_current_token_location(ps);

        if (nativeSegment == ZR_NULL) {
            snprintf(errorMsg, sizeof(errorMsg), "Invalid module path segment in %s", directiveName);
            report_error(ps, errorMsg);
            return ZR_NULL;
        }

        if (length > 0) {
            if (length + 1 >= sizeof(buffer)) {
                report_error(ps, "Module path is too long");
                return ZR_NULL;
            }
            buffer[length++] = '.';
        }

        if (length + segmentLength >= sizeof(buffer)) {
            report_error(ps, "Module path is too long");
            return ZR_NULL;
        }

        memcpy(buffer + length, nativeSegment, segmentLength);
        length += segmentLength;
        ZrParser_Lexer_Next(ps->lexer);

        if (!consume_token(ps, ZR_TK_DOT)) {
            break;
        }
        if (!is_module_path_segment_token(ps->lexer->t.token)) {
            snprintf(errorMsg, sizeof(errorMsg), "Expected identifier after '.' in %s path", directiveName);
            report_error(ps, errorMsg);
            return ZR_NULL;
        }
    }

    SZrFileRange pathLocation;
    if (!ZrParser_MergeLocations(ps, startLoc, endLoc, &pathLocation)) {
        return ZR_NULL;
    }
    buffer[length] = '\0';
    pathString = ZrParser_Lexer_CreateString(ps->lexer, buffer, length);
    if (pathString == ZR_NULL) {
        report_error(ps, "Failed to allocate module path string");
        return ZR_NULL;
    }

    return create_string_literal_node_with_location(
            ps,
            pathString,
            ZR_FALSE,
            pathString,
            pathLocation);
}

SZrAstNode *parse_normalized_module_path(SZrParserState *ps, const TZrChar *directiveName) {
    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    if (ps->lexer->t.token == ZR_TK_LPAREN) {
        ZrParser_Lexer_Next(ps->lexer);
        if (ps->lexer->t.token != ZR_TK_STRING) {
            report_import_path_not_constant(ps, get_current_token_location(ps), directiveName);
            skip_balanced_after_open_paren(ps);
            return ZR_NULL;
        }

        {
            SZrAstNode *stringNode = parse_literal(ps);
            expect_token(ps, ZR_TK_RPAREN);
            consume_token(ps, ZR_TK_RPAREN);
            return stringNode;
        }
    }

    report_removed_legacy_syntax(
            ps,
            "bare import path",
            "Use `import(\"module.path\")`; static import paths must be string literals in parentheses.");
    return ZR_NULL;
}

SZrAstNodeArray *parse_leading_decorators(SZrParserState *ps) {
    SZrAstNodeArray *decorators;

    if (ps == ZR_NULL) {
        return ZR_NULL;
    }

    decorators = parser_ast_array_new(ps, 2);
    if (decorators == ZR_NULL) {
        return ZR_NULL;
    }

    while (ps->lexer->t.token == ZR_TK_SHARP) {
        SZrAstNode *decorator = parse_decorator_expression(ps);
        if (decorator == ZR_NULL) {
            break;
        }
        if (!parser_ast_array_add(ps, decorators, decorator)) {
            ZrParser_Ast_Free(ps->state, decorator);
            free_ast_node_array_with_elements(ps->state, decorators);
            return ZR_NULL;
        }
    }

    return decorators;
}

TZrBool consume_type_closing_angle(SZrParserState *ps) {
    if (ps->lexer->t.token == ZR_TK_GREATER_THAN) {
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }

    /* 嵌套泛型中的 >> 先消费一个 >，把剩余字符交回 token source. */
    if (ps->lexer->t.token == ZR_TK_RIGHT_SHIFT) {
        if (!ZrParser_Lexer_SplitCurrentPunct(ps->lexer, '>')) {
            return ZR_FALSE;
        }
        /* Splitting leaves the first piece current; consume it here. */
        ZrParser_Lexer_Next(ps->lexer);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

// 获取当前位置信息

SZrFileRange get_current_location(SZrParserState *ps) {
    /* Historical issue in the local projection replaced by the shared helper: */
    /* BUG: 首行从非零偏移回溯时循环在 pos==0 停止，未计入首字节；
     * parse_script 等入口据此生成 AST 范围，结束列会比 1-based 列少一。 */
    return ZrParser_SourceProjection_CurrentLocation(ps);
}

void get_string_view_for_length(SZrString *value, const TZrChar **text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

SZrFilePosition get_file_position_from_offset(SZrLexState *lexer, TZrSize offset) {
    TZrInt32 line = 1;
    TZrSize lineStart = 0;
    TZrSize index = 0;

    if (lexer == ZR_NULL || lexer->source == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, 1, 1);
    }

    if (offset > lexer->sourceLength) {
        offset = lexer->sourceLength;
    }

    if (lexer->filePositionCacheOffset <= offset) {
        index = lexer->filePositionCacheOffset;
        line = lexer->filePositionCacheLine > 0 ? lexer->filePositionCacheLine : 1;
        lineStart = lexer->filePositionCacheLineStart;
    }

    /* BUG: lexer 将单独的 \r 视为换行，而此处只计 \n；旧属性迁移诊断以本
     * 函数生成名称/类型范围，含单独 CR 的源文件会得到上一行的行列。 */
    for (; index < offset; index++) {
        if (lexer->source[index] == '\n') {
            line++;
            lineStart = index + 1;
        }
    }

    lexer->filePositionCacheOffset = offset;
    lexer->filePositionCacheLineStart = lineStart;
    lexer->filePositionCacheLine = line;

    return ZrParser_FilePosition_Create(offset, line, (TZrInt32) (offset - lineStart + 1));
}

TZrSize get_current_token_length(SZrParserState *ps) {
    const TZrChar *text = ZR_NULL;
    TZrSize length = 0;
    const TZrChar *tokenText;

    if (ps == ZR_NULL || ps->lexer == ZR_NULL) {
        return 0;
    }
    if (ps->lexer->tokenSourceState != ZR_NULL) {
        return ps->lexer->t.rawLength;
    }

    switch (ps->lexer->t.token) {
        case ZR_TK_IDENTIFIER:
            get_string_view_for_length(ps->lexer->t.seminfo.stringValue, &text, &length);
            return length;

        case ZR_TK_INTEGER:
        case ZR_TK_FLOAT:
            get_string_view_for_length(ps->lexer->t.seminfo.stringValue, &text, &length);
            return length;

        case ZR_TK_BOOLEAN:
            return ps->lexer->t.seminfo.booleanValue ? 4 : 5;

        case ZR_TK_EOS:
            return 0;

        default:
            tokenText = ZrParser_Lexer_TokenToString(ps->lexer, ps->lexer->t.token);
            return tokenText != ZR_NULL ? strlen(tokenText) : 0;
    }
}

SZrFileRange get_current_token_location(SZrParserState *ps) {
    return ZrParser_SourceProjection_CurrentLocation(ps);
}

// 获取当前行的代码片段（前后各20个字符）

void get_line_snippet(SZrParserState *ps, TZrChar *buffer, TZrSize bufferSize, TZrInt32 *errorColumn) {
    if (ps->lexer == ZR_NULL || bufferSize == 0) {
        buffer[0] = '\0';
        *errorColumn = 1;
        return;
    }
    if (ps->lexer->source == ZR_NULL) {
        TZrSize length = ps->lexer->t.rawLength;
        if (length >= bufferSize) {
            length = bufferSize - 1U;
        }
        if (length > 0U && ps->lexer->t.rawText != ZR_NULL) {
            memcpy(buffer, ps->lexer->t.rawText, length);
        }
        buffer[length] = '\0';
        *errorColumn = ps->lexer->t.span.startColumn > 0
                                ? ps->lexer->t.span.startColumn
                                : 1;
        return;
    }

    // 计算列号并找到行首
    TZrSize pos = ps->lexer->currentPos;
    TZrInt32 column = 1;
    TZrSize lineStart = pos;

    /* BUG: lexer 把单独 CR 当换行，这里的行首/行尾扫描只认 LF；
     * 裸 CR 后的语法错误会把相邻两行拼成一段并错置插入符。 */
    // 向前查找行首
    while (lineStart > 0 && ps->lexer->source[lineStart - 1] != '\n') {
        lineStart--;
        column++;
    }

    // 向后查找行尾
    TZrSize lineEnd = pos;
    while (lineEnd < ps->lexer->sourceLength && ps->lexer->source[lineEnd] != '\n') {
        lineEnd++;
    }

    // 计算要显示的起始和结束位置（前后各固定上下文字符）
    TZrSize snippetStart = lineStart;
    TZrSize snippetEnd = lineEnd;
    TZrInt32 displayColumn = column;

    // 如果列号大于上下文半径，向前移动起始位置
    if (column > (TZrInt32)ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS) {
        snippetStart = pos - ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS;
        displayColumn = (TZrInt32)ZR_PARSER_ERROR_SNIPPET_FOCUS_COLUMN;
    }

    // 如果剩余字符少于上下文半径，向后扩展
    if (lineEnd - pos < ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS) {
        TZrSize needed = ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS - (lineEnd - pos);
        if (snippetStart >= needed) {
            snippetStart -= needed;
            displayColumn += (TZrInt32)needed;
        } else {
            snippetStart = 0;
            displayColumn = column;
        }
    }

    // BUG: 长行上这里把片段起点退回行首，后续最多复制 127 字节却保留原始列号；
    // 从第 128 列起错误 token 不在片段内，日志中的 ^ 会落在片段右侧。
    // 确保不越界
    if (snippetStart > lineStart) {
        snippetStart = lineStart;
        displayColumn = column;
    }
    if (snippetEnd > ps->lexer->sourceLength) {
        snippetEnd = ps->lexer->sourceLength;
    }

    // 复制代码片段
    TZrSize snippetLen = snippetEnd - snippetStart;
    if (snippetLen >= bufferSize) {
        snippetLen = bufferSize - 1;
    }

    for (TZrSize i = 0; i < snippetLen; i++) {
        TZrChar c = ps->lexer->source[snippetStart + i];
        // 将制表符和换行符替换为空格以便显示
        if (c == '\t') {
            buffer[i] = ' ';
        } else if (c == '\n' || c == '\r') {
            buffer[i] = ' ';
        } else {
            buffer[i] = c;
        }
    }
    buffer[snippetLen] = '\0';

    *errorColumn = displayColumn;
}

// 报告解析错误（带 token 信息）

void report_error_with_token(SZrParserState *ps, const TZrChar *msg, EZrToken token) {
    SZrFileRange errorRange;

    /* Report the resource limit once; unwinding must preserve that diagnostic. */
    if (ps->hasNestingError) return;
    ps->hasError = ZR_TRUE;
    /* BUG: expect_token 或模块路径错误会传入栈上 errorMsg；公开的单语句
     * 解析入口返回后仍可读取 ps->errorMessage，该指针已悬空，文本可能损坏。 */
    ps->errorMessage = msg;
    errorRange = get_current_token_location(ps);

    if (ps->errorCallback != ZR_NULL) {
        ps->errorCallback(ps->errorUserData, &errorRange, msg, token);
    }

    if (!ps->suppressErrorOutput && ps->lexer != ZR_NULL) {
        // 获取 token 字符串
        const TZrChar *tokenStr = ZrParser_Lexer_TokenToString(ps->lexer, token);

        // 获取文件名
        const TZrChar *fileName = "<unknown>";
        if (ps->lexer->sourceName != ZR_NULL) {
            TZrNativeString nameStr = ZrCore_String_GetNativeString(ps->lexer->sourceName);
            if (nameStr != ZR_NULL) {
                fileName = nameStr;
            }
        }

        // 计算列号（从当前行开始到当前位置的字符数）
        TZrInt32 column = 1;
        if (ps->lexer->tokenSourceState != ZR_NULL &&
            ps->lexer->t.span.startColumn > 0) {
            column = ps->lexer->t.span.startColumn;
        } else if (ps->lexer->source != ZR_NULL && ps->lexer->currentPos > 0) {
            TZrSize pos = ps->lexer->currentPos - 1;
            while (pos > 0 && ps->lexer->source[pos] != '\n') {
                pos--;
                column++;
            }
        }

        // 获取代码片段
        TZrChar snippet[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
        TZrInt32 displayColumn = 1;
        get_line_snippet(ps, snippet, sizeof(snippet), &displayColumn);

        // 输出详细的错误信息
        // 使用 lastLine 而不是 lineNumber，因为 lastLine 是当前 token 的行号
        TZrInt32 errorLine = ps->lexer->lastLine > 0 ? ps->lexer->lastLine : ps->lexer->lineNumber;
        if (snippet[0] != '\0') {
            ZrCore_Log_Diagnosticf(ps->state,
                                   ZR_LOG_LEVEL_ERROR,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "  [%s:%d:%d] %s (遇到 token: '%s')\n"
                                   "    %s\n"
                                   "%*s^\n",
                                   fileName,
                                   errorLine,
                                   column,
                                   msg,
                                   tokenStr,
                                   snippet,
                                   displayColumn > 0 ? displayColumn - 1 : 0,
                                   "");
        } else {
            ZrCore_Log_Diagnosticf(ps->state,
                                   ZR_LOG_LEVEL_ERROR,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "  [%s:%d:%d] %s (遇到 token: '%s')\n",
                                   fileName,
                                   errorLine,
                                   column,
                                   msg,
                                   tokenStr);
        }
    }
}

// 报告解析错误

TZrBool report_reserved_ownership_intrinsic_name(SZrParserState *ps) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return ZR_FALSE;
    }
    location = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_Build(
                ps->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "reserved_ownership_intrinsic_name",
                "Ownership intrinsic name is reserved in lexical namespaces",
                "share, degrade, wake, intoGc, and drop are language-level ownership operations and cannot be rebound.",
                "Choose a different lexical binding name. These spellings remain legal as object member names.")) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
        return ZR_FALSE;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
    return ZR_TRUE;
}

void report_error(SZrParserState *ps, const TZrChar *msg) {
    if (ps->hasNestingError) return;
    ps->hasError = ZR_TRUE;
    ps->errorMessage = msg;

    // 使用词法分析器的错误报告函数
    if (ps->lexer != ZR_NULL) {
        EZrToken currentToken = ps->lexer->t.token;
        report_error_with_token(ps, msg, currentToken);
    }
}

void report_structured_parser_error(SZrParserState *ps,
                                    const SZrStructuredDiagnostic *diagnostic,
                                    EZrToken token) {
    const TZrChar *message;
    const TZrChar *cause = ZR_NULL;
    const TZrChar *suggestion = ZR_NULL;
    TZrChar formattedMessage[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (ps == ZR_NULL || ps->hasNestingError || diagnostic == ZR_NULL || diagnostic->message == ZR_NULL) {
        return;
    }

    message = ZrCore_String_GetNativeString(diagnostic->message);
    if (message == ZR_NULL) {
        return;
    }
    if (diagnostic->cause != ZR_NULL) {
        cause = ZrCore_String_GetNativeString(diagnostic->cause);
    }
    if (diagnostic->suggestion != ZR_NULL) {
        suggestion = ZrCore_String_GetNativeString(diagnostic->suggestion);
    }

    /* 回调只借用 diagnostic；构建方在本函数返回后释放其数组存储。 */
    if (ps->structuredErrorCallback != ZR_NULL) {
        ps->structuredErrorCallback(ps->errorUserData, diagnostic, token);
    }

    if (diagnostic->severity != ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        if (!ps->suppressErrorOutput && ps->lexer != ZR_NULL) {
            const TZrChar *fileName = "<unknown>";
            if (ps->lexer->sourceName != ZR_NULL) {
                TZrNativeString nameStr = ZrCore_String_GetNativeString(ps->lexer->sourceName);
                if (nameStr != ZR_NULL) {
                    fileName = nameStr;
                }
            }

            ZrCore_Log_Diagnosticf(ps->state,
                                   diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_WARNING
                                           ? ZR_LOG_LEVEL_WARNING
                                           : ZR_LOG_LEVEL_INFO,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "  [%s:%d:%d] %s\n",
                                   fileName,
                                   diagnostic->location.start.line,
                                   diagnostic->location.start.column,
                                   message);
        }
        return;
    }

    if (cause != ZR_NULL || suggestion != ZR_NULL) {
        if (cause != ZR_NULL && suggestion != ZR_NULL) {
            snprintf(formattedMessage,
                     sizeof(formattedMessage),
                     "%s\nCause: %s\nSuggestion: %s",
                     message,
                     cause,
                     suggestion);
        } else if (cause != ZR_NULL) {
            snprintf(formattedMessage, sizeof(formattedMessage), "%s\nCause: %s", message, cause);
        } else {
            snprintf(formattedMessage, sizeof(formattedMessage), "%s\nSuggestion: %s", message, suggestion);
        }
        report_error_with_token(ps, formattedMessage, token);
        ps->errorMessage = message;
    } else {
        report_error_with_token(ps, message, token);
    }
}

void report_missing_expression_after_assignment(SZrParserState *ps) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingExpressionAfterAssignment(
            ps->state,
            &diagnostic,
            get_current_token_location(ps))) {
        report_error_with_token(ps, "Missing expression after '='", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_right_operand(SZrParserState *ps, const TZrChar *operatorText, SZrFileRange operatorLocation) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingRightOperand(ps->state,
                                                             &diagnostic,
                                                             operatorLocation,
                                                             operatorText)) {
        report_error_with_token(ps, "Missing expression after operator", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_condition(SZrParserState *ps, const TZrChar *statementKind, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingCondition(ps->state,
                                                          &diagnostic,
                                                          location,
                                                          statementKind)) {
        report_error_with_token(ps, "Missing condition expression", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_condition_close(SZrParserState *ps, const TZrChar *statementKind, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingConditionClose(ps->state,
                                                               &diagnostic,
                                                               location,
                                                               statementKind)) {
        report_error_with_token(ps, "Missing ')' after condition", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_member_name(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingMemberName(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing member name after '.'", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_index_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange fixLocation;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    fixLocation = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_BuildMissingIndexClose(
                ps->state,
                &diagnostic,
                location,
                fixLocation)) {
        report_error_with_token(ps, "Missing closing ']' in index access", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_call_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange fixLocation;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    fixLocation = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_BuildMissingCallClose(
                ps->state,
                &diagnostic,
                location,
                fixLocation)) {
        report_error_with_token(ps, "Missing closing ')' in function call", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_parameter_list_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingParameterListClose(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing closing ')' in function declaration parameters", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_group_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange fixLocation;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    fixLocation = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_BuildMissingGroupClose(
                ps->state,
                &diagnostic,
                location,
                fixLocation)) {
        report_error_with_token(ps, "Missing closing ')' in grouped expression", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_array_element_assignment(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildArrayElementAssignment(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Array element cannot be an assignment expression", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_array_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange fixLocation;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    fixLocation = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_BuildMissingArrayClose(
                ps->state,
                &diagnostic,
                location,
                fixLocation)) {
        report_error_with_token(ps, "Missing closing ']' in array literal", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_array_element_separator(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingArrayElementSeparator(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing separator between array elements", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_object_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange fixLocation;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    fixLocation = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_BuildMissingObjectClose(
                ps->state,
                &diagnostic,
                location,
                fixLocation)) {
        report_error_with_token(ps, "Missing closing '}' in object literal", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_object_computed_key_close(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange fixLocation;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    fixLocation = get_current_token_location(ps);
    if (!ZrParser_DiagnosticBuilder_BuildMissingObjectComputedKeyClose(
                ps->state,
                &diagnostic,
                location,
                fixLocation)) {
        report_error_with_token(ps, "Missing closing ']' in computed object key", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_object_property_colon(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingObjectPropertyColon(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing ':' after object property key", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

void report_missing_object_property_separator(SZrParserState *ps, SZrFileRange location) {
    SZrStructuredDiagnostic diagnostic;

    if (ps == ZR_NULL || ps->state == ZR_NULL || ps->lexer == ZR_NULL) {
        return;
    }

    if (!ZrParser_DiagnosticBuilder_BuildMissingObjectPropertySeparator(ps->state, &diagnostic, location)) {
        report_error_with_token(ps, "Missing separator between object properties", ps->lexer->t.token);
        return;
    }

    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
}

// ==================== AST 节点创建辅助函数 ====================
