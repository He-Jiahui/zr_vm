//
// Created by Auto on 2025/01/XX.
//

#include "lsp_interface_internal.h"
#include "lsp_metadata_provider.h"
#include "lsp_module_metadata.h"
#include "lsp_project_internal.h"

#include "zr_vm_parser/type_inference.h"

static void get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
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

static TZrBool receiver_type_text_is_specific(const TZrChar *text) {
    return text != ZR_NULL && text[0] != '\0' &&
           strcmp(text, "object") != 0 &&
           strcmp(text, "unknown") != 0;
}

TZrBool ZrLanguageServer_Lsp_StringsEqual(SZrString *left, SZrString *right) {
    TZrNativeString leftText;
    TZrNativeString rightText;
    TZrSize leftLength;
    TZrSize rightLength;

    get_string_view(left, &leftText, &leftLength);
    get_string_view(right, &rightText, &rightLength);

    if (leftText == ZR_NULL || rightText == ZR_NULL) {
        return left == right;
    }

    return leftLength == rightLength && memcmp(leftText, rightText, leftLength) == 0;
}

TZrBool ZrLanguageServer_Lsp_StringContainsCaseInsensitive(SZrString *haystack, SZrString *needle) {
    TZrNativeString haystackText;
    TZrNativeString needleText;
    TZrSize haystackLength;
    TZrSize needleLength;
    TZrSize startIndex;

    get_string_view(haystack, &haystackText, &haystackLength);
    get_string_view(needle, &needleText, &needleLength);

    if (needleText == ZR_NULL || needleLength == 0) {
        return ZR_TRUE;
    }

    if (haystackText == ZR_NULL || haystackLength < needleLength) {
        return ZR_FALSE;
    }

    for (startIndex = 0; startIndex + needleLength <= haystackLength; startIndex++) {
        TZrSize offset;
        TZrBool matched = ZR_TRUE;
        for (offset = 0; offset < needleLength; offset++) {
            TZrChar haystackChar = (TZrChar)tolower((unsigned char)haystackText[startIndex + offset]);
            TZrChar needleChar = (TZrChar)tolower((unsigned char)needleText[offset]);
            if (haystackChar != needleChar) {
                matched = ZR_FALSE;
                break;
            }
        }
        if (matched) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

SZrFileRange ZrLanguageServer_Lsp_GetSymbolLookupRange(SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                         ZrParser_FilePosition_Create(0, 0, 0),
                                         ZR_NULL);
    }

    if (symbol->selectionRange.end.offset > symbol->selectionRange.start.offset ||
        symbol->selectionRange.end.line > symbol->selectionRange.start.line ||
        symbol->selectionRange.end.column > symbol->selectionRange.start.column) {
        return symbol->selectionRange;
    }

    return symbol->location;
}

static const TZrChar *symbol_type_to_display_name(EZrSymbolType type) {
    switch (type) {
        case ZR_SYMBOL_MODULE: return "module";
        case ZR_SYMBOL_CLASS: return "class";
        case ZR_SYMBOL_METHOD: return "method";
        case ZR_SYMBOL_PROPERTY: return "property";
        case ZR_SYMBOL_FIELD: return "field";
        case ZR_SYMBOL_ENUM: return "enum";
        case ZR_SYMBOL_INTERFACE: return "interface";
        case ZR_SYMBOL_FUNCTION: return "function";
        case ZR_SYMBOL_VARIABLE: return "variable";
        case ZR_SYMBOL_PARAMETER: return "parameter";
        case ZR_SYMBOL_ENUM_MEMBER: return "enum member";
        case ZR_SYMBOL_STRUCT: return "struct";
        default: return "symbol";
    }
}

static TZrSize resolve_file_offset(const TZrChar *content,
                                   TZrSize contentLength,
                                   SZrFilePosition position) {
    if (position.offset > 0 && position.offset <= contentLength) {
        return position.offset;
    }

    return ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content,
                                             contentLength,
                                             (position.line > 0 ? position.line - 1 : 0),
                                             (position.column > 0 ? position.column - 1 : 0));
}

static TZrSize find_line_start_offset(const TZrChar *content, TZrSize offset) {
    while (offset > 0 && content[offset - 1] != '\n' && content[offset - 1] != '\r') {
        offset--;
    }

    return offset;
}

static TZrSize trim_line_start_offset(const TZrChar *content, TZrSize start, TZrSize end) {
    while (start < end && isspace((unsigned char)content[start])) {
        start++;
    }

    return start;
}

static TZrSize trim_line_end_offset(const TZrChar *content, TZrSize start, TZrSize end) {
    while (end > start && isspace((unsigned char)content[end - 1])) {
        end--;
    }

    return end;
}

static TZrSize skip_line_breaks_backward(const TZrChar *content, TZrSize offset) {
    while (offset > 0 && (content[offset - 1] == '\n' || content[offset - 1] == '\r')) {
        offset--;
    }

    return offset;
}

static TZrBool line_contains_literal(const TZrChar *content,
                                     TZrSize start,
                                     TZrSize end,
                                     const TZrChar *literal) {
    TZrSize literalLength = literal != ZR_NULL ? strlen(literal) : 0;

    if (content == ZR_NULL || literal == ZR_NULL || literalLength == 0 || end < start ||
        literalLength > end - start) {
        return ZR_FALSE;
    }

    for (TZrSize index = start; index + literalLength <= end; index++) {
        if (memcmp(content + index, literal, literalLength) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void append_buffer_slice(TZrChar *buffer,
                                TZrSize bufferSize,
                                TZrSize *used,
                                const TZrChar *text,
                                TZrSize length) {
    TZrSize available;
    TZrSize writeLength;

    if (buffer == ZR_NULL || used == ZR_NULL || bufferSize == 0 || text == ZR_NULL || length == 0) {
        return;
    }

    if (*used >= bufferSize - 1) {
        buffer[bufferSize - 1] = '\0';
        return;
    }

    available = bufferSize - 1 - *used;
    writeLength = length < available ? length : available;
    memcpy(buffer + *used, text, writeLength);
    *used += writeLength;
    buffer[*used] = '\0';
}

static void append_buffer_text(TZrChar *buffer,
                               TZrSize bufferSize,
                               TZrSize *used,
                               const TZrChar *text) {
    if (text == ZR_NULL) {
        return;
    }

    append_buffer_slice(buffer, bufferSize, used, text, strlen(text));
}

static void append_cleaned_line_comment(const TZrChar *content,
                                        TZrSize lineStart,
                                        TZrSize lineEnd,
                                        TZrChar *buffer,
                                        TZrSize bufferSize,
                                        TZrSize *used) {
    TZrSize start = trim_line_start_offset(content, lineStart, lineEnd);
    TZrSize end = trim_line_end_offset(content, start, lineEnd);

    if (end <= start + 2 || content[start] != '/' || content[start + 1] != '/') {
        return;
    }

    start += 2;
    if (start < end && content[start] == ' ') {
        start++;
    }

    if (*used > 0) {
        append_buffer_text(buffer, bufferSize, used, "\n");
    }
    append_buffer_slice(buffer, bufferSize, used, content + start, end - start);
}

static void append_cleaned_block_comment_line(const TZrChar *content,
                                              TZrSize lineStart,
                                              TZrSize lineEnd,
                                              TZrBool isFirstLine,
                                              TZrBool isLastLine,
                                              TZrChar *buffer,
                                              TZrSize bufferSize,
                                              TZrSize *used) {
    TZrSize start = trim_line_start_offset(content, lineStart, lineEnd);
    TZrSize end = trim_line_end_offset(content, start, lineEnd);

    if (end <= start) {
        return;
    }

    if (isFirstLine) {
        for (TZrSize index = start; index + 1 < end; index++) {
            if (content[index] == '/' && content[index + 1] == '*') {
                start = index + 2;
                while (start < end && isspace((unsigned char)content[start])) {
                    start++;
                }
                if (start < end && content[start] == '*') {
                    start++;
                    if (start < end && content[start] == ' ') {
                        start++;
                    }
                }
                break;
            }
        }
    } else if (start < end && content[start] == '*') {
        start++;
        if (start < end && content[start] == ' ') {
            start++;
        }
    }

    if (isLastLine) {
        for (TZrSize index = start; index + 1 < end; index++) {
            if (content[index] == '*' && content[index + 1] == '/') {
                end = index;
                break;
            }
        }
        end = trim_line_end_offset(content, start, end);
    }

    if (end <= start) {
        return;
    }

    if (*used > 0) {
        append_buffer_text(buffer, bufferSize, used, "\n");
    }
    append_buffer_slice(buffer, bufferSize, used, content + start, end - start);
}

static TZrBool extract_leading_comment_text(const TZrChar *content,
                                            TZrSize contentLength,
                                            SZrFileRange range,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    TZrSize declarationStart;
    TZrSize declarationLineStart;
    TZrSize previousLineEnd;
    TZrSize lineStart;
    TZrSize trimmedStart;
    TZrSize trimmedEnd;
    TZrSize used = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (content == ZR_NULL || contentLength == 0) {
        return ZR_FALSE;
    }

    declarationStart = resolve_file_offset(content, contentLength, range.start);
    if (declarationStart > contentLength) {
        declarationStart = contentLength;
    }
    declarationLineStart = find_line_start_offset(content, declarationStart);
    previousLineEnd = skip_line_breaks_backward(content, declarationLineStart);
    if (previousLineEnd == 0) {
        return ZR_FALSE;
    }

    lineStart = find_line_start_offset(content, previousLineEnd);
    trimmedStart = trim_line_start_offset(content, lineStart, previousLineEnd);
    trimmedEnd = trim_line_end_offset(content, trimmedStart, previousLineEnd);
    if (trimmedStart >= trimmedEnd) {
        return ZR_FALSE;
    }

    if (trimmedEnd - trimmedStart >= 2 &&
        content[trimmedStart] == '/' && content[trimmedStart + 1] == '/') {
        TZrSize lineStarts[ZR_LSP_COMMENT_SCAN_LINE_LIMIT];
        TZrSize lineEnds[ZR_LSP_COMMENT_SCAN_LINE_LIMIT];
        TZrSize count = 0;
        TZrSize cursor = declarationLineStart;

        while (cursor > 0 && count < ZR_LSP_COMMENT_SCAN_LINE_LIMIT) {
            TZrSize rawLineEnd = skip_line_breaks_backward(content, cursor);
            TZrSize rawLineStart = find_line_start_offset(content, rawLineEnd);
            TZrSize rawTrimmedStart = trim_line_start_offset(content, rawLineStart, rawLineEnd);
            TZrSize rawTrimmedEnd = trim_line_end_offset(content, rawTrimmedStart, rawLineEnd);

            if (rawTrimmedStart >= rawTrimmedEnd ||
                rawTrimmedEnd - rawTrimmedStart < 2 ||
                content[rawTrimmedStart] != '/' || content[rawTrimmedStart + 1] != '/') {
                break;
            }

            lineStarts[count] = rawLineStart;
            lineEnds[count] = rawLineEnd;
            count++;
            cursor = rawLineStart;
        }

        for (TZrSize index = count; index > 0; index--) {
            append_cleaned_line_comment(content,
                                        lineStarts[index - 1],
                                        lineEnds[index - 1],
                                        buffer,
                                        bufferSize,
                                        &used);
        }

        return used > 0;
    }

    if (line_contains_literal(content, trimmedStart, trimmedEnd, "*/")) {
        TZrSize lineStarts[ZR_LSP_COMMENT_SCAN_LINE_LIMIT];
        TZrSize lineEnds[ZR_LSP_COMMENT_SCAN_LINE_LIMIT];
        TZrSize count = 0;
        TZrBool foundStart = ZR_FALSE;
        TZrSize cursor = declarationLineStart;

        while (cursor > 0 && count < ZR_LSP_COMMENT_SCAN_LINE_LIMIT) {
            TZrSize rawLineEnd = skip_line_breaks_backward(content, cursor);
            TZrSize rawLineStart = find_line_start_offset(content, rawLineEnd);

            lineStarts[count] = rawLineStart;
            lineEnds[count] = rawLineEnd;
            count++;

            if (line_contains_literal(content, rawLineStart, rawLineEnd, "/*")) {
                foundStart = ZR_TRUE;
                break;
            }

            cursor = rawLineStart;
        }

        if (!foundStart) {
            return ZR_FALSE;
        }

        for (TZrSize index = count; index > 0; index--) {
            append_cleaned_block_comment_line(content,
                                              lineStarts[index - 1],
                                              lineEnds[index - 1],
                                              index == count,
                                              index == 1,
                                              buffer,
                                              bufferSize,
                                              &used);
        }

        return used > 0;
    }

    return ZR_FALSE;
}

SZrString *ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(SZrState *state,
                                                              SZrSymbol *symbol,
                                                              const TZrChar *content,
                                                              TZrSize contentLength) {
    TZrChar commentBuffer[ZR_LSP_COMMENT_BUFFER_LENGTH];

    if (state == ZR_NULL || symbol == ZR_NULL) {
        return ZR_NULL;
    }

    if (!extract_leading_comment_text(content,
                                      contentLength,
                                      symbol->location,
                                      commentBuffer,
                                      sizeof(commentBuffer))) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, commentBuffer, strlen(commentBuffer));
}

SZrString *ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(SZrState *state,
                                                      SZrSymbol *symbol,
                                                      const TZrChar *content,
                                                      TZrSize contentLength) {
    TZrNativeString nameText;
    TZrSize nameLength;
    const TZrChar *kindText;
    TZrChar commentBuffer[ZR_LSP_COMMENT_BUFFER_LENGTH];
    TZrChar markdownBuffer[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrSize used = 0;

    if (state == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL) {
        return ZR_NULL;
    }

    kindText = symbol_type_to_display_name(symbol->type);
    get_string_view(symbol->name, &nameText, &nameLength);
    if (nameText == ZR_NULL || nameLength == 0) {
        return ZR_NULL;
    }

    markdownBuffer[0] = '\0';
    append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, "**");
    append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, kindText);
    append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, "** `");
    append_buffer_slice(markdownBuffer, sizeof(markdownBuffer), &used, nameText, nameLength);
    append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, "`");

    if (symbol->typeInfo != ZR_NULL) {
        const TZrChar *typeText =
            ZrParser_TypeNameString_Get(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer));
        if (typeText != ZR_NULL && typeText[0] != '\0') {
            append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, "\n\nType: ");
            append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, typeText);
        }
    }

    if (extract_leading_comment_text(content,
                                     contentLength,
                                     symbol->location,
                                     commentBuffer,
                                     sizeof(commentBuffer))) {
        append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, "\n\n");
        append_buffer_text(markdownBuffer, sizeof(markdownBuffer), &used, commentBuffer);
    }

    return ZrCore_String_Create(state, markdownBuffer, strlen(markdownBuffer));
}

