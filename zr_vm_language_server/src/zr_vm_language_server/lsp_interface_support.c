//
// Created by Auto on 2025/01/XX.
//

#include "lsp_interface_internal.h"

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
        TZrSize lineStarts[32];
        TZrSize lineEnds[32];
        TZrSize count = 0;
        TZrSize cursor = declarationLineStart;

        while (cursor > 0 && count < 32) {
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
        TZrSize lineStarts[32];
        TZrSize lineEnds[32];
        TZrSize count = 0;
        TZrBool foundStart = ZR_FALSE;
        TZrSize cursor = declarationLineStart;

        while (cursor > 0 && count < 32) {
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
    TZrChar commentBuffer[1024];

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

SZrString *ZrLanguageServer_Lsp_ParseResolvedTypeFromHoverMarkdown(SZrState *state,
                                                                   SZrString *hoverMarkdown) {
    TZrNativeString text;
    TZrSize length;
    const TZrChar *prefix = "Resolved Type: ";
    TZrSize prefixLength = strlen(prefix);

    if (state == ZR_NULL || hoverMarkdown == ZR_NULL) {
        return ZR_NULL;
    }

    get_string_view(hoverMarkdown, &text, &length);
    if (text == ZR_NULL || length < prefixLength) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index + prefixLength <= length; index++) {
        TZrSize end;

        if (memcmp(text + index, prefix, prefixLength) != 0) {
            continue;
        }

        end = index + prefixLength;
        while (end < length && text[end] != '\n' && text[end] != '\r') {
            end++;
        }

        if (end <= index + prefixLength) {
            return ZR_NULL;
        }

        return ZrCore_String_Create(state,
                                    (TZrNativeString)(text + index + prefixLength),
                                    end - index - prefixLength);
    }

    return ZR_NULL;
}

SZrString *ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(SZrState *state,
                                                      SZrSymbol *symbol,
                                                      const TZrChar *content,
                                                      TZrSize contentLength) {
    TZrNativeString nameText;
    TZrSize nameLength;
    const TZrChar *kindText;
    TZrChar commentBuffer[1024];
    TZrChar markdownBuffer[2048];
    TZrChar typeBuffer[128];
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
    TZrChar buffer[512];
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
        case ZR_SYMBOL_MODULE: return 2;
        case ZR_SYMBOL_CLASS: return 5;
        case ZR_SYMBOL_METHOD: return 6;
        case ZR_SYMBOL_PROPERTY: return 7;
        case ZR_SYMBOL_FIELD: return 8;
        case ZR_SYMBOL_ENUM: return 10;
        case ZR_SYMBOL_INTERFACE: return 11;
        case ZR_SYMBOL_FUNCTION: return 12;
        case ZR_SYMBOL_VARIABLE:
        case ZR_SYMBOL_PARAMETER: return 13;
        case ZR_SYMBOL_ENUM_MEMBER: return 22;
        case ZR_SYMBOL_STRUCT: return 23;
        default: return 13;
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

static void append_class_member_completions_recursive(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrSymbol *classSymbol,
                                                      TZrBool wantStatic,
                                                      TZrSize depth,
                                                      SZrArray *result) {
    SZrClassDeclaration *classDecl;
    TZrSize memberIndex;

    if (state == ZR_NULL || analyzer == ZR_NULL || classSymbol == ZR_NULL || result == ZR_NULL ||
        depth > 8 || classSymbol->type != ZR_SYMBOL_CLASS || classSymbol->astNode == ZR_NULL ||
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
    TZrSize bestOffset = 0;

    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL || content == ZR_NULL ||
        result == ZR_NULL || cursorOffset == 0 || cursorOffset > contentLength ||
        content[cursorOffset - 1] != '.') {
        return ZR_FALSE;
    }

    receiverEnd = cursorOffset - 1;
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
    } else {
        find_construct_initialized_class_symbol_recursive(state,
                                                          analyzer,
                                                          ast,
                                                          content + receiverStart,
                                                          receiverLength,
                                                          cursorOffset,
                                                          &classSymbol,
                                                          &bestOffset);
    }

    if (classSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    append_class_member_completions_recursive(state, analyzer, classSymbol, wantStatic, 0, result);
    return result->length > 0;
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
