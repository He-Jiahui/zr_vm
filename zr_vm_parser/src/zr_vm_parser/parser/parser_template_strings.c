#include "parser_internal.h"

// 为模板插值建立独立 lexer 与源码副本；成功 AST 交外层模板段。
SZrAstNode *parse_embedded_expression(
        SZrParserState *ps, const TZrChar *source, TZrSize sourceLength,
        SZrSyntaxContext syntaxContext, SZrTokenSpan origin,
        const SZrFileRange *originLocation) {
    SZrParserState nestedParser;
    SZrAstNode *expression = ZR_NULL;
    TZrChar *sourceCopy;

    if (ps == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }

    if (sourceLength == ZR_MAX_SIZE) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Template interpolation source size overflow");
        return ZR_NULL;
    }
    sourceCopy = ZrParser_Ownership_Allocate(
            ps->lexer->ownership, sourceLength + 1u,
            ZR_MEMORY_NATIVE_TYPE_STRING, ZR_FALSE);
    if (sourceCopy == ZR_NULL) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to allocate template interpolation source");
        return ZR_NULL;
    }

    memcpy(sourceCopy, source, sourceLength);
    sourceCopy[sourceLength] = '\0';

    ZrParser_State_InitEmbeddedSource(
            &nestedParser, ps, sourceCopy, sourceLength, syntaxContext, origin,
            originLocation);
    if (!nestedParser.hasError) {
        expression = parse_expression(&nestedParser);
        if (nestedParser.hasError || expression == ZR_NULL || nestedParser.lexer->t.token != ZR_TK_EOS) {
            ZrParser_Ast_Free(ps->state, expression);
            expression = ZR_NULL;
        }
    }

    if (nestedParser.hasNestingError) {
        ps->hasNestingError = ps->hasFatalError = ps->hasError = ZR_TRUE;
        ps->errorMessage = nestedParser.errorMessage;
        ps->nestingErrorReported = nestedParser.nestingErrorReported;
    }
    if (nestedParser.hasFatalError) {
        ps->hasFatalError = ZR_TRUE;
    }
    if (nestedParser.lexer != ZR_NULL && nestedParser.lexer->hasSourceEvidenceError) {
        ps->hasError = ZR_TRUE;
        ps->hasFatalError = ZR_TRUE;
        ps->errorMessage = nestedParser.errorMessage;
        ps->lexer->hasSourceEvidenceError = ZR_TRUE;
        ps->lexer->t.hasLexError = ZR_TRUE;
        ps->lexer->t.lexErrorMessage = nestedParser.lexer->t.lexErrorMessage;
    }
    ZrParser_State_Free(&nestedParser);
    ZrCore_Memory_RawFreeWithType(ps->state->global, sourceCopy, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_STRING);
    return expression;
}