static SZrString *append_resolved_type_to_detail(SZrState *state,
                                                 SZrString *detail,
                                                 SZrString *resolvedTypeText) {
    TZrNativeString detailText;
    TZrNativeString resolvedText;
    TZrSize detailLength;
    TZrSize resolvedLength;
    const TZrChar *prefix = "Resolved Type: ";
    TZrSize prefixLength = strlen(prefix);
    TZrChar buffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrSize used = 0;

    if (state == ZR_NULL || resolvedTypeText == ZR_NULL) {
        return detail;
    }

    get_string_view(detail, &detailText, &detailLength);
    get_string_view(resolvedTypeText, &resolvedText, &resolvedLength);
    if (resolvedText == ZR_NULL || resolvedLength == 0) {
        return detail;
    }

    if (detailText != ZR_NULL) {
        for (TZrSize index = 0; index + prefixLength + resolvedLength <= detailLength; index++) {
            if (memcmp(detailText + index, prefix, prefixLength) == 0 &&
                memcmp(detailText + index + prefixLength, resolvedText, resolvedLength) == 0) {
                return detail;
            }
        }
    }

    buffer[0] = '\0';
    if (detailText != ZR_NULL && detailLength > 0) {
        append_buffer_slice(buffer, sizeof(buffer), &used, detailText, detailLength);
        append_buffer_text(buffer, sizeof(buffer), &used, "\n");
    }
    append_buffer_text(buffer, sizeof(buffer), &used, prefix);
    append_buffer_slice(buffer, sizeof(buffer), &used, resolvedText, resolvedLength);
    return ZrCore_String_Create(state, buffer, strlen(buffer));
}

void ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrCompletionItem *item,
                                            SZrString *hoveredSymbolName,
                                            SZrString *resolvedTypeText,
                                            const TZrChar *content,
                                            TZrSize contentLength) {
    SZrSymbol *symbol;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || item == ZR_NULL ||
        item->label == ZR_NULL) {
        return;
    }

    symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, item->label, ZR_NULL);
    if (symbol != ZR_NULL && item->detail == ZR_NULL) {
        const TZrChar *kindText = symbol_type_to_display_name(symbol->type);
        item->detail = ZrCore_String_Create(state, (TZrNativeString)kindText, strlen(kindText));
    }

    if (symbol != ZR_NULL && item->documentation == ZR_NULL) {
        item->documentation = ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(state,
                                                                  symbol,
                                                                  content,
                                                                  contentLength);
    }

    if (hoveredSymbolName != ZR_NULL &&
        resolvedTypeText != ZR_NULL &&
        ZrLanguageServer_Lsp_StringsEqual(item->label, hoveredSymbolName)) {
        item->detail = append_resolved_type_to_detail(state, item->detail, resolvedTypeText);
    }
}

static TZrBool should_suppress_parser_diagnostic(SZrDiagnostic *diag) {
    TZrNativeString codeText;
    TZrSize codeLength;
    TZrNativeString messageText;
    TZrSize messageLength;
    static const TZrChar parserSyntaxCode[] = "parser_syntax_error";
    static const TZrChar legacyModuleMessage[] = "Legacy module syntax is not supported; use %module";
    static const TZrChar legacyImportMessage[] = "Legacy import() syntax is not supported; use %import";

    if (diag == ZR_NULL || diag->code == ZR_NULL || diag->message == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_view(diag->code, &codeText, &codeLength);
    if (codeText == ZR_NULL || codeLength != strlen(parserSyntaxCode) ||
        memcmp(codeText, parserSyntaxCode, codeLength) != 0) {
        return ZR_FALSE;
    }

    get_string_view(diag->message, &messageText, &messageLength);
    if (messageText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (messageLength == strlen(legacyModuleMessage) &&
        memcmp(messageText, legacyModuleMessage, messageLength) == 0) {
        return ZR_TRUE;
    }

    return messageLength == strlen(legacyImportMessage) &&
           memcmp(messageText, legacyImportMessage, messageLength) == 0;
}

void ZrLanguageServer_Lsp_AppendDiagnostic(SZrState *state, SZrArray *result, SZrDiagnostic *diag) {
    SZrLspDiagnostic *lspDiag;

    if (state == ZR_NULL || result == ZR_NULL || diag == ZR_NULL || should_suppress_parser_diagnostic(diag)) {
        return;
    }

    lspDiag = (SZrLspDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDiagnostic));
    if (lspDiag == ZR_NULL) {
        return;
    }

    lspDiag->range = ZrLanguageServer_LspRange_FromFileRange(diag->location);
    lspDiag->severity = (TZrInt32)diag->severity + 1;
    lspDiag->code = diag->code;
    lspDiag->message = diag->message;
    ZrCore_Array_Init(state, &lspDiag->relatedInformation, sizeof(SZrLspLocation), 0);
    ZrCore_Array_Push(state, result, &lspDiag);
}

static TZrInt32 symbol_type_to_lsp_kind(EZrSymbolType type) {
    switch (type) {
        case ZR_SYMBOL_MODULE: return ZR_LSP_SYMBOL_KIND_MODULE;
        case ZR_SYMBOL_CLASS: return ZR_LSP_SYMBOL_KIND_CLASS;
        case ZR_SYMBOL_METHOD: return ZR_LSP_SYMBOL_KIND_METHOD;
        case ZR_SYMBOL_PROPERTY: return ZR_LSP_SYMBOL_KIND_PROPERTY;
        case ZR_SYMBOL_FIELD: return ZR_LSP_SYMBOL_KIND_FIELD;
        case ZR_SYMBOL_ENUM: return ZR_LSP_SYMBOL_KIND_ENUM;
        case ZR_SYMBOL_INTERFACE: return ZR_LSP_SYMBOL_KIND_INTERFACE;
        case ZR_SYMBOL_FUNCTION: return ZR_LSP_SYMBOL_KIND_FUNCTION;
        case ZR_SYMBOL_VARIABLE:
        case ZR_SYMBOL_PARAMETER: return ZR_LSP_SYMBOL_KIND_VARIABLE;
        case ZR_SYMBOL_ENUM_MEMBER: return ZR_LSP_SYMBOL_KIND_ENUM_MEMBER;
        case ZR_SYMBOL_STRUCT: return ZR_LSP_SYMBOL_KIND_STRUCT;
        default: return ZR_LSP_SYMBOL_KIND_VARIABLE;
    }
}

SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformation(SZrState *state, SZrSymbol *symbol) {
    SZrLspSymbolInformation *info;
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return ZR_NULL;
    }

    info = (SZrLspSymbolInformation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspSymbolInformation));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    info->name = symbol->name;
    info->kind = symbol_type_to_lsp_kind(symbol->type);
    info->containerName = ZR_NULL;
    info->location.uri = symbol->location.source;
    info->location.range = ZrLanguageServer_LspRange_FromFileRange(symbol->location);
    return info;
}