// 将静态文本复制为 VM 字符串并追加为模板片段，片段 AST 随数组转移。
TZrBool append_template_static_segment(SZrParserState *ps, SZrAstNodeArray *segments, const TZrChar *text,
                                              TZrSize length) {
    SZrString *segmentString;
    SZrAstNode *segmentNode;

    if (ps == ZR_NULL || segments == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    segmentString = ZrParser_Lexer_CreateString(ps->lexer, text, length);
    if (segmentString == ZR_NULL) {
        ps->hasFatalError = ZR_TRUE;
        report_error(ps, "Failed to allocate template string segment");
        return ZR_FALSE;
    }

    segmentNode = create_string_literal_node(ps, segmentString, ZR_FALSE, segmentString);
    if (segmentNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!parser_ast_array_add(ps, segments, segmentNode)) {
        ZrParser_Ast_Free(ps->state, segmentNode);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

// 扫描嵌套花括号、字符串与注释边界后分别解析静态段和插值表达式。
SZrAstNode *parse_template_string_literal(
        SZrParserState *ps, SZrString *rawValue,
        SZrSyntaxContext syntaxContext, SZrTokenSpan origin,
        const SZrFileRange *originLocation) {
    TZrNativeString rawText;
    TZrSize rawLength;
    SZrAstNodeArray *segments;
    TZrSize segmentStart = 0;
    TZrSize index = 0;

    if (ps == ZR_NULL || rawValue == ZR_NULL) {
        return ZR_NULL;
    }

    get_string_native_parts(rawValue, &rawText, &rawLength);
    if (rawText == ZR_NULL) {
        return ZR_NULL;
    }

    segments = parser_ast_array_new(ps, ZR_PARSER_INITIAL_CAPACITY_TINY);
    if (segments == ZR_NULL) {
        return ZR_NULL;
    }

    while (index < rawLength) {
        if (rawText[index] == '$' && index + 1 < rawLength && rawText[index + 1] == '{') {
            TZrSize expressionStart = index + 2;
            TZrSize cursor = expressionStart;
            TZrInt32 braceDepth = 1;
            TZrInt32 stringDelimiter = 0;

            if (!append_template_static_segment(ps, segments, rawText + segmentStart, index - segmentStart)) {
                free_ast_node_array_with_elements(ps->state, segments);
                return ZR_NULL;
            }

            while (cursor < rawLength) {
                TZrChar current = rawText[cursor];

                if (stringDelimiter != 0) {
                    if (current == '\\' && cursor + 1 < rawLength) {
                        cursor += 2;
                        continue;
                    }
                    if (current == stringDelimiter) {
                        stringDelimiter = 0;
                    }
                    cursor++;
                    continue;
                }

                if (current == '/' && cursor + 1 < rawLength) {
                    if (rawText[cursor + 1] == '/') {
                        cursor += 2;
                        while (cursor < rawLength && rawText[cursor] != '\n' &&
                               rawText[cursor] != '\r') {
                            cursor++;
                        }
                        continue;
                    }
                    if (rawText[cursor + 1] == '*') {
                        cursor += 2;
                        while (cursor + 1 < rawLength &&
                               !(rawText[cursor] == '*' && rawText[cursor + 1] == '/')) {
                            cursor++;
                        }
                        cursor = cursor + 1 < rawLength ? cursor + 2 : rawLength;
                        continue;
                    }
                }

                if (current == '\'' || current == '"' || current == '`') {
                    stringDelimiter = current;
                    cursor++;
                    continue;
                }

                if (current == '{') {
                    braceDepth++;
                } else if (current == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        break;
                    }
                }
                cursor++;
            }

            if (cursor >= rawLength || braceDepth != 0) {
                report_error(ps, "Unterminated template string interpolation");
                free_ast_node_array_with_elements(ps->state, segments);
                return ZR_NULL;
            }

            {
                SZrAstNode *expression =
                        parse_embedded_expression(
                                ps, rawText + expressionStart, cursor - expressionStart,
                                syntaxContext, origin, originLocation);
                SZrAstNode *segmentNode;

                if (expression == ZR_NULL) {
                    report_error(ps, "Failed to parse template string interpolation");
                    free_ast_node_array_with_elements(ps->state, segments);
                    return ZR_NULL;
                }

                segmentNode = create_interpolated_segment_node(ps, expression);
                if (segmentNode == ZR_NULL) {
                    ZrParser_Ast_Free(ps->state, expression);
                    free_ast_node_array_with_elements(ps->state, segments);
                    return ZR_NULL;
                }

                if (!parser_ast_array_add(ps, segments, segmentNode)) {
                    ZrParser_Ast_Free(ps->state, segmentNode);
                    free_ast_node_array_with_elements(ps->state, segments);
                    return ZR_NULL;
                }
            }

            index = cursor + 1;
            segmentStart = index;
            continue;
        }

        index++;
    }

    if (!append_template_static_segment(ps, segments, rawText + segmentStart, rawLength - segmentStart)) {
        free_ast_node_array_with_elements(ps->state, segments);
        return ZR_NULL;
    }

    SZrAstNode *node = create_template_string_literal_node(ps, segments);
    if (node == ZR_NULL) {
        free_ast_node_array_with_elements(ps->state, segments);
    }
    return node;
}