static SZrString *extract_identifier_name_from_node(SZrAstNode *node) {
    if (node != ZR_NULL && node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    return ZR_NULL;
}

static SZrString *extract_construct_target_type_name_from_node(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name;

        case ZR_AST_TYPE:
            if (node->data.type.name == ZR_NULL) {
                return ZR_NULL;
            }
            if (node->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
                return node->data.type.name->data.identifier.name;
            }
            if (node->data.type.name->type == ZR_AST_GENERIC_TYPE &&
                node->data.type.name->data.genericType.name != ZR_NULL) {
                return node->data.type.name->data.genericType.name->name;
            }
            return ZR_NULL;

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return extract_construct_target_type_name_from_node(node->data.prototypeReferenceExpression.target);

        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;

            if (primary->members != ZR_NULL && primary->members->nodes != ZR_NULL) {
                for (TZrSize index = primary->members->count; index > 0; index--) {
                    SZrAstNode *memberNode = primary->members->nodes[index - 1];
                    if (memberNode != ZR_NULL &&
                        memberNode->type == ZR_AST_MEMBER_EXPRESSION &&
                        memberNode->data.memberExpression.property != ZR_NULL &&
                        memberNode->data.memberExpression.property->type == ZR_AST_IDENTIFIER_LITERAL) {
                        return memberNode->data.memberExpression.property->data.identifier.name;
                    }
                }
            }

            return extract_construct_target_type_name_from_node(primary->property);
        }

        default:
            return ZR_NULL;
    }
}

static SZrString *get_class_property_name(SZrAstNode *memberNode) {
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_CLASS_PROPERTY ||
        memberNode->data.classProperty.modifier == ZR_NULL) {
        return ZR_NULL;
    }

    if (memberNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
        memberNode->data.classProperty.modifier->data.propertyGet.name != ZR_NULL) {
        return memberNode->data.classProperty.modifier->data.propertyGet.name->name;
    }

    if (memberNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
        memberNode->data.classProperty.modifier->data.propertySet.name != ZR_NULL) {
        return memberNode->data.classProperty.modifier->data.propertySet.name->name;
    }

    return ZR_NULL;
}

static TZrBool completion_items_contains_label(SZrArray *items, SZrString *label) {
    TZrSize index;

    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < items->length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual((*itemPtr)->label, label)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void append_completion_item_for_symbol_name(SZrState *state,
                                                   SZrArray *result,
                                                   SZrString *name,
                                                   const TZrChar *kind) {
    TZrNativeString labelText;
    TZrSize labelLength;
    SZrCompletionItem *item;

    if (state == ZR_NULL || result == ZR_NULL || name == ZR_NULL || kind == ZR_NULL ||
        completion_items_contains_label(result, name)) {
        return;
    }

    get_string_view(name, &labelText, &labelLength);
    if (labelText == ZR_NULL) {
        return;
    }

    item = ZrLanguageServer_CompletionItem_New(state, labelText, kind, ZR_NULL, ZR_NULL, ZR_NULL);
    if (item != ZR_NULL) {
        ZrCore_Array_Push(state, result, &item);
    }
}

static void append_completion_item_for_native_name(SZrState *state,
                                                   SZrArray *result,
                                                   const TZrChar *name,
                                                   const TZrChar *kind) {
    SZrString *label;

    if (state == ZR_NULL || result == ZR_NULL || name == ZR_NULL || kind == ZR_NULL) {
        return;
    }

    label = ZrCore_String_Create(state, (TZrNativeString)name, strlen(name));
    if (label != ZR_NULL) {
        append_completion_item_for_symbol_name(state, result, label, kind);
    }
}

static void append_class_member_completions_recursive(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrSymbol *classSymbol,
                                                      TZrBool wantStatic,
                                                      TZrSize depth,
                                                      SZrArray *result) {
    SZrClassDeclaration *classDecl;
    TZrSize memberIndex;

    if (state == ZR_NULL || analyzer == ZR_NULL || classSymbol == ZR_NULL || result == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH || classSymbol->type != ZR_SYMBOL_CLASS ||
        classSymbol->astNode == ZR_NULL ||
        classSymbol->astNode->type != ZR_AST_CLASS_DECLARATION) {
        return;
    }

    classDecl = &classSymbol->astNode->data.classDeclaration;
    if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
        for (memberIndex = 0; memberIndex < classDecl->members->count; memberIndex++) {
            SZrAstNode *memberNode = classDecl->members->nodes[memberIndex];
            if (memberNode == ZR_NULL) {
                continue;
            }

            if (memberNode->type == ZR_AST_CLASS_FIELD &&
                memberNode->data.classField.isStatic == wantStatic &&
                memberNode->data.classField.name != ZR_NULL) {
                append_completion_item_for_symbol_name(state,
                                                       result,
                                                       memberNode->data.classField.name->name,
                                                       "field");
            } else if (memberNode->type == ZR_AST_CLASS_METHOD &&
                       memberNode->data.classMethod.isStatic == wantStatic &&
                       memberNode->data.classMethod.name != ZR_NULL) {
                append_completion_item_for_symbol_name(state,
                                                       result,
                                                       memberNode->data.classMethod.name->name,
                                                       "method");
            } else if (memberNode->type == ZR_AST_CLASS_PROPERTY &&
                       memberNode->data.classProperty.isStatic == wantStatic) {
                append_completion_item_for_symbol_name(state,
                                                       result,
                                                       get_class_property_name(memberNode),
                                                       "property");
            }
        }
    }

    if (classDecl->inherits != ZR_NULL && classDecl->inherits->nodes != ZR_NULL) {
        for (memberIndex = 0; memberIndex < classDecl->inherits->count; memberIndex++) {
            SZrAstNode *inheritNode = classDecl->inherits->nodes[memberIndex];
            if (inheritNode != ZR_NULL && inheritNode->type == ZR_AST_TYPE &&
                inheritNode->data.type.name != ZR_NULL &&
                inheritNode->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
                SZrString *baseName = inheritNode->data.type.name->data.identifier.name;
                SZrSymbol *baseClass = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable,
                                                                           baseName,
                                                                           ZR_NULL);
                if (baseClass != ZR_NULL && baseClass != classSymbol) {
                    append_class_member_completions_recursive(state,
                                                              analyzer,
                                                              baseClass,
                                                              wantStatic,
                                                              depth + 1,
                                                              result);
                }
            }
        }
    }
}

static const SZrTypePrototypeInfo *find_type_prototype_by_text(SZrSemanticAnalyzer *analyzer,
                                                               const TZrChar *typeName) {
    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->compilerState->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
            (const SZrTypePrototypeInfo *)ZrCore_Array_Get(&analyzer->compilerState->typePrototypes, index);
        if (prototype != ZR_NULL &&
            prototype->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(prototype->name), typeName) == 0) {
            return prototype;
        }
    }

    return ZR_NULL;
}

static TZrBool extract_base_type_name(const TZrChar *typeName,
                                      TZrChar *buffer,
                                      TZrSize bufferSize) {
    const TZrChar *start;
    const TZrChar *end;
    const TZrChar *cursor;
    TZrSize length;

    if (typeName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    start = typeName;
    end = typeName + strlen(typeName);
    for (cursor = typeName; *cursor != '\0'; cursor++) {
        if (*cursor == '<' || *cursor == '[') {
            end = cursor;
            break;
        }
    }

    for (cursor = end; cursor > start; cursor--) {
        if (cursor[-1] == '.') {
            start = cursor;
            break;
        }
    }

    length = (TZrSize)(end - start);
    if (length == 0 || length >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';
    return ZR_TRUE;
}

static const ZrLibTypeDescriptor *find_native_type_descriptor_in_module(const ZrLibModuleDescriptor *module,
                                                                        const TZrChar *typeName) {
    TZrChar baseName[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (module == ZR_NULL || typeName == ZR_NULL || !extract_base_type_name(typeName, baseName, sizeof(baseName))) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &module->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, baseName) == 0) {
            return typeDescriptor;
        }
    }

    return ZR_NULL;
}

static const ZrLibTypeDescriptor *find_native_type_descriptor_across_modules(SZrState *state,
                                                                             SZrLspProjectIndex *projectIndex,
                                                                             SZrSemanticAnalyzer *analyzer,
                                                                             SZrAstNode *ast,
                                                                             const TZrChar *typeName,
                                                                             const ZrLibModuleDescriptor **outModule) {
    const ZrLibTypeDescriptor *typeDescriptor;

    typeDescriptor = ZrLanguageServer_LspModuleMetadata_FindNativeTypeDescriptor(state, typeName, outModule);
    if (typeDescriptor != ZR_NULL || state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return typeDescriptor;
    }

    {
        SZrArray bindings;

        ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        ZrLanguageServer_LspProject_CollectImportBindings(state, ast, &bindings);
        for (TZrSize index = 0; index < bindings.length; index++) {
            SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
            SZrLspResolvedImportedModule resolvedModule;

            if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
                continue;
            }

            memset(&resolvedModule, 0, sizeof(resolvedModule));
            if (!ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                          analyzer,
                                                                          projectIndex,
                                                                          (*bindingPtr)->moduleName,
                                                                          &resolvedModule) ||
                resolvedModule.nativeDescriptor == ZR_NULL) {
                continue;
            }

            typeDescriptor = find_native_type_descriptor_in_module(resolvedModule.nativeDescriptor, typeName);
            if (typeDescriptor != ZR_NULL) {
                if (outModule != ZR_NULL) {
                    *outModule = resolvedModule.nativeDescriptor;
                }
                ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
                return typeDescriptor;
            }
        }
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    }

    return ZR_NULL;
}

static void append_native_type_descriptor_member_completions(SZrState *state,
                                                             const ZrLibModuleDescriptor *module,
                                                             const ZrLibTypeDescriptor *typeDescriptor,
                                                             TZrBool wantStatic,
                                                             TZrSize depth,
                                                             SZrArray *result) {
    if (state == ZR_NULL || module == ZR_NULL || typeDescriptor == ZR_NULL || result == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return;
    }

    if (!wantStatic) {
        for (TZrSize index = 0; index < typeDescriptor->fieldCount; index++) {
            const ZrLibFieldDescriptor *field = &typeDescriptor->fields[index];
            if (field->name != ZR_NULL) {
                append_completion_item_for_native_name(state, result, field->name, "field");
            }
        }
    }

    for (TZrSize index = 0; index < typeDescriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *method = &typeDescriptor->methods[index];
        if (method->name == ZR_NULL || method->isStatic != wantStatic) {
            continue;
        }

        append_completion_item_for_native_name(state, result, method->name, "method");
    }

    if (!wantStatic && typeDescriptor->extendsTypeName != ZR_NULL) {
        const ZrLibTypeDescriptor *baseDescriptor =
            find_native_type_descriptor_in_module(module, typeDescriptor->extendsTypeName);
        if (baseDescriptor != ZR_NULL && baseDescriptor != typeDescriptor) {
            append_native_type_descriptor_member_completions(state,
                                                             module,
                                                             baseDescriptor,
                                                             wantStatic,
                                                             depth + 1,
                                                             result);
        }
    }

    if (!wantStatic) {
        for (TZrSize index = 0; index < typeDescriptor->implementsTypeCount; index++) {
            const TZrChar *interfaceName = typeDescriptor->implementsTypeNames[index];
            const ZrLibTypeDescriptor *interfaceDescriptor =
                find_native_type_descriptor_in_module(module, interfaceName);

            if (interfaceDescriptor != ZR_NULL && interfaceDescriptor != typeDescriptor) {
                append_native_type_descriptor_member_completions(state,
                                                                 module,
                                                                 interfaceDescriptor,
                                                                 wantStatic,
                                                                 depth + 1,
                                                                 result);
            }
        }
    }
}

static TZrBool append_receiver_native_type_completions(SZrState *state,
                                                       SZrLspProjectIndex *projectIndex,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *ast,
                                                       const TZrChar *typeName,
                                                       TZrBool wantStatic,
                                                       SZrArray *result) {
    const ZrLibModuleDescriptor *module;
    const ZrLibTypeDescriptor *typeDescriptor;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    typeDescriptor = find_native_type_descriptor_across_modules(state,
                                                                projectIndex,
                                                                analyzer,
                                                                ast,
                                                                typeName,
                                                                &module);
    if (typeDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    append_native_type_descriptor_member_completions(state, module, typeDescriptor, wantStatic, 0, result);
    return result->length > 0;
}

static void find_receiver_variable_prototype_recursive(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node,
                                                       const TZrChar *receiverText,
                                                       TZrSize receiverLength,
                                                       TZrSize cursorOffset,
                                                       const SZrTypePrototypeInfo **bestPrototype,
                                                       TZrChar *bestTypeName,
                                                       TZrSize bestTypeNameSize,
                                                       TZrSize *bestOffset);

static TZrBool copy_type_text_from_symbol(SZrState *state,
                                          SZrSymbol *symbol,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    TZrChar typeBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *typeText;

    if (state == ZR_NULL || symbol == ZR_NULL || symbol->typeInfo == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    typeText = ZrParser_TypeNameString_Get(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer));
    if (typeText == ZR_NULL || typeText[0] == '\0') {
        return ZR_FALSE;
    }

    snprintf(buffer, bufferSize, "%s", typeText);
    return ZR_TRUE;
}

static TZrBool copy_type_text_from_type_env(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrString *receiverName,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    SZrInferredType inferredType;
    const TZrChar *typeText;
    TZrChar typeBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrBool foundType;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || receiverName == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    foundType = ZrParser_TypeEnvironment_LookupVariable(state,
                                                        analyzer->compilerState->typeEnv,
                                                        receiverName,
                                                        &inferredType);
    if (!foundType) {
        ZrParser_InferredType_Free(state, &inferredType);
        return ZR_FALSE;
    }

    typeText = ZrParser_TypeNameString_Get(state, &inferredType, typeBuffer, sizeof(typeBuffer));
    if (typeText != ZR_NULL && typeText[0] != '\0') {
        snprintf(buffer, bufferSize, "%s", typeText);
    }
    ZrParser_InferredType_Free(state, &inferredType);
    return typeText != ZR_NULL && typeText[0] != '\0';
}

static TZrBool resolve_receiver_type_text(SZrState *state,
                                          SZrSemanticAnalyzer *analyzer,
                                          SZrAstNode *ast,
                                          SZrString *receiverName,
                                          const TZrChar *receiverText,
                                          TZrSize receiverLength,
                                          TZrSize cursorOffset,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    SZrSymbol *receiverSymbol;
    const SZrTypePrototypeInfo *receiverPrototype = ZR_NULL;
    TZrSize bestOffset = 0;

    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL || receiverName == ZR_NULL ||
        receiverText == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    receiverSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, receiverName, ZR_NULL);
    if (copy_type_text_from_symbol(state, receiverSymbol, buffer, bufferSize) ||
        copy_type_text_from_type_env(state, analyzer, receiverName, buffer, bufferSize)) {
        if (receiver_type_text_is_specific(buffer)) {
            return ZR_TRUE;
        }
        buffer[0] = '\0';
    }

    find_receiver_variable_prototype_recursive(state,
                                               analyzer,
                                               ast,
                                               receiverText,
                                               receiverLength,
                                               cursorOffset,
                                               &receiverPrototype,
                                               buffer,
                                               bufferSize,
                                               &bestOffset);
    if (buffer[0] != '\0') {
        return ZR_TRUE;
    }

    if (receiverPrototype != ZR_NULL && receiverPrototype->name != ZR_NULL) {
        snprintf(buffer, bufferSize, "%s", ZrCore_String_GetNativeString(receiverPrototype->name));
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool type_text_parse_generic_arguments(const TZrChar *typeText,
                                                 TZrChar arguments[ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX]
                                                                  [ZR_LSP_NATIVE_GENERIC_TEXT_MAX],
                                                 TZrSize *argumentCount) {
    const TZrChar *genericStart;
    const TZrChar *cursor;
    const TZrChar *segmentStart;
    TZrSize count = 0;
    TZrInt32 depth = 0;

    if (arguments == ZR_NULL || argumentCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *argumentCount = 0;
    if (typeText == ZR_NULL) {
        return ZR_FALSE;
    }

    genericStart = strchr(typeText, '<');
    if (genericStart == ZR_NULL) {
        return ZR_TRUE;
    }

    segmentStart = genericStart + 1;
    for (cursor = genericStart + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            depth++;
            continue;
        }

        if (*cursor == '>') {
            if (depth == 0) {
                const TZrChar *trimStart = segmentStart;
                const TZrChar *trimEnd = cursor;

                while (trimStart < trimEnd && isspace((unsigned char)*trimStart)) {
                    trimStart++;
                }
                while (trimEnd > trimStart && isspace((unsigned char)trimEnd[-1])) {
                    trimEnd--;
                }

                if (trimEnd > trimStart && count < ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX) {
                    TZrSize length = (TZrSize)(trimEnd - trimStart);
                    if (length >= ZR_LSP_NATIVE_GENERIC_TEXT_MAX) {
                        length = ZR_LSP_NATIVE_GENERIC_TEXT_MAX - 1;
                    }
                    memcpy(arguments[count], trimStart, length);
                    arguments[count][length] = '\0';
                    count++;
                }
                *argumentCount = count;
                return ZR_TRUE;
            }

            depth--;
            continue;
        }

        if (*cursor == ',' && depth == 0) {
            const TZrChar *trimStart = segmentStart;
            const TZrChar *trimEnd = cursor;

            while (trimStart < trimEnd && isspace((unsigned char)*trimStart)) {
                trimStart++;
            }
            while (trimEnd > trimStart && isspace((unsigned char)trimEnd[-1])) {
                trimEnd--;
            }

            if (trimEnd > trimStart && count < ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX) {
                TZrSize length = (TZrSize)(trimEnd - trimStart);
                if (length >= ZR_LSP_NATIVE_GENERIC_TEXT_MAX) {
                    length = ZR_LSP_NATIVE_GENERIC_TEXT_MAX - 1;
                }
                memcpy(arguments[count], trimStart, length);
                arguments[count][length] = '\0';
                count++;
            }
            segmentStart = cursor + 1;
        }
    }

    return ZR_FALSE;
}

static TZrBool native_hover_is_identifier_char(TZrChar ch) {
    return (TZrBool)(isalnum((unsigned char)ch) || ch == '_');
}

static void specialize_native_type_text(const TZrChar *templateText,
                                        const ZrLibGenericParameterDescriptor *genericParameters,
                                        TZrSize genericParameterCount,
                                        TZrChar arguments[ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX]
                                                         [ZR_LSP_NATIVE_GENERIC_TEXT_MAX],
                                        TZrSize argumentCount,
                                        TZrChar *buffer,
                                        TZrSize bufferSize) {
    TZrSize used = 0;
    const TZrChar *cursor;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (templateText == ZR_NULL) {
        return;
    }

    if (genericParameterCount == 0 || argumentCount == 0 || genericParameterCount != argumentCount) {
        append_buffer_text(buffer, bufferSize, &used, templateText);
        return;
    }

    cursor = templateText;
    while (*cursor != '\0') {
        TZrBool replaced = ZR_FALSE;

        for (TZrSize index = 0; index < genericParameterCount; index++) {
            const TZrChar *parameterName = genericParameters[index].name;
            TZrSize parameterLength;
            TZrChar previousChar;
            TZrChar nextChar;

            if (parameterName == ZR_NULL) {
                continue;
            }

            parameterLength = strlen(parameterName);
            if (parameterLength == 0 || strncmp(cursor, parameterName, parameterLength) != 0) {
                continue;
            }

            previousChar = cursor == templateText ? '\0' : cursor[-1];
            nextChar = cursor[parameterLength];
            if ((previousChar != '\0' && native_hover_is_identifier_char(previousChar)) ||
                (nextChar != '\0' && native_hover_is_identifier_char(nextChar))) {
                continue;
            }

            append_buffer_text(buffer, bufferSize, &used, arguments[index]);
            cursor += parameterLength;
            replaced = ZR_TRUE;
            break;
        }

        if (!replaced) {
            append_buffer_slice(buffer, bufferSize, &used, cursor, 1);
            cursor++;
        }
    }
}

static const ZrLibFieldDescriptor *find_native_field_descriptor_recursive(const ZrLibModuleDescriptor *module,
                                                                          const ZrLibTypeDescriptor *typeDescriptor,
                                                                          const TZrChar *memberName,
                                                                          TZrSize depth) {
    if (module == ZR_NULL || typeDescriptor == ZR_NULL || memberName == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < typeDescriptor->fieldCount; index++) {
        const ZrLibFieldDescriptor *field = &typeDescriptor->fields[index];
        if (field->name != ZR_NULL && strcmp(field->name, memberName) == 0) {
            return field;
        }
    }

    if (typeDescriptor->extendsTypeName != ZR_NULL) {
        const ZrLibTypeDescriptor *baseDescriptor =
            find_native_type_descriptor_in_module(module, typeDescriptor->extendsTypeName);
        const ZrLibFieldDescriptor *field =
            find_native_field_descriptor_recursive(module, baseDescriptor, memberName, depth + 1);
        if (field != ZR_NULL) {
            return field;
        }
    }

    for (TZrSize index = 0; index < typeDescriptor->implementsTypeCount; index++) {
        const ZrLibTypeDescriptor *interfaceDescriptor =
            find_native_type_descriptor_in_module(module, typeDescriptor->implementsTypeNames[index]);
        const ZrLibFieldDescriptor *field =
            find_native_field_descriptor_recursive(module, interfaceDescriptor, memberName, depth + 1);
        if (field != ZR_NULL) {
            return field;
        }
    }

    return ZR_NULL;
}

static const ZrLibMethodDescriptor *find_native_method_descriptor_recursive(const ZrLibModuleDescriptor *module,
                                                                           const ZrLibTypeDescriptor *typeDescriptor,
                                                                           const TZrChar *memberName,
                                                                           TZrSize depth) {
    if (module == ZR_NULL || typeDescriptor == ZR_NULL || memberName == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < typeDescriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *method = &typeDescriptor->methods[index];
        if (method->name != ZR_NULL && strcmp(method->name, memberName) == 0) {
            return method;
        }
    }

    if (typeDescriptor->extendsTypeName != ZR_NULL) {
        const ZrLibTypeDescriptor *baseDescriptor =
            find_native_type_descriptor_in_module(module, typeDescriptor->extendsTypeName);
        const ZrLibMethodDescriptor *method =
            find_native_method_descriptor_recursive(module, baseDescriptor, memberName, depth + 1);
        if (method != ZR_NULL) {
            return method;
        }
    }

    for (TZrSize index = 0; index < typeDescriptor->implementsTypeCount; index++) {
        const ZrLibTypeDescriptor *interfaceDescriptor =
            find_native_type_descriptor_in_module(module, typeDescriptor->implementsTypeNames[index]);
        const ZrLibMethodDescriptor *method =
            find_native_method_descriptor_recursive(module, interfaceDescriptor, memberName, depth + 1);
        if (method != ZR_NULL) {
            return method;
        }
    }

    return ZR_NULL;
}

static TZrBool receiver_range_contains_offset(SZrFileRange range, TZrSize offset) {
    if (range.start.offset > 0 && range.end.offset > 0) {
        return range.start.offset <= offset && offset <= range.end.offset;
    }

    return ZR_FALSE;
}

static TZrBool receiver_build_primary_prefix(SZrAstNode *primaryNode,
                                             TZrSize prefixMemberCount,
                                             SZrAstNode *tempNode,
                                             SZrPrimaryExpression *tempPrimary,
                                             SZrAstNodeArray *tempMembers) {
    SZrPrimaryExpression *originalPrimary;

    if (primaryNode == ZR_NULL ||
        primaryNode->type != ZR_AST_PRIMARY_EXPRESSION ||
        tempNode == ZR_NULL ||
        tempPrimary == ZR_NULL ||
        tempMembers == ZR_NULL) {
        return ZR_FALSE;
    }

    originalPrimary = &primaryNode->data.primaryExpression;
    memset(tempNode, 0, sizeof(*tempNode));
    memset(tempPrimary, 0, sizeof(*tempPrimary));
    memset(tempMembers, 0, sizeof(*tempMembers));

    tempNode->type = ZR_AST_PRIMARY_EXPRESSION;
    tempNode->location = primaryNode->location;
    tempNode->data.primaryExpression = *tempPrimary;
    tempNode->data.primaryExpression.property = originalPrimary->property;

    if (prefixMemberCount > 0 && originalPrimary->members != ZR_NULL) {
        tempMembers->nodes = originalPrimary->members->nodes;
        tempMembers->count = prefixMemberCount;
        tempMembers->capacity = prefixMemberCount;
        tempNode->data.primaryExpression.members = tempMembers;
    } else {
        tempNode->data.primaryExpression.members = ZR_NULL;
    }

    return ZR_TRUE;
}

static TZrBool find_receiver_member_context_recursive(SZrAstNode *node,
                                                      TZrSize cursorOffset,
                                                      SZrAstNode **outPrimaryNode,
                                                      TZrSize *outMemberIndex,
                                                      SZrString **outMemberName) {
    if (node == ZR_NULL || outPrimaryNode == ZR_NULL || outMemberIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (find_receiver_member_context_recursive(node->data.script.statements->nodes[index],
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (find_receiver_member_context_recursive(node->data.block.body->nodes[index],
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_FUNCTION_DECLARATION:
            return find_receiver_member_context_recursive(node->data.functionDeclaration.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_TEST_DECLARATION:
            return find_receiver_member_context_recursive(node->data.testDeclaration.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return find_receiver_member_context_recursive(node->data.compileTimeDeclaration.declaration,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.members != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    if (find_receiver_member_context_recursive(node->data.classDeclaration.members->nodes[index],
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_STRUCT_DECLARATION:
            if (node->data.structDeclaration.members != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.structDeclaration.members->count; index++) {
                    if (find_receiver_member_context_recursive(node->data.structDeclaration.members->nodes[index],
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_CLASS_METHOD:
            return find_receiver_member_context_recursive(node->data.classMethod.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_CLASS_META_FUNCTION:
            return find_receiver_member_context_recursive(node->data.classMetaFunction.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_STRUCT_METHOD:
            return find_receiver_member_context_recursive(node->data.structMethod.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_STRUCT_META_FUNCTION:
            return find_receiver_member_context_recursive(node->data.structMetaFunction.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_CLASS_PROPERTY:
            return find_receiver_member_context_recursive(node->data.classProperty.modifier,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_PROPERTY_GET:
            return find_receiver_member_context_recursive(node->data.propertyGet.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_PROPERTY_SET:
            return find_receiver_member_context_recursive(node->data.propertySet.body,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_VARIABLE_DECLARATION:
            return find_receiver_member_context_recursive(node->data.variableDeclaration.value,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_RETURN_STATEMENT:
            return find_receiver_member_context_recursive(node->data.returnStatement.expr,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_EXPRESSION_STATEMENT:
            return find_receiver_member_context_recursive(node->data.expressionStatement.expr,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return find_receiver_member_context_recursive(node->data.assignmentExpression.left,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName) ||
                   find_receiver_member_context_recursive(node->data.assignmentExpression.right,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_BINARY_EXPRESSION:
            return find_receiver_member_context_recursive(node->data.binaryExpression.left,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName) ||
                   find_receiver_member_context_recursive(node->data.binaryExpression.right,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_UNARY_EXPRESSION:
            return find_receiver_member_context_recursive(node->data.unaryExpression.argument,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.functionCall.args->count; index++) {
                    if (find_receiver_member_context_recursive(node->data.functionCall.args->nodes[index],
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (find_receiver_member_context_recursive(node->data.constructExpression.target,
                                                       cursorOffset,
                                                       outPrimaryNode,
                                                       outMemberIndex,
                                                       outMemberName)) {
                return ZR_TRUE;
            }
            if (node->data.constructExpression.args != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.constructExpression.args->count; index++) {
                    if (find_receiver_member_context_recursive(node->data.constructExpression.args->nodes[index],
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_LAMBDA_EXPRESSION:
            return find_receiver_member_context_recursive(node->data.lambdaExpression.block,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        case ZR_AST_PRIMARY_EXPRESSION:
            if (node->data.primaryExpression.property != ZR_NULL &&
                find_receiver_member_context_recursive(node->data.primaryExpression.property,
                                                       cursorOffset,
                                                       outPrimaryNode,
                                                       outMemberIndex,
                                                       outMemberName)) {
                return ZR_TRUE;
            }

            if (node->data.primaryExpression.members != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
                    SZrAstNode *memberNode = node->data.primaryExpression.members->nodes[index];

                    if (memberNode != ZR_NULL &&
                        memberNode->type == ZR_AST_MEMBER_EXPRESSION &&
                        memberNode->data.memberExpression.property != ZR_NULL &&
                        memberNode->data.memberExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
                        receiver_range_contains_offset(memberNode->data.memberExpression.property->location,
                                                      cursorOffset)) {
                        *outPrimaryNode = node;
                        *outMemberIndex = index;
                        if (outMemberName != ZR_NULL) {
                            *outMemberName = memberNode->data.memberExpression.property->data.identifier.name;
                        }
                        return ZR_TRUE;
                    }

                    if (find_receiver_member_context_recursive(memberNode,
                                                               cursorOffset,
                                                               outPrimaryNode,
                                                               outMemberIndex,
                                                               outMemberName)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_MEMBER_EXPRESSION:
            return find_receiver_member_context_recursive(node->data.memberExpression.property,
                                                          cursorOffset,
                                                          outPrimaryNode,
                                                          outMemberIndex,
                                                          outMemberName);

        default:
            return ZR_FALSE;
    }
}

static TZrBool try_infer_receiver_type_text_from_ast(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *ast,
                                                     TZrSize cursorOffset,
                                                     TZrChar *buffer,
                                                     TZrSize bufferSize) {
    SZrAstNode *primaryNode = ZR_NULL;
    TZrSize memberIndex = 0;
    SZrAstNode *tempNode;
    SZrPrimaryExpression tempPrimary;
    SZrAstNodeArray tempMembers;
    SZrInferredType receiverType;
    const TZrChar *typeText;
    TZrChar typeBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (!find_receiver_member_context_recursive(ast, cursorOffset, &primaryNode, &memberIndex, ZR_NULL)) {
        return ZR_FALSE;
    }

    tempNode = (SZrAstNode *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAstNode));
    if (tempNode == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    memset(&tempPrimary, 0, sizeof(tempPrimary));
    memset(&tempMembers, 0, sizeof(tempMembers));
    if (!receiver_build_primary_prefix(primaryNode, memberIndex, tempNode, &tempPrimary, &tempMembers) ||
        analyzer->compilerState == ZR_NULL ||
        !ZrParser_ExpressionType_Infer(analyzer->compilerState, tempNode, &receiverType)) {
        ZrParser_InferredType_Free(state, &receiverType);
        ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
        return ZR_FALSE;
    }

    typeText = ZrParser_TypeNameString_Get(state, &receiverType, typeBuffer, sizeof(typeBuffer));
    if (typeText != ZR_NULL && typeText[0] != '\0') {
        snprintf(buffer, bufferSize, "%s", typeText);
    }

    ZrParser_InferredType_Free(state, &receiverType);
    ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
    return buffer[0] != '\0';
}

TZrBool ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(SZrState *state,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNode *ast,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            TZrSize cursorOffset,
                                                            SZrLspResolvedMetadataMember *outResolved) {
    TZrSize memberStart;
    TZrSize memberEnd;
    TZrSize receiverStart;
    TZrSize receiverEnd;
    TZrChar memberName[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar receiverNameText[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar receiverTypeText[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar genericArguments[ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX][ZR_LSP_NATIVE_GENERIC_TEXT_MAX];
    TZrSize genericArgumentCount = 0;
    const ZrLibModuleDescriptor *module;
    const ZrLibTypeDescriptor *typeDescriptor;
    const ZrLibFieldDescriptor *fieldDescriptor;
    const ZrLibMethodDescriptor *methodDescriptor;
    SZrString *receiverName;
    TZrChar specializedType[ZR_LSP_TEXT_BUFFER_LENGTH];
    EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL || content == ZR_NULL ||
        cursorOffset >= contentLength || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outResolved, 0, sizeof(*outResolved));

    memberStart = cursorOffset;
    while (memberStart > 0 && native_hover_is_identifier_char(content[memberStart - 1])) {
        memberStart--;
    }
    memberEnd = cursorOffset;
    while (memberEnd < contentLength && native_hover_is_identifier_char(content[memberEnd])) {
        memberEnd++;
    }
    if (memberEnd <= memberStart || memberStart == 0 || content[memberStart - 1] != '.') {
        return ZR_FALSE;
    }

    receiverEnd = memberStart - 1;
    receiverStart = receiverEnd;
    while (receiverStart > 0 && native_hover_is_identifier_char(content[receiverStart - 1])) {
        receiverStart--;
    }
    if (memberEnd - memberStart >= sizeof(memberName)) {
        return ZR_FALSE;
    }

    memcpy(memberName, content + memberStart, memberEnd - memberStart);
    memberName[memberEnd - memberStart] = '\0';

    receiverTypeText[0] = '\0';
    if (!try_infer_receiver_type_text_from_ast(state,
                                               analyzer,
                                               ast,
                                               cursorOffset,
                                               receiverTypeText,
                                               sizeof(receiverTypeText)) &&
        (receiverEnd <= receiverStart ||
         receiverEnd - receiverStart >= sizeof(receiverNameText))) {
        return ZR_FALSE;
    }
    if (!receiver_type_text_is_specific(receiverTypeText)) {
        receiverTypeText[0] = '\0';
    }

    if (receiverTypeText[0] == '\0') {
        memcpy(receiverNameText, content + receiverStart, receiverEnd - receiverStart);
        receiverNameText[receiverEnd - receiverStart] = '\0';

        receiverName = ZrCore_String_Create(state,
                                            (TZrNativeString)receiverNameText,
                                            strlen(receiverNameText));
        if (receiverName == ZR_NULL ||
            !resolve_receiver_type_text(state,
                                        analyzer,
                                        ast,
                                        receiverName,
                                        receiverNameText,
                                        strlen(receiverNameText),
                                        cursorOffset,
                                        receiverTypeText,
                                        sizeof(receiverTypeText))) {
            return ZR_FALSE;
        }
    }

    typeDescriptor = find_native_type_descriptor_across_modules(state,
                                                                projectIndex,
                                                                analyzer,
                                                                ast,
                                                                receiverTypeText,
                                                                &module);
    if (typeDescriptor == ZR_NULL) {
        return ZR_FALSE;
    }

    type_text_parse_generic_arguments(receiverTypeText, genericArguments, &genericArgumentCount);
    fieldDescriptor = find_native_field_descriptor_recursive(module, typeDescriptor, memberName, 0);
    methodDescriptor = find_native_method_descriptor_recursive(module, typeDescriptor, memberName, 0);

    if (module != ZR_NULL && module->moduleName != ZR_NULL) {
        outResolved->module.moduleName =
            ZrCore_String_Create(state, (TZrNativeString)module->moduleName, strlen(module->moduleName));
        ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state, module->moduleName, &sourceKind);
        outResolved->module.nativeDescriptor = module;
    }
    outResolved->module.sourceKind = sourceKind;
    outResolved->memberName = ZrCore_String_Create(state, (TZrNativeString)memberName, strlen(memberName));
    outResolved->ownerTypeName =
        ZrCore_String_Create(state, (TZrNativeString)receiverTypeText, strlen(receiverTypeText));
    outResolved->ownerTypeDescriptor = typeDescriptor;
    outResolved->typeDescriptor = typeDescriptor;

    if (methodDescriptor != ZR_NULL) {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_METHOD;
        outResolved->methodDescriptor = methodDescriptor;
        specialize_native_type_text(methodDescriptor->returnTypeName,
                                    typeDescriptor->genericParameters,
                                    typeDescriptor->genericParameterCount,
                                    genericArguments,
                                    genericArgumentCount,
                                    specializedType,
                                    sizeof(specializedType));
        if (specializedType[0] != '\0') {
            outResolved->resolvedTypeText =
                ZrCore_String_Create(state, (TZrNativeString)specializedType, strlen(specializedType));
        }
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, ZR_NULL);
        ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(&provider, projectIndex, outResolved);
        return outResolved->memberName != ZR_NULL;
    }

    if (fieldDescriptor != ZR_NULL) {
        outResolved->memberKind = ZR_LSP_METADATA_MEMBER_FIELD;
        outResolved->fieldDescriptor = fieldDescriptor;
        specialize_native_type_text(fieldDescriptor->typeName,
                                    typeDescriptor->genericParameters,
                                    typeDescriptor->genericParameterCount,
                                    genericArguments,
                                    genericArgumentCount,
                                    specializedType,
                                    sizeof(specializedType));
        if (specializedType[0] != '\0') {
            outResolved->resolvedTypeText =
                ZrCore_String_Create(state, (TZrNativeString)specializedType, strlen(specializedType));
        }
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, ZR_NULL);
        ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(&provider, projectIndex, outResolved);
        return outResolved->memberName != ZR_NULL;
    }

    return ZR_FALSE;
}

static void append_type_prototype_member_completions(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     const SZrTypePrototypeInfo *prototype,
                                                     TZrBool wantStatic,
                                                     TZrSize depth,
                                                     SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || prototype == ZR_NULL || result == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&prototype->members, index);
        const TZrChar *kind = ZR_NULL;

        if (member == ZR_NULL || member->name == ZR_NULL || member->isMetaMethod || member->isStatic != wantStatic) {
            continue;
        }

        switch (member->memberType) {
            case ZR_AST_CLASS_FIELD:
            case ZR_AST_STRUCT_FIELD:
                kind = "field";
                break;
            case ZR_AST_CLASS_PROPERTY:
                kind = "property";
                break;
            case ZR_AST_CLASS_METHOD:
            case ZR_AST_STRUCT_METHOD:
                kind = "method";
                break;
            default:
                break;
        }

        if (kind != ZR_NULL) {
            append_completion_item_for_symbol_name(state, result, member->name, kind);
        }
    }

    if (prototype->extendsTypeName != ZR_NULL) {
        const SZrTypePrototypeInfo *basePrototype =
            find_type_prototype_by_text(analyzer, ZrCore_String_GetNativeString(prototype->extendsTypeName));
        if (basePrototype != ZR_NULL && basePrototype != prototype) {
            append_type_prototype_member_completions(state,
                                                     analyzer,
                                                     basePrototype,
                                                     wantStatic,
                                                     depth + 1,
                                                     result);
        }
    }

    for (TZrSize index = 0; index < prototype->inherits.length; index++) {
        SZrString **inheritPtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&prototype->inherits, index);
        const SZrTypePrototypeInfo *basePrototype;

        if (inheritPtr == ZR_NULL || *inheritPtr == ZR_NULL) {
            continue;
        }

        basePrototype = find_type_prototype_by_text(analyzer, ZrCore_String_GetNativeString(*inheritPtr));
        if (basePrototype != ZR_NULL && basePrototype != prototype) {
            append_type_prototype_member_completions(state,
                                                     analyzer,
                                                     basePrototype,
                                                     wantStatic,
                                                     depth + 1,
                                                     result);
        }
    }
}

static TZrBool append_receiver_symbol_type_completions(SZrState *state,
                                                       SZrLspProjectIndex *projectIndex,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *ast,
                                                       SZrSymbol *receiverSymbol,
                                                       TZrBool wantStatic,
                                                       SZrArray *result) {
    TZrChar typeBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *typeText;
    const SZrTypePrototypeInfo *prototype;

    if (state == ZR_NULL || analyzer == ZR_NULL || receiverSymbol == ZR_NULL ||
        receiverSymbol->typeInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    typeText = ZrParser_TypeNameString_Get(state, receiverSymbol->typeInfo, typeBuffer, sizeof(typeBuffer));
    if (typeText == ZR_NULL || typeText[0] == '\0') {
        return ZR_FALSE;
    }

    prototype = find_type_prototype_by_text(analyzer, typeText);
    if (prototype != ZR_NULL) {
        append_type_prototype_member_completions(state, analyzer, prototype, wantStatic, 0, result);
        return result->length > 0;
    }

    return append_receiver_native_type_completions(state,
                                                   projectIndex,
                                                   analyzer,
                                                   ast,
                                                   typeText,
                                                   wantStatic,
                                                   result);
}

static TZrBool append_receiver_name_type_env_completions(SZrState *state,
                                                         SZrLspProjectIndex *projectIndex,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNode *ast,
                                                         SZrString *receiverName,
                                                         TZrBool wantStatic,
                                                         SZrArray *result) {
    SZrInferredType inferredType;
    TZrChar typeBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *typeText;
    const SZrTypePrototypeInfo *prototype;
    TZrBool foundType;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || receiverName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    foundType = ZrParser_TypeEnvironment_LookupVariable(state,
                                                        analyzer->compilerState->typeEnv,
                                                        receiverName,
                                                        &inferredType);
    if (!foundType) {
        ZrParser_InferredType_Free(state, &inferredType);
        return ZR_FALSE;
    }

    typeText = ZrParser_TypeNameString_Get(state, &inferredType, typeBuffer, sizeof(typeBuffer));
    if (typeText == ZR_NULL || typeText[0] == '\0') {
        ZrParser_InferredType_Free(state, &inferredType);
        return ZR_FALSE;
    }

    prototype = find_type_prototype_by_text(analyzer, typeText);
    if (prototype != ZR_NULL) {
        append_type_prototype_member_completions(state, analyzer, prototype, wantStatic, 0, result);
        ZrParser_InferredType_Free(state, &inferredType);
        return result->length > 0;
    }

    append_receiver_native_type_completions(state,
                                            projectIndex,
                                            analyzer,
                                            ast,
                                            typeText,
                                            wantStatic,
                                            result);
    ZrParser_InferredType_Free(state, &inferredType);
    return result->length > 0;
}

static void find_receiver_variable_prototype_recursive(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node,
                                                       const TZrChar *receiverText,
                                                       TZrSize receiverLength,
                                                       TZrSize cursorOffset,
                                                       const SZrTypePrototypeInfo **bestPrototype,
                                                       TZrChar *bestTypeName,
                                                       TZrSize bestTypeNameSize,
                                                       TZrSize *bestOffset) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || receiverText == ZR_NULL ||
        bestPrototype == ZR_NULL || bestOffset == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < script->statements->count; index++) {
                    find_receiver_variable_prototype_recursive(state,
                                                               analyzer,
                                                               script->statements->nodes[index],
                                                               receiverText,
                                                               receiverLength,
                                                               cursorOffset,
                                                               bestPrototype,
                                                               bestTypeName,
                                                               bestTypeNameSize,
                                                               bestOffset);
                }
            }
            break;
        }

        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < block->body->count; index++) {
                    find_receiver_variable_prototype_recursive(state,
                                                               analyzer,
                                                               block->body->nodes[index],
                                                               receiverText,
                                                               receiverLength,
                                                               cursorOffset,
                                                               bestPrototype,
                                                               bestTypeName,
                                                               bestTypeNameSize,
                                                               bestOffset);
                }
            }
            break;
        }

        case ZR_AST_FUNCTION_DECLARATION:
            if (node->data.functionDeclaration.body != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.functionDeclaration.body,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            break;

        case ZR_AST_TEST_DECLARATION:
            if (node->data.testDeclaration.body != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.testDeclaration.body,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            break;

        case ZR_AST_EXPRESSION_STATEMENT:
            if (node->data.expressionStatement.expr != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.expressionStatement.expr,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            break;

        case ZR_AST_USING_STATEMENT:
            if (node->data.usingStatement.resource != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.usingStatement.resource,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            if (node->data.usingStatement.body != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.usingStatement.body,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            break;

        case ZR_AST_RETURN_STATEMENT:
            if (node->data.returnStatement.expr != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.returnStatement.expr,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            break;

        case ZR_AST_LAMBDA_EXPRESSION:
            if (node->data.lambdaExpression.block != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.lambdaExpression.block,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            if (node->data.primaryExpression.property != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.primaryExpression.property,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            if (node->data.primaryExpression.members != ZR_NULL &&
                node->data.primaryExpression.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
                    find_receiver_variable_prototype_recursive(state,
                                                               analyzer,
                                                               node->data.primaryExpression.members->nodes[index],
                                                               receiverText,
                                                               receiverLength,
                                                               cursorOffset,
                                                               bestPrototype,
                                                               bestTypeName,
                                                               bestTypeNameSize,
                                                               bestOffset);
                }
            }
            break;

        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL && node->data.functionCall.args->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.functionCall.args->count; index++) {
                    find_receiver_variable_prototype_recursive(state,
                                                               analyzer,
                                                               node->data.functionCall.args->nodes[index],
                                                               receiverText,
                                                               receiverLength,
                                                               cursorOffset,
                                                               bestPrototype,
                                                               bestTypeName,
                                                               bestTypeNameSize,
                                                               bestOffset);
                }
            }
            break;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (node->data.constructExpression.target != ZR_NULL) {
                find_receiver_variable_prototype_recursive(state,
                                                           analyzer,
                                                           node->data.constructExpression.target,
                                                           receiverText,
                                                           receiverLength,
                                                           cursorOffset,
                                                           bestPrototype,
                                                           bestTypeName,
                                                           bestTypeNameSize,
                                                           bestOffset);
            }
            if (node->data.constructExpression.args != ZR_NULL &&
                node->data.constructExpression.args->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.constructExpression.args->count; index++) {
                    find_receiver_variable_prototype_recursive(state,
                                                               analyzer,
                                                               node->data.constructExpression.args->nodes[index],
                                                               receiverText,
                                                               receiverLength,
                                                               cursorOffset,
                                                               bestPrototype,
                                                               bestTypeName,
                                                               bestTypeNameSize,
                                                               bestOffset);
                }
            }
            break;

        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrString *patternName = extract_identifier_name_from_node(varDecl->pattern);
            SZrString *constructTargetTypeName = ZR_NULL;
            TZrNativeString patternText;
            TZrSize patternLength;
            SZrInferredType inferredType;
            const TZrChar *typeText = ZR_NULL;
            TZrChar typeBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
            const SZrTypePrototypeInfo *prototype;
            TZrBool hasType = ZR_FALSE;

            if (node->location.start.offset > cursorOffset || patternName == ZR_NULL) {
                break;
            }

            get_string_view(patternName, &patternText, &patternLength);
            if (patternText == ZR_NULL || patternLength != receiverLength ||
                memcmp(patternText, receiverText, receiverLength) != 0) {
                break;
            }

            ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
            if (varDecl->typeInfo != ZR_NULL) {
                hasType = ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState,
                                                                 varDecl->typeInfo,
                                                                 &inferredType);
            } else if (varDecl->value != ZR_NULL) {
                hasType = ZrParser_ExpressionType_Infer(analyzer->compilerState,
                                                        varDecl->value,
                                                        &inferredType);
            }

            if (!hasType) {
                if (varDecl->value != ZR_NULL && varDecl->value->type == ZR_AST_CONSTRUCT_EXPRESSION) {
                    constructTargetTypeName =
                        extract_construct_target_type_name_from_node(varDecl->value->data.constructExpression.target);
                    if (constructTargetTypeName != ZR_NULL) {
                        typeText = ZrCore_String_GetNativeString(constructTargetTypeName);
                    }
                }
                if (typeText == ZR_NULL || typeText[0] == '\0') {
                    ZrParser_InferredType_Free(state, &inferredType);
                    break;
                }
            } else {
                typeText = ZrParser_TypeNameString_Get(state, &inferredType, typeBuffer, sizeof(typeBuffer));
            }

            if ((typeText == ZR_NULL || typeText[0] == '\0' ||
                 find_type_prototype_by_text(analyzer, typeText) == ZR_NULL) &&
                varDecl->value != ZR_NULL && varDecl->value->type == ZR_AST_CONSTRUCT_EXPRESSION) {
                constructTargetTypeName =
                    extract_construct_target_type_name_from_node(varDecl->value->data.constructExpression.target);
                if (constructTargetTypeName != ZR_NULL) {
                    typeText = ZrCore_String_GetNativeString(constructTargetTypeName);
                }
            }

            prototype = typeText != ZR_NULL ? find_type_prototype_by_text(analyzer, typeText) : ZR_NULL;
            if (typeText != ZR_NULL && node->location.start.offset >= *bestOffset) {
                *bestPrototype = prototype;
                *bestOffset = node->location.start.offset;
                if (bestTypeName != ZR_NULL && bestTypeNameSize > 0) {
                    snprintf(bestTypeName, bestTypeNameSize, "%s", typeText);
                }
            }
            ZrParser_InferredType_Free(state, &inferredType);
            break;
        }

        default:
            break;
    }
}

static void find_construct_initialized_class_symbol_recursive(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrAstNode *node,
                                                              const TZrChar *receiverText,
                                                              TZrSize receiverLength,
                                                              TZrSize cursorOffset,
                                                              SZrSymbol **bestClassSymbol,
                                                              TZrSize *bestOffset) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || receiverText == ZR_NULL ||
        bestClassSymbol == ZR_NULL || bestOffset == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    find_construct_initialized_class_symbol_recursive(state,
                                                                      analyzer,
                                                                      script->statements->nodes[i],
                                                                      receiverText,
                                                                      receiverLength,
                                                                      cursorOffset,
                                                                      bestClassSymbol,
                                                                      bestOffset);
                }
            }
            break;
        }

        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    find_construct_initialized_class_symbol_recursive(state,
                                                                      analyzer,
                                                                      block->body->nodes[i],
                                                                      receiverText,
                                                                      receiverLength,
                                                                      cursorOffset,
                                                                      bestClassSymbol,
                                                                      bestOffset);
                }
            }
            break;
        }

        case ZR_AST_TEST_DECLARATION:
            if (node->data.testDeclaration.body != ZR_NULL) {
                find_construct_initialized_class_symbol_recursive(state,
                                                                  analyzer,
                                                                  node->data.testDeclaration.body,
                                                                  receiverText,
                                                                  receiverLength,
                                                                  cursorOffset,
                                                                  bestClassSymbol,
                                                                  bestOffset);
            }
            break;

        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrString *patternName = extract_identifier_name_from_node(varDecl->pattern);
            TZrNativeString patternText;
            TZrSize patternLength;

            if (node->location.start.offset > cursorOffset || patternName == ZR_NULL ||
                varDecl->value == ZR_NULL || varDecl->value->type != ZR_AST_CONSTRUCT_EXPRESSION) {
                break;
            }

            get_string_view(patternName, &patternText, &patternLength);
            if (patternText == ZR_NULL || patternLength != receiverLength ||
                memcmp(patternText, receiverText, receiverLength) != 0) {
                break;
            }

            if (varDecl->value->data.constructExpression.target != ZR_NULL &&
                varDecl->value->data.constructExpression.target->type == ZR_AST_IDENTIFIER_LITERAL) {
                SZrString *className =
                    varDecl->value->data.constructExpression.target->data.identifier.name;
                SZrSymbol *classSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable,
                                                                             className,
                                                                             ZR_NULL);
                if (classSymbol != ZR_NULL && classSymbol->type == ZR_SYMBOL_CLASS &&
                    node->location.start.offset >= *bestOffset) {
                    *bestClassSymbol = classSymbol;
                    *bestOffset = node->location.start.offset;
                }
            }
            break;
        }

        default:
            break;
    }
}

TZrBool ZrLanguageServer_Lsp_TryCollectReceiverCompletions(SZrState *state,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNode *ast,
                                                           const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrSize cursorOffset,
                                                           SZrArray *result) {
    TZrSize receiverEnd;
    TZrSize receiverStart;
    TZrSize receiverLength;
    SZrString *receiverName;
    SZrSymbol *receiverSymbol;
    TZrBool wantStatic = ZR_FALSE;
    SZrSymbol *classSymbol = ZR_NULL;
    const SZrTypePrototypeInfo *receiverPrototype = ZR_NULL;
    TZrChar receiverTypeName[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrSize bestOffset = 0;

    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL || content == ZR_NULL ||
        result == ZR_NULL || cursorOffset == 0 || cursorOffset > contentLength ||
        content[cursorOffset - 1] != '.') {
        return ZR_FALSE;
    }

    receiverEnd = cursorOffset - 1;
    receiverTypeName[0] = '\0';
    receiverStart = receiverEnd;
    while (receiverStart > 0) {
        TZrChar ch = content[receiverStart - 1];
        if (!isalnum((unsigned char)ch) && ch != '_') {
            break;
        }
        receiverStart--;
    }

    receiverLength = receiverEnd - receiverStart;
    if (receiverLength == 0) {
        if (try_infer_receiver_type_text_from_ast(state,
                                                  analyzer,
                                                  ast,
                                                  cursorOffset,
                                                  receiverTypeName,
                                                  sizeof(receiverTypeName))) {
            receiverPrototype = find_type_prototype_by_text(analyzer, receiverTypeName);
            if (receiverPrototype != ZR_NULL) {
                append_type_prototype_member_completions(state,
                                                         analyzer,
                                                         receiverPrototype,
                                                         ZR_FALSE,
                                                         0,
                                                         result);
                if (result->length > 0) {
                    return ZR_TRUE;
                }
            }
            return append_receiver_native_type_completions(state,
                                                           projectIndex,
                                                           analyzer,
                                                           ast,
                                                           receiverTypeName,
                                                           ZR_FALSE,
                                                           result);
        }
        return ZR_FALSE;
    }

    receiverName = ZrCore_String_Create(state,
                                        (TZrNativeString)(content + receiverStart),
                                        receiverLength);
    if (receiverName == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, receiverName, ZR_NULL);
    if (receiverSymbol != ZR_NULL && receiverSymbol->type == ZR_SYMBOL_CLASS) {
        classSymbol = receiverSymbol;
        wantStatic = ZR_TRUE;
    } else if (append_receiver_symbol_type_completions(state,
                                                       projectIndex,
                                                       analyzer,
                                                       ast,
                                                       receiverSymbol,
                                                       ZR_FALSE,
                                                       result)) {
        return ZR_TRUE;
    } else if (append_receiver_name_type_env_completions(state,
                                                         projectIndex,
                                                         analyzer,
                                                         ast,
                                                         receiverName,
                                                         ZR_FALSE,
                                                         result)) {
        return ZR_TRUE;
    } else {
        find_receiver_variable_prototype_recursive(state,
                                                  analyzer,
                                                  ast,
                                                  content + receiverStart,
                                                  receiverLength,
                                                  cursorOffset,
                                                  &receiverPrototype,
                                                  receiverTypeName,
                                                  sizeof(receiverTypeName),
                                                  &bestOffset);
        if (receiverPrototype != ZR_NULL) {
            append_type_prototype_member_completions(state,
                                                     analyzer,
                                                     receiverPrototype,
                                                     ZR_FALSE,
                                                     0,
                                                     result);
            return result->length > 0;
        }
        if (receiverTypeName[0] != '\0' &&
            append_receiver_native_type_completions(state,
                                                    projectIndex,
                                                    analyzer,
                                                    ast,
                                                    receiverTypeName,
                                                    ZR_FALSE,
                                                    result)) {
            return ZR_TRUE;
        }
        if (try_infer_receiver_type_text_from_ast(state,
                                                  analyzer,
                                                  ast,
                                                  cursorOffset,
                                                  receiverTypeName,
                                                  sizeof(receiverTypeName))) {
            receiverPrototype = find_type_prototype_by_text(analyzer, receiverTypeName);
            if (receiverPrototype != ZR_NULL) {
                append_type_prototype_member_completions(state,
                                                         analyzer,
                                                         receiverPrototype,
                                                         ZR_FALSE,
                                                         0,
                                                         result);
                if (result->length > 0) {
                    return ZR_TRUE;
                }
            }
            if (append_receiver_native_type_completions(state,
                                                        projectIndex,
                                                        analyzer,
                                                        ast,
                                                        receiverTypeName,
                                                        ZR_FALSE,
                                                        result)) {
                return ZR_TRUE;
            }
        }
    }

    {
        SZrSymbol *resolvedClassSymbol = classSymbol;
        TZrSize resolvedBestOffset = bestOffset;

        if (resolvedClassSymbol == ZR_NULL) {
            find_construct_initialized_class_symbol_recursive(state,
                                                              analyzer,
                                                              ast,
                                                              content + receiverStart,
                                                              receiverLength,
                                                              cursorOffset,
                                                              &resolvedClassSymbol,
                                                              &resolvedBestOffset);
        }

        if (resolvedClassSymbol == ZR_NULL) {
            return ZR_FALSE;
        }

        append_class_member_completions_recursive(state, analyzer, resolvedClassSymbol, wantStatic, 0, result);
        return result->length > 0;
    }
}

static TZrBool file_position_is_in_range(SZrFileRange position, SZrFileRange targetRange) {
    if (!ZrLanguageServer_Lsp_StringsEqual(position.source, targetRange.source) &&
        position.source != ZR_NULL && targetRange.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetRange.start.offset > 0 && targetRange.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return targetRange.start.offset <= position.start.offset &&
               position.end.offset <= targetRange.end.offset;
    }

    {
        TZrBool startMatch = (targetRange.start.line < position.start.line) ||
                             (targetRange.start.line == position.start.line &&
                              targetRange.start.column <= position.start.column);
        TZrBool endMatch = (position.end.line < targetRange.end.line) ||
                           (position.end.line == targetRange.end.line &&
                            position.end.column <= targetRange.end.column);
        return startMatch && endMatch;
    }
}

SZrSymbol *ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(SZrSemanticAnalyzer *analyzer, SZrFileRange position) {
    SZrSymbolScope *globalScope;
    TZrSize symbolIndex;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return ZR_NULL;
    }

    {
        SZrSymbol *definition = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
        if (definition != ZR_NULL) {
            return definition;
        }
    }

    globalScope = analyzer->symbolTable->globalScope;
    if (globalScope == ZR_NULL) {
        return ZR_NULL;
    }

    for (symbolIndex = 0; symbolIndex < globalScope->symbols.length; symbolIndex++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&globalScope->symbols, symbolIndex);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            SZrSymbol *symbol = *symbolPtr;
            TZrSize referenceIndex;
            if (file_position_is_in_range(position, ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol))) {
                return symbol;
            }

            for (referenceIndex = 0; referenceIndex < symbol->references.length; referenceIndex++) {
                SZrFileRange *referenceRange =
                    (SZrFileRange *)ZrCore_Array_Get(&symbol->references, referenceIndex);
                if (referenceRange != ZR_NULL && file_position_is_in_range(position, *referenceRange)) {
                    return symbol;
                }
            }
        }
    }

    return ZR_NULL;
}

// 更新文档
