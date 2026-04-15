//
// Created by Auto on 2025/01/XX.
//

#include "interface/lsp_interface_internal.h"
#include "metadata/lsp_metadata_provider.h"
#include "module/lsp_module_metadata.h"
#include "project/lsp_project_internal.h"
#include "semantic/lsp_semantic_import_chain.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/type_inference.h"
#include "type_inference_internal.h"
#include "zr_vm_library/file.h"

static TZrBool symbol_name_matches(SZrSymbol *symbol, SZrString *name);
static TZrInt32 completion_metadata_symbol_priority(SZrSymbol *symbol);
static TZrBool completion_metadata_symbol_is_better(SZrSymbol *candidate, SZrSymbol *best);
static SZrSymbol *find_symbol_for_completion_metadata(SZrSymbolTable *table, SZrString *name);
static const SZrTypePrototypeInfo *find_type_prototype_by_text(SZrSemanticAnalyzer *analyzer,
                                                               const TZrChar *typeName);
static void append_type_prototype_member_completions(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     const SZrTypePrototypeInfo *prototype,
                                                     TZrBool wantStatic,
                                                     TZrSize depth,
                                                     SZrArray *result);
static TZrBool extract_base_type_name(const TZrChar *typeName,
                                      TZrChar *buffer,
                                      TZrSize bufferSize);

static TZrBool type_prototype_matches_name(const SZrTypePrototypeInfo *prototype, SZrString *typeName) {
    return prototype != ZR_NULL && prototype->name != ZR_NULL && typeName != ZR_NULL &&
           ZrCore_String_Equal(prototype->name, typeName);
}

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
           strcmp(text, "cannot infer exact type") != 0 &&
           strcmp(text, "object") != 0 &&
           strcmp(text, "unknown") != 0;
}

static TZrBool receiver_symbol_is_type_declaration(const SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    return symbol->type == ZR_SYMBOL_CLASS ||
           symbol->type == ZR_SYMBOL_STRUCT ||
           symbol->type == ZR_SYMBOL_INTERFACE ||
           symbol->type == ZR_SYMBOL_ENUM;
}

static void lsp_interface_support_normalize_path_for_compare(const TZrChar *path,
                                                             TZrChar *buffer,
                                                             TZrSize bufferSize) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *source = path;
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    if (ZrLibrary_File_NormalizePath((TZrNativeString)path, normalizedPath, sizeof(normalizedPath))) {
        source = normalizedPath;
    }

    for (TZrSize index = 0; source[index] != '\0' && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = source[index];
        if (current == '\\') {
            current = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }

    while (writeIndex > 1 && buffer[writeIndex - 1] == '/') {
        writeIndex--;
    }
    buffer[writeIndex] = '\0';
}

static TZrBool lsp_interface_support_file_range_contains_range(SZrFileRange outer, SZrFileRange inner) {
    if (!ZrLanguageServer_Lsp_StringsEqual(outer.source, inner.source) &&
        outer.source != ZR_NULL &&
        inner.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (outer.start.offset > 0 && outer.end.offset > 0 &&
        inner.start.offset > 0 && inner.end.offset > 0) {
        return outer.start.offset <= inner.start.offset &&
               inner.end.offset <= outer.end.offset;
    }

    return ((outer.start.line < inner.start.line) ||
            (outer.start.line == inner.start.line && outer.start.column <= inner.start.column)) &&
           ((inner.end.line < outer.end.line) ||
            (inner.end.line == outer.end.line && inner.end.column <= outer.end.column));
}

static TZrBool receiver_project_range_is_declared_in_extern_block(SZrAstNode *node, SZrFileRange range) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (receiver_project_range_is_declared_in_extern_block(node->data.script.statements->nodes[index],
                                                                           range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return node->data.compileTimeDeclaration.declaration != ZR_NULL &&
                   receiver_project_range_is_declared_in_extern_block(node->data.compileTimeDeclaration.declaration,
                                                                      range);

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (receiver_project_range_is_declared_in_extern_block(node->data.block.body->nodes[index],
                                                                           range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL &&
                node->data.externBlock.declarations->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    SZrAstNode *declaration = node->data.externBlock.declarations->nodes[index];

                    if (declaration != ZR_NULL &&
                        lsp_interface_support_file_range_contains_range(declaration->location, range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool receiver_project_declaration_is_ffi_wrapper(SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNode *declarationNode,
                                                           SZrFileRange declarationRange) {
    if (declarationNode != ZR_NULL &&
        (declarationNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION ||
         declarationNode->type == ZR_AST_EXTERN_DELEGATE_DECLARATION)) {
        return ZR_TRUE;
    }

    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if ((declarationRange.end.offset <= declarationRange.start.offset &&
         declarationRange.end.line <= declarationRange.start.line &&
         declarationRange.source == ZR_NULL) &&
        declarationNode != ZR_NULL) {
        declarationRange = declarationNode->location;
    }

    return receiver_project_range_is_declared_in_extern_block(analyzer->ast, declarationRange);
}

static EZrLspImportedModuleSourceKind receiver_project_member_source_kind(
    SZrSemanticAnalyzer *analyzer,
    SZrLspProjectFileRecord *sourceRecord,
    SZrAstNode *declarationNode,
    SZrFileRange declarationRange) {
    if (sourceRecord != ZR_NULL) {
        return sourceRecord->isFfiWrapperSource
                   ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
                   : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
    }

    return receiver_project_declaration_is_ffi_wrapper(analyzer, declarationNode, declarationRange)
               ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
               : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
}

static SZrFilePosition lsp_interface_support_file_position_from_offset(const TZrChar *content,
                                                                       TZrSize contentLength,
                                                                       TZrSize offset) {
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    if (content == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, line, column);
    }

    if (offset > contentLength) {
        offset = contentLength;
    }

    for (TZrSize index = 0; index < offset; index++) {
        if (content[index] == '\n') {
            line++;
            column = 1;
        } else if (content[index] != '\r') {
            column++;
        }
    }

    return ZrParser_FilePosition_Create(offset, line, column);
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

TZrBool ZrLanguageServer_Lsp_UrisResolveToSameNativePath(SZrString *left, SZrString *right) {
    TZrChar leftPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar rightPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedLeft[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRight[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_Lsp_StringsEqual(left, right)) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_Lsp_FileUriToNativePath(left, leftPath, sizeof(leftPath)) ||
        !ZrLanguageServer_Lsp_FileUriToNativePath(right, rightPath, sizeof(rightPath))) {
        return ZR_FALSE;
    }

    lsp_interface_support_normalize_path_for_compare(leftPath, normalizedLeft, sizeof(normalizedLeft));
    lsp_interface_support_normalize_path_for_compare(rightPath, normalizedRight, sizeof(normalizedRight));
    return normalizedLeft[0] != '\0' && strcmp(normalizedLeft, normalizedRight) == 0;
}

SZrHashKeyValuePair *ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(SZrState *state,
                                                                   SZrHashSet *set,
                                                                   SZrString *uri) {
    TZrSize bucketIndex;

    ZR_UNUSED_PARAMETER(state);

    if (set == ZR_NULL || uri == ZR_NULL || !set->isValid || set->buckets == ZR_NULL) {
        return ZR_NULL;
    }

    for (bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
        while (pair != ZR_NULL) {
            SZrRawObject *rawObject = ZrCore_Value_GetRawObject(&pair->key);
            SZrString *storedUri = rawObject != ZR_NULL ? (SZrString *)rawObject : ZR_NULL;

            if (storedUri != ZR_NULL && ZrLanguageServer_Lsp_UrisResolveToSameNativePath(storedUri, uri)) {
                return pair;
            }

            pair = pair->next;
        }
    }

    return ZR_NULL;
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

static void append_symbol_ffi_hover_metadata(SZrSymbol *symbol,
                                             TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *used) {
    TZrNativeString metadataText;
    TZrSize metadataLength;

    if (symbol == ZR_NULL || symbol->ffiHoverMetadata == ZR_NULL || buffer == ZR_NULL || used == ZR_NULL ||
        bufferSize == 0) {
        return;
    }

    get_string_view(symbol->ffiHoverMetadata, &metadataText, &metadataLength);
    if (metadataText == ZR_NULL || metadataLength == 0) {
        return;
    }
    append_buffer_slice(buffer, bufferSize, used, metadataText, metadataLength);
}

SZrString *ZrLanguageServer_Lsp_AppendSymbolFfiMetadataMarkdown(SZrState *state,
                                                                SZrString *base,
                                                                SZrSymbol *symbol) {
    TZrNativeString baseText;
    TZrSize baseLength;
    TZrChar metadataBuffer[ZR_LSP_COMMENT_BUFFER_LENGTH];
    TZrChar combinedBuffer[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrSize metadataUsed = 0;
    TZrSize combinedUsed = 0;

    if (state == ZR_NULL || base == ZR_NULL || symbol == ZR_NULL) {
        return base;
    }

    metadataBuffer[0] = '\0';
    append_symbol_ffi_hover_metadata(symbol, metadataBuffer, sizeof(metadataBuffer), &metadataUsed);
    if (metadataUsed == 0) {
        return base;
    }

    get_string_view(base, &baseText, &baseLength);
    if (baseText == ZR_NULL || baseLength == 0 || strstr(baseText, metadataBuffer) != ZR_NULL) {
        return base;
    }

    combinedBuffer[0] = '\0';
    append_buffer_slice(combinedBuffer, sizeof(combinedBuffer), &combinedUsed, baseText, baseLength);
    append_buffer_slice(combinedBuffer, sizeof(combinedBuffer), &combinedUsed, metadataBuffer, metadataUsed);
    return ZrCore_String_Create(state, combinedBuffer, strlen(combinedBuffer));
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

    append_symbol_ffi_hover_metadata(symbol, markdownBuffer, sizeof(markdownBuffer), &used);

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

    symbol = find_symbol_for_completion_metadata(analyzer->symbolTable, item->label);
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
    ZrCore_Array_Init(state,
                      &lspDiag->relatedInformation,
                      sizeof(SZrLspDiagnosticRelatedInformation),
                      diag->relatedInformation.length > 0 ? (TZrSize)diag->relatedInformation.length : 0);
    for (TZrSize index = 0; index < diag->relatedInformation.length; index++) {
        SZrDiagnosticRelatedInformation *related =
            (SZrDiagnosticRelatedInformation *)ZrCore_Array_Get(&diag->relatedInformation, index);
        SZrLspDiagnosticRelatedInformation lspRelated;

        if (related == ZR_NULL) {
            continue;
        }

        memset(&lspRelated, 0, sizeof(lspRelated));
        lspRelated.location.uri = related->location.source;
        lspRelated.location.range = ZrLanguageServer_LspRange_FromFileRange(related->location);
        lspRelated.message = related->message;
        ZrCore_Array_Push(state, &lspDiag->relatedInformation, &lspRelated);
    }
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

static const SZrTypePrototypeInfo *resolve_construct_target_prototype_from_node(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *node,
        TZrChar *buffer,
        TZrSize bufferSize) {
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    SZrString *resolvedTypeName = ZR_NULL;
    EZrObjectPrototypeType prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    TZrBool allowValueConstruction = ZR_FALSE;
    TZrBool allowBoxedConstruction = ZR_FALSE;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    if (!resolve_prototype_target_inference(analyzer->compilerState, node, &prototype, &resolvedTypeName) &&
        !resolve_source_type_declaration_target_inference(analyzer->compilerState,
                                                          node,
                                                          &resolvedTypeName,
                                                          &prototypeType,
                                                          &allowValueConstruction,
                                                          &allowBoxedConstruction)) {
        return ZR_NULL;
    }

    if (resolvedTypeName != ZR_NULL && buffer != ZR_NULL && bufferSize > 0) {
        snprintf(buffer, bufferSize, "%s", ZrCore_String_GetNativeString(resolvedTypeName));
    }

    if (prototype != ZR_NULL) {
        return prototype;
    }

    if (resolvedTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    return find_type_prototype_by_text(analyzer, ZrCore_String_GetNativeString(resolvedTypeName));
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

static TZrBool symbol_name_matches(SZrSymbol *symbol, SZrString *name) {
    return symbol != ZR_NULL &&
           symbol->name != ZR_NULL &&
           name != ZR_NULL &&
           ZrLanguageServer_Lsp_StringsEqual(symbol->name, name);
}

static TZrInt32 completion_metadata_symbol_priority(SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return -1;
    }

    switch (symbol->type) {
        case ZR_SYMBOL_PROPERTY:
            return 4;
        case ZR_SYMBOL_METHOD:
            return 3;
        case ZR_SYMBOL_FIELD:
            return 2;
        default:
            return 1;
    }
}

static TZrBool completion_metadata_symbol_is_better(SZrSymbol *candidate, SZrSymbol *best) {
    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    if (best == ZR_NULL) {
        return ZR_TRUE;
    }

    return completion_metadata_symbol_priority(candidate) > completion_metadata_symbol_priority(best);
}

static SZrSymbol *find_symbol_for_completion_metadata(SZrSymbolTable *table, SZrString *name) {
    SZrSymbol *best = ZR_NULL;

    if (table == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    best = ZrLanguageServer_SymbolTable_Lookup(table, name, ZR_NULL);
    if (best != ZR_NULL) {
        return best;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || !symbol_name_matches(*symbolPtr, name)) {
                continue;
            }

            if (completion_metadata_symbol_is_better(*symbolPtr, best)) {
                best = *symbolPtr;
            }
        }
    }

    return best;
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

static SZrString *completion_type_member_display_name(SZrState *state,
                                                      const SZrTypeMemberInfo *member,
                                                      const TZrChar **outKind) {
    const TZrChar *memberName;
    const TZrChar *propertyPrefix = ZR_NULL;
    TZrSize propertyPrefixLength = 0;

    if (outKind != ZR_NULL) {
        *outKind = ZR_NULL;
    }

    if (state == ZR_NULL || member == ZR_NULL || member->name == ZR_NULL) {
        return ZR_NULL;
    }

    memberName = ZrCore_String_GetNativeStringShort(member->name);
    if (memberName == ZR_NULL) {
        memberName = ZrCore_String_GetNativeString(member->name);
    }
    if (memberName == ZR_NULL) {
        return member->name;
    }

    if (strncmp(memberName, "__get_", 6) == 0) {
        propertyPrefix = "__get_";
        propertyPrefixLength = 6;
    } else if (strncmp(memberName, "__set_", 6) == 0) {
        propertyPrefix = "__set_";
        propertyPrefixLength = 6;
    }

    if (propertyPrefix != ZR_NULL && memberName[propertyPrefixLength] != '\0') {
        if (outKind != ZR_NULL) {
            *outKind = "property";
        }
        return ZrCore_String_Create(state,
                                    (TZrNativeString)(memberName + propertyPrefixLength),
                                    strlen(memberName + propertyPrefixLength));
    }

    return member->name;
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

static void append_struct_member_completions_recursive(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrSymbol *structSymbol,
                                                       TZrBool wantStatic,
                                                       TZrSize depth,
                                                       SZrArray *result) {
    SZrStructDeclaration *structDecl;
    TZrSize memberIndex;

    if (state == ZR_NULL || analyzer == ZR_NULL || structSymbol == ZR_NULL || result == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH || structSymbol->type != ZR_SYMBOL_STRUCT ||
        structSymbol->astNode == ZR_NULL ||
        structSymbol->astNode->type != ZR_AST_STRUCT_DECLARATION) {
        return;
    }

    structDecl = &structSymbol->astNode->data.structDeclaration;
    if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
        for (memberIndex = 0; memberIndex < structDecl->members->count; memberIndex++) {
            SZrAstNode *memberNode = structDecl->members->nodes[memberIndex];
            if (memberNode == ZR_NULL) {
                continue;
            }

            if (memberNode->type == ZR_AST_STRUCT_FIELD &&
                memberNode->data.structField.isStatic == wantStatic &&
                memberNode->data.structField.name != ZR_NULL) {
                append_completion_item_for_symbol_name(state,
                                                       result,
                                                       memberNode->data.structField.name->name,
                                                       "field");
            } else if (memberNode->type == ZR_AST_STRUCT_METHOD &&
                       memberNode->data.structMethod.isStatic == wantStatic &&
                       memberNode->data.structMethod.name != ZR_NULL) {
                append_completion_item_for_symbol_name(state,
                                                       result,
                                                       memberNode->data.structMethod.name->name,
                                                       "method");
            }
        }
    }

    if (structDecl->inherits != ZR_NULL && structDecl->inherits->nodes != ZR_NULL) {
        for (memberIndex = 0; memberIndex < structDecl->inherits->count; memberIndex++) {
            SZrAstNode *inheritNode = structDecl->inherits->nodes[memberIndex];
            if (inheritNode != ZR_NULL && inheritNode->type == ZR_AST_TYPE &&
                inheritNode->data.type.name != ZR_NULL &&
                inheritNode->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
                SZrString *baseName = inheritNode->data.type.name->data.identifier.name;
                SZrSymbol *baseStruct = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable,
                                                                            baseName,
                                                                            ZR_NULL);
                if (baseStruct != ZR_NULL && baseStruct != structSymbol) {
                    append_struct_member_completions_recursive(state,
                                                               analyzer,
                                                               baseStruct,
                                                               wantStatic,
                                                               depth + 1,
                                                               result);
                }
            }
        }
    }
}

static TZrBool append_type_symbol_member_completions_by_name(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             const TZrChar *typeText,
                                                             TZrBool wantStatic,
                                                             SZrArray *result) {
    TZrChar baseTypeName[ZR_LSP_TYPE_BUFFER_LENGTH];
    SZrString *typeName;
    SZrSymbol *typeSymbol;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        typeText == ZR_NULL || typeText[0] == '\0' || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!extract_base_type_name(typeText, baseTypeName, sizeof(baseTypeName))) {
        return ZR_FALSE;
    }

    typeName = ZrCore_String_Create(state, (TZrNativeString)baseTypeName, strlen(baseTypeName));
    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    typeSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, typeName, ZR_NULL);
    if (typeSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeSymbol->type == ZR_SYMBOL_CLASS) {
        append_class_member_completions_recursive(state, analyzer, typeSymbol, wantStatic, 0, result);
    } else if (typeSymbol->type == ZR_SYMBOL_STRUCT) {
        append_struct_member_completions_recursive(state, analyzer, typeSymbol, wantStatic, 0, result);
    }

    return result->length > 0;
}

static const SZrTypePrototypeInfo *find_type_prototype_by_text(SZrSemanticAnalyzer *analyzer,
                                                               const TZrChar *typeName) {
    SZrCompilerState *compilerState;
    const SZrTypePrototypeInfo *bestMatch = ZR_NULL;
    SZrString *typeNameString;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    compilerState = analyzer->compilerState;
    for (TZrSize index = 0; index < compilerState->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
            (const SZrTypePrototypeInfo *)ZrCore_Array_Get(&compilerState->typePrototypes, index);
        if (prototype != ZR_NULL &&
            prototype->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(prototype->name), typeName) == 0) {
            if (bestMatch == ZR_NULL || (bestMatch->isImportedNative && !prototype->isImportedNative)) {
                bestMatch = prototype;
                if (!prototype->isImportedNative) {
                    return prototype;
                }
            }
        }
    }

    if (compilerState->currentTypePrototypeInfo != ZR_NULL &&
        compilerState->currentTypePrototypeInfo->name != ZR_NULL &&
        strcmp(ZrCore_String_GetNativeString(compilerState->currentTypePrototypeInfo->name), typeName) == 0 &&
        (bestMatch == ZR_NULL ||
         (bestMatch->isImportedNative && !compilerState->currentTypePrototypeInfo->isImportedNative))) {
        bestMatch = compilerState->currentTypePrototypeInfo;
        if (!bestMatch->isImportedNative) {
            return bestMatch;
        }
    }

    if (bestMatch != ZR_NULL) {
        return bestMatch;
    }

    typeNameString = compilerState->state != ZR_NULL
                             ? ZrCore_String_Create(compilerState->state, (TZrNativeString)typeName, strlen(typeName))
                             : ZR_NULL;
    if (typeNameString == ZR_NULL || !find_compiler_type_prototype_inference(compilerState, typeNameString)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < compilerState->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
            (const SZrTypePrototypeInfo *)ZrCore_Array_Get(&compilerState->typePrototypes, index);
        if (prototype != ZR_NULL &&
            prototype->name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(prototype->name), typeName) == 0 &&
            (bestMatch == ZR_NULL || (bestMatch->isImportedNative && !prototype->isImportedNative))) {
            bestMatch = prototype;
            if (!prototype->isImportedNative) {
                return prototype;
            }
        }
    }

    if (compilerState->currentTypePrototypeInfo != ZR_NULL &&
        compilerState->currentTypePrototypeInfo->name != ZR_NULL &&
        strcmp(ZrCore_String_GetNativeString(compilerState->currentTypePrototypeInfo->name), typeName) == 0 &&
        (bestMatch == ZR_NULL ||
         (bestMatch->isImportedNative && !compilerState->currentTypePrototypeInfo->isImportedNative))) {
        bestMatch = compilerState->currentTypePrototypeInfo;
    }

    return bestMatch;
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

static const ZrLibTypeDescriptor *find_native_type_descriptor_in_module_graph(
    SZrState *state,
    const ZrLibModuleDescriptor *module,
    const TZrChar *typeName,
    const ZrLibModuleDescriptor **outModule,
    TZrSize depth) {
    const ZrLibTypeDescriptor *typeDescriptor;

    if (outModule != ZR_NULL) {
        *outModule = ZR_NULL;
    }
    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return ZR_NULL;
    }

    typeDescriptor = find_native_type_descriptor_in_module(module, typeName);
    if (typeDescriptor != ZR_NULL) {
        if (outModule != ZR_NULL) {
            *outModule = module;
        }
        return typeDescriptor;
    }

    for (TZrSize index = 0; index < module->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *link = &module->moduleLinks[index];
        const ZrLibModuleDescriptor *linkedModule;
        const ZrLibTypeDescriptor *linkedTypeDescriptor;

        if (link->moduleName == ZR_NULL || link->moduleName[0] == '\0') {
            continue;
        }

        linkedModule = ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state,
                                                                                        link->moduleName,
                                                                                        ZR_NULL);
        if (linkedModule == ZR_NULL || linkedModule == module) {
            continue;
        }

        linkedTypeDescriptor = find_native_type_descriptor_in_module_graph(state,
                                                                           linkedModule,
                                                                           typeName,
                                                                           outModule,
                                                                           depth + 1);
        if (linkedTypeDescriptor != ZR_NULL) {
            return linkedTypeDescriptor;
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
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZrLanguageServer_LspModuleMetadata_FindNativeTypeDescriptor(state, typeName, outModule);
    }

    {
        SZrArray bindings;
        const ZrLibTypeDescriptor *typeDescriptor = ZR_NULL;

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

            typeDescriptor = find_native_type_descriptor_in_module_graph(state,
                                                                         resolvedModule.nativeDescriptor,
                                                                         typeName,
                                                                         outModule,
                                                                         0);
            if (typeDescriptor != ZR_NULL) {
                ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
                return typeDescriptor;
            }
        }
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    }

    return ZrLanguageServer_LspModuleMetadata_FindNativeTypeDescriptor(state, typeName, outModule);
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

static TZrBool receiver_name_is_explicit_type_binding(SZrSemanticAnalyzer *analyzer, SZrString *receiverName) {
    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || receiverName == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrParser_TypeEnvironment_LookupType(analyzer->compilerState->typeEnv, receiverName);
}

static TZrBool append_receiver_explicit_type_binding_completions(SZrState *state,
                                                                 SZrLspProjectIndex *projectIndex,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrAstNode *ast,
                                                                 SZrString *receiverName,
                                                                 SZrArray *result) {
    const TZrChar *typeText;
    const SZrTypePrototypeInfo *prototype;

    if (state == ZR_NULL || analyzer == ZR_NULL || receiverName == ZR_NULL || result == ZR_NULL ||
        !receiver_name_is_explicit_type_binding(analyzer, receiverName)) {
        return ZR_FALSE;
    }

    typeText = ZrCore_String_GetNativeString(receiverName);
    if (typeText == ZR_NULL || typeText[0] == '\0') {
        return ZR_FALSE;
    }

    prototype = find_type_prototype_by_text(analyzer, typeText);
    if (prototype != ZR_NULL) {
        append_type_prototype_member_completions(state, analyzer, prototype, ZR_TRUE, 0, result);
        if (result->length > 0) {
            return ZR_TRUE;
        }
    }

    if (append_type_symbol_member_completions_by_name(state, analyzer, typeText, ZR_TRUE, result)) {
        return ZR_TRUE;
    }

    return append_receiver_native_type_completions(state,
                                                   projectIndex,
                                                   analyzer,
                                                   ast,
                                                   typeText,
                                                   ZR_TRUE,
                                                   result);
}

static TZrBool prototype_array_layout_matches(const SZrArray *array, TZrSize elementSize) {
    if (array == ZR_NULL || !array->isValid || array->elementSize != elementSize) {
        return ZR_FALSE;
    }

    if (array->length == 0) {
        return ZR_TRUE;
    }

    return array->head != ZR_NULL && array->capacity >= array->length;
}

static SZrSymbol *lookup_receiver_symbol_at_offset(SZrSemanticAnalyzer *analyzer,
                                                   SZrString *uri,
                                                   const TZrChar *content,
                                                   TZrSize contentLength,
                                                   TZrSize cursorOffset,
                                                   SZrString *receiverName) {
    SZrFilePosition filePosition;
    SZrFileRange fileRange;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || receiverName == ZR_NULL ||
        uri == ZR_NULL || content == ZR_NULL || cursorOffset > contentLength) {
        return analyzer != ZR_NULL
                   ? ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, receiverName, ZR_NULL)
                   : ZR_NULL;
    }

    filePosition = lsp_interface_support_file_position_from_offset(content, contentLength, cursorOffset);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    return ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable, receiverName, fileRange);
}

static TZrBool resolve_receiver_type_text(SZrState *state,
                                          SZrSemanticAnalyzer *analyzer,
                                          SZrString *uri,
                                          SZrAstNode *ast,
                                          const TZrChar *content,
                                          TZrSize contentLength,
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
    receiverSymbol = lookup_receiver_symbol_at_offset(analyzer,
                                                      uri,
                                                      content,
                                                      contentLength,
                                                      cursorOffset,
                                                      receiverName);
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

static SZrFileRange receiver_project_member_lookup_range(SZrAstNode *declarationNode) {
    if (declarationNode == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                         ZrParser_FilePosition_Create(0, 0, 0),
                                         ZR_NULL);
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_FIELD:
            return declarationNode->data.classField.nameLocation;
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.nameLocation;
        case ZR_AST_CLASS_PROPERTY:
            if (declarationNode->data.classProperty.modifier != ZR_NULL) {
                return receiver_project_member_lookup_range(declarationNode->data.classProperty.modifier);
            }
            break;
        case ZR_AST_PROPERTY_GET:
            return declarationNode->data.propertyGet.nameLocation;
        case ZR_AST_PROPERTY_SET:
            return declarationNode->data.propertySet.nameLocation;
        default:
            break;
    }

    return declarationNode->location;
}

static TZrBool receiver_project_identifier_boundary(const TZrChar *content,
                                                    TZrSize contentLength,
                                                    TZrSize offset) {
    if (content == ZR_NULL || offset >= contentLength) {
        return ZR_TRUE;
    }

    return !(isalnum((unsigned char)content[offset]) || content[offset] == '_');
}

static TZrBool receiver_project_try_member_name_range(const TZrChar *content,
                                                      TZrSize contentLength,
                                                      SZrString *uri,
                                                      SZrAstNode *declarationNode,
                                                      SZrString *memberName,
                                                      SZrFileRange *outRange) {
    TZrNativeString memberText;
    TZrSize memberLength;
    TZrSize searchStart;
    TZrSize searchEnd;

    if (content == ZR_NULL || declarationNode == ZR_NULL || memberName == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_view(memberName, &memberText, &memberLength);
    if (memberText == ZR_NULL || memberLength == 0) {
        return ZR_FALSE;
    }

    searchStart = declarationNode->location.start.offset <= contentLength
                      ? declarationNode->location.start.offset
                      : contentLength;
    searchEnd = declarationNode->location.end.offset > searchStart &&
                        declarationNode->location.end.offset <= contentLength
                    ? declarationNode->location.end.offset
                    : searchStart;

    while (searchStart > 0 && content[searchStart - 1] != '\n' && content[searchStart - 1] != '\r') {
        searchStart--;
    }
    while (searchEnd < contentLength && content[searchEnd] != '\n' && content[searchEnd] != '\r') {
        searchEnd++;
    }

    for (TZrSize index = searchStart; index + memberLength <= searchEnd; index++) {
        if (memcmp(content + index, memberText, memberLength) != 0) {
            continue;
        }

        if (!receiver_project_identifier_boundary(content, contentLength, index == 0 ? contentLength : index - 1) ||
            !receiver_project_identifier_boundary(content, contentLength, index + memberLength)) {
            continue;
        }

        *outRange = ZrParser_FileRange_Create(
            lsp_interface_support_file_position_from_offset(content, contentLength, index),
            lsp_interface_support_file_position_from_offset(content, contentLength, index + memberLength),
            uri);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrFileRange receiver_project_member_declaration_range(SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              SZrAstNode *declarationNode,
                                                              SZrString *memberName) {
    SZrFileRange range;

    if (declarationNode == ZR_NULL) {
        return ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                         ZrParser_FilePosition_Create(0, 0, 0),
                                         ZR_NULL);
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_FIELD:
            range = declarationNode->data.classField.nameLocation;
            break;

        case ZR_AST_STRUCT_FIELD:
            if (receiver_project_try_member_name_range(content, contentLength, uri, declarationNode, memberName, &range)) {
                return range;
            }
            range = declarationNode->location;
            break;

        case ZR_AST_CLASS_METHOD:
            range = declarationNode->data.classMethod.nameLocation;
            break;

        case ZR_AST_STRUCT_METHOD:
            if (receiver_project_try_member_name_range(content, contentLength, uri, declarationNode, memberName, &range)) {
                return range;
            }
            range = declarationNode->location;
            break;

        case ZR_AST_CLASS_PROPERTY:
            if (declarationNode->data.classProperty.modifier != ZR_NULL) {
                return receiver_project_member_declaration_range(uri,
                                                                 content,
                                                                 contentLength,
                                                                 declarationNode->data.classProperty.modifier,
                                                                 memberName);
            }
            range = declarationNode->location;
            break;

        case ZR_AST_PROPERTY_GET:
            range = declarationNode->data.propertyGet.nameLocation;
            break;

        case ZR_AST_PROPERTY_SET:
            range = declarationNode->data.propertySet.nameLocation;
            break;

        case ZR_AST_ENUM_MEMBER:
            if (receiver_project_try_member_name_range(content, contentLength, uri, declarationNode, memberName, &range)) {
                return range;
            }
            range = declarationNode->location;
            break;

        default:
            range = declarationNode->location;
            break;
    }

    if (range.source == ZR_NULL) {
        range.source = uri;
    }
    return range;
}

static TZrBool receiver_project_node_declares_type(SZrAstNode *node, SZrString *typeName) {
    if (node == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            return node->data.classDeclaration.name != ZR_NULL &&
                   node->data.classDeclaration.name->name != ZR_NULL &&
                   ZrLanguageServer_Lsp_StringsEqual(node->data.classDeclaration.name->name, typeName);

        case ZR_AST_STRUCT_DECLARATION:
            return node->data.structDeclaration.name != ZR_NULL &&
                   node->data.structDeclaration.name->name != ZR_NULL &&
                   ZrLanguageServer_Lsp_StringsEqual(node->data.structDeclaration.name->name, typeName);

        case ZR_AST_ENUM_DECLARATION:
            return node->data.enumDeclaration.name != ZR_NULL &&
                   node->data.enumDeclaration.name->name != ZR_NULL &&
                   ZrLanguageServer_Lsp_StringsEqual(node->data.enumDeclaration.name->name, typeName);

        default:
            return ZR_FALSE;
    }
}

static SZrAstNode *receiver_project_find_type_declaration_recursive(SZrAstNode *node, SZrString *typeName);

static SZrAstNode *receiver_project_find_type_declaration_in_array(SZrAstNodeArray *nodes, SZrString *typeName) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        SZrAstNode *match = receiver_project_find_type_declaration_recursive(nodes->nodes[index], typeName);
        if (match != ZR_NULL) {
            return match;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *receiver_project_find_type_declaration_recursive(SZrAstNode *node, SZrString *typeName) {
    if (node == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (receiver_project_node_declares_type(node, typeName)) {
        return node;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return receiver_project_find_type_declaration_in_array(node->data.script.statements, typeName);

        case ZR_AST_BLOCK:
            return receiver_project_find_type_declaration_in_array(node->data.block.body, typeName);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return receiver_project_find_type_declaration_recursive(node->data.compileTimeDeclaration.declaration,
                                                                    typeName);

        case ZR_AST_EXTERN_BLOCK:
            return receiver_project_find_type_declaration_in_array(node->data.externBlock.declarations, typeName);

        default:
            return ZR_NULL;
    }
}

static SZrAstNode *receiver_project_find_type_member_declaration(SZrAstNode *typeDeclaration,
                                                                 SZrString *memberName,
                                                                 EZrLspMetadataMemberKind *outKind) {
    SZrAstNodeArray *members = ZR_NULL;

    if (outKind != ZR_NULL) {
        *outKind = ZR_LSP_METADATA_MEMBER_NONE;
    }
    if (typeDeclaration == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclaration->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDeclaration->data.classDeclaration.members;
    } else if (typeDeclaration->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDeclaration->data.structDeclaration.members;
    } else if (typeDeclaration->type == ZR_AST_ENUM_DECLARATION) {
        members = typeDeclaration->data.enumDeclaration.members;
    } else {
        return ZR_NULL;
    }

    if (members == ZR_NULL || members->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];
        SZrString *name = ZR_NULL;
        EZrLspMetadataMemberKind kind = ZR_LSP_METADATA_MEMBER_NONE;

        if (member == ZR_NULL) {
            continue;
        }

        switch (member->type) {
            case ZR_AST_CLASS_FIELD:
                name = member->data.classField.name != ZR_NULL ? member->data.classField.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_FIELD;
                break;

            case ZR_AST_STRUCT_FIELD:
                name = member->data.structField.name != ZR_NULL ? member->data.structField.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_FIELD;
                break;

            case ZR_AST_CLASS_METHOD:
                name = member->data.classMethod.name != ZR_NULL ? member->data.classMethod.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_METHOD;
                break;

            case ZR_AST_STRUCT_METHOD:
                name = member->data.structMethod.name != ZR_NULL ? member->data.structMethod.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_METHOD;
                break;

            case ZR_AST_CLASS_PROPERTY:
                if (member->data.classProperty.modifier != ZR_NULL) {
                    if (member->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
                        member->data.classProperty.modifier->data.propertyGet.name != ZR_NULL) {
                        name = member->data.classProperty.modifier->data.propertyGet.name->name;
                        kind = ZR_LSP_METADATA_MEMBER_FIELD;
                    } else if (member->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
                               member->data.classProperty.modifier->data.propertySet.name != ZR_NULL) {
                        name = member->data.classProperty.modifier->data.propertySet.name->name;
                        kind = ZR_LSP_METADATA_MEMBER_FIELD;
                    }
                }
                break;

            case ZR_AST_ENUM_MEMBER:
                name = member->data.enumMember.name != ZR_NULL ? member->data.enumMember.name->name : ZR_NULL;
                kind = ZR_LSP_METADATA_MEMBER_CONSTANT;
                break;

            default:
                break;
        }

        if (name != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(name, memberName)) {
            if (outKind != ZR_NULL) {
                *outKind = kind;
            }
            return member;
        }
    }

    return ZR_NULL;
}

static EZrLspMetadataMemberKind receiver_project_member_kind(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return ZR_LSP_METADATA_MEMBER_NONE;
    }

    switch (memberInfo->memberType) {
        case ZR_AST_CLASS_FIELD:
        case ZR_AST_STRUCT_FIELD:
        case ZR_AST_CLASS_PROPERTY:
            return ZR_LSP_METADATA_MEMBER_FIELD;
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
            return ZR_LSP_METADATA_MEMBER_METHOD;
        default:
            return ZR_LSP_METADATA_MEMBER_NONE;
    }
}

static const SZrTypeMemberInfo *find_receiver_project_member_recursive(SZrCompilerState *compilerState,
                                                                       SZrTypePrototypeInfo *prototype,
                                                                       SZrString *memberName,
                                                                       TZrUInt32 depth) {
    SZrArray membersSnapshot;
    SZrArray inheritsSnapshot;
    SZrString *prototypeName;
    SZrString *extendsTypeName;

    if (compilerState == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return ZR_NULL;
    }

    membersSnapshot = prototype->members;
    inheritsSnapshot = prototype->inherits;
    prototypeName = prototype->name;
    extendsTypeName = prototype->extendsTypeName;

    for (TZrSize index = 0; index < membersSnapshot.length; index++) {
        const SZrTypeMemberInfo *memberInfo =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, index);
        if (memberInfo == ZR_NULL || memberInfo->name == ZR_NULL ||
            memberInfo->isMetaMethod ||
            receiver_project_member_kind(memberInfo) == ZR_LSP_METADATA_MEMBER_NONE) {
            continue;
        }

        if (ZrLanguageServer_Lsp_StringsEqual(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    if (extendsTypeName != ZR_NULL) {
        SZrTypePrototypeInfo *basePrototype =
            find_compiler_type_prototype_inference(compilerState, extendsTypeName);
        const SZrTypeMemberInfo *memberInfo;

        if (basePrototype != ZR_NULL && !type_prototype_matches_name(basePrototype, prototypeName)) {
            memberInfo = find_receiver_project_member_recursive(compilerState,
                                                                basePrototype,
                                                                memberName,
                                                                depth + 1);
            if (memberInfo != ZR_NULL) {
                return memberInfo;
            }
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
        SZrTypePrototypeInfo *inheritPrototype;
        const SZrTypeMemberInfo *memberInfo;

        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL) {
            continue;
        }

        inheritPrototype = find_compiler_type_prototype_inference(compilerState, *inheritTypeNamePtr);
        if (inheritPrototype == ZR_NULL || type_prototype_matches_name(inheritPrototype, prototypeName)) {
            continue;
        }

        memberInfo = find_receiver_project_member_recursive(compilerState,
                                                            inheritPrototype,
                                                            memberName,
                                                            depth + 1);
        if (memberInfo != ZR_NULL) {
            return memberInfo;
        }
    }

    return ZR_NULL;
}

static void receiver_project_member_set_type_text(SZrState *state,
                                                  const SZrTypeMemberInfo *memberInfo,
                                                  SZrLspResolvedMetadataMember *outResolved) {
    const TZrChar *typeText = ZR_NULL;

    if (state == ZR_NULL || memberInfo == ZR_NULL || outResolved == ZR_NULL) {
        return;
    }

    switch (receiver_project_member_kind(memberInfo)) {
        case ZR_LSP_METADATA_MEMBER_FIELD:
            typeText = memberInfo->fieldTypeName != ZR_NULL
                           ? ZrCore_String_GetNativeString(memberInfo->fieldTypeName)
                           : ZR_NULL;
            break;
        case ZR_LSP_METADATA_MEMBER_METHOD:
            typeText = memberInfo->returnTypeName != ZR_NULL
                           ? ZrCore_String_GetNativeString(memberInfo->returnTypeName)
                           : ZR_NULL;
            break;
        default:
            break;
    }

    if (typeText != ZR_NULL && typeText[0] != '\0') {
        outResolved->resolvedTypeText = ZrCore_String_Create(state, (TZrNativeString)typeText, strlen(typeText));
    }
}

static void receiver_project_set_type_text_from_symbol(SZrState *state,
                                                       SZrSymbol *symbol,
                                                       SZrLspResolvedMetadataMember *outResolved) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText;

    if (state == ZR_NULL || symbol == ZR_NULL || symbol->typeInfo == ZR_NULL || outResolved == ZR_NULL ||
        outResolved->resolvedTypeText != ZR_NULL) {
        return;
    }

    typeText = ZrParser_TypeNameString_Get(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer));
    if (typeText != ZR_NULL && typeText[0] != '\0') {
        outResolved->resolvedTypeText = ZrCore_String_Create(state, (TZrNativeString)typeText, strlen(typeText));
    }
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
                                                            SZrString *uri,
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
                                        uri,
                                        ast,
                                        content,
                                        contentLength,
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

TZrBool ZrLanguageServer_Lsp_TryResolveReceiverProjectMember(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrString *uri,
                                                             SZrAstNode *ast,
                                                             const TZrChar *content,
                                                             TZrSize contentLength,
                                                             TZrSize cursorOffset,
                                                             SZrLspResolvedMetadataMember *outResolved) {
    TZrSize memberStart;
    TZrSize memberEnd;
    TZrSize receiverStart;
    TZrSize receiverEnd;
    TZrChar receiverTypeText[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar receiverNameText[ZR_LSP_TYPE_BUFFER_LENGTH];
    SZrString *receiverName = ZR_NULL;
    SZrSymbol *receiverSymbol = ZR_NULL;
    SZrString *typeName = ZR_NULL;
    SZrString *memberName = ZR_NULL;
    SZrTypePrototypeInfo *prototype = ZR_NULL;
    const SZrTypeMemberInfo *memberInfo = ZR_NULL;
    SZrAstNode *typeDeclaration = ZR_NULL;
    SZrAstNode *memberDeclaration = ZR_NULL;
    EZrLspMetadataMemberKind memberDeclarationKind = ZR_LSP_METADATA_MEMBER_NONE;
    SZrFileRange declarationRange;
    SZrString *declarationUri;
    SZrLspProjectFileRecord *sourceRecord = ZR_NULL;
    SZrLspMetadataProvider provider;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL ||
        uri == ZR_NULL || content == ZR_NULL || cursorOffset >= contentLength || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

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
                                        uri,
                                        ast,
                                        content,
                                        contentLength,
                                        receiverName,
                                        receiverNameText,
                                        strlen(receiverNameText),
                                        cursorOffset,
                                        receiverTypeText,
                                        sizeof(receiverTypeText)) ||
            !receiver_type_text_is_specific(receiverTypeText)) {
            return ZR_FALSE;
        }
    }

    if (receiverName != ZR_NULL) {
        receiverSymbol = lookup_receiver_symbol_at_offset(analyzer,
                                                          uri,
                                                          content,
                                                          contentLength,
                                                          cursorOffset,
                                                          receiverName);
    }

    if ((receiverSymbol != ZR_NULL && receiver_symbol_is_type_declaration(receiverSymbol)) ||
        (receiverSymbol == ZR_NULL && receiverName != ZR_NULL &&
         receiver_name_is_explicit_type_binding(analyzer, receiverName))) {
        typeName = receiverName;
    } else {
        typeName = ZrCore_String_Create(state, (TZrNativeString)receiverTypeText, strlen(receiverTypeText));
    }
    memberName = ZrCore_String_Create(state,
                                      (TZrNativeString)(content + memberStart),
                                      memberEnd - memberStart);
    if (typeName == ZR_NULL || memberName == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = find_compiler_type_prototype_inference(analyzer->compilerState, typeName);
    if (prototype != ZR_NULL && prototype->name != ZR_NULL && !prototype->isImportedNative &&
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE &&
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        memberInfo = find_receiver_project_member_recursive(analyzer->compilerState, prototype, memberName, 0);
    }

    typeDeclaration = receiver_project_find_type_declaration_recursive(ast,
                                                                       prototype != ZR_NULL ? prototype->name : typeName);
    memberDeclaration = receiver_project_find_type_member_declaration(typeDeclaration,
                                                                      memberName,
                                                                      &memberDeclarationKind);
    if (memberDeclaration != ZR_NULL && memberDeclarationKind != ZR_LSP_METADATA_MEMBER_NONE) {
        outResolved->memberName = memberName;
        outResolved->memberKind = memberDeclarationKind;
        outResolved->ownerTypeName = prototype != ZR_NULL ? prototype->name : typeName;

        if (memberDeclaration->type == ZR_AST_ENUM_MEMBER) {
            declarationRange = receiver_project_member_declaration_range(uri,
                                                                         content,
                                                                         contentLength,
                                                                         memberDeclaration,
                                                                         memberName);
        } else {
            declarationRange = receiver_project_member_lookup_range(memberDeclaration);
            if (declarationRange.source == ZR_NULL) {
                declarationRange.source = uri;
            }
        }
        declarationUri = declarationRange.source != ZR_NULL ? declarationRange.source : uri;
        if (declarationUri != ZR_NULL) {
            declarationRange.source = declarationUri;
            if (projectIndex != ZR_NULL) {
                sourceRecord = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, declarationUri);
                outResolved->module.projectIndex = projectIndex;
                outResolved->module.sourceRecord = sourceRecord;
                outResolved->module.sourceKind = receiver_project_member_source_kind(analyzer,
                                                                                     sourceRecord,
                                                                                     memberDeclaration,
                                                                                     declarationRange);
                if (sourceRecord != ZR_NULL) {
                    outResolved->module.moduleName = sourceRecord->moduleName;
                }
            }
            if (outResolved->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED) {
                outResolved->module.sourceKind = receiver_project_member_source_kind(analyzer,
                                                                                     sourceRecord,
                                                                                     memberDeclaration,
                                                                                     declarationRange);
            }
            outResolved->declarationAnalyzer = analyzer;
            outResolved->declarationUri = declarationUri;
            outResolved->declarationRange = declarationRange;
            outResolved->hasDeclaration = ZR_TRUE;
            outResolved->declarationSymbol =
                ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, declarationRange);
            receiver_project_set_type_text_from_symbol(state, outResolved->declarationSymbol, outResolved);
        }
    }

    if (memberInfo != ZR_NULL &&
        !outResolved->hasDeclaration &&
        receiver_project_member_kind(memberInfo) != ZR_LSP_METADATA_MEMBER_NONE) {
        outResolved->memberName = memberName;
        outResolved->memberKind = receiver_project_member_kind(memberInfo);
        outResolved->ownerTypeName = prototype != ZR_NULL ? prototype->name : typeName;
        receiver_project_member_set_type_text(state, memberInfo, outResolved);

        if (memberInfo->declarationNode != ZR_NULL) {
            declarationRange = receiver_project_member_lookup_range(memberInfo->declarationNode);
            declarationUri = declarationRange.source;
            if (declarationUri == ZR_NULL) {
                declarationUri = uri;
                declarationRange.source = uri;
            }
            if (declarationUri != ZR_NULL) {
                if (projectIndex != ZR_NULL) {
                    sourceRecord = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, declarationUri);
                    outResolved->module.projectIndex = projectIndex;
                    outResolved->module.sourceRecord = sourceRecord;
                    outResolved->module.sourceKind = receiver_project_member_source_kind(analyzer,
                                                                                         sourceRecord,
                                                                                         memberInfo->declarationNode,
                                                                                         declarationRange);
                    if (sourceRecord != ZR_NULL) {
                        outResolved->module.moduleName = sourceRecord->moduleName;
                    }
                }
                if (outResolved->module.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED) {
                    outResolved->module.sourceKind = receiver_project_member_source_kind(analyzer,
                                                                                         sourceRecord,
                                                                                         memberInfo->declarationNode,
                                                                                         declarationRange);
                }
                outResolved->declarationAnalyzer = analyzer;
                outResolved->declarationUri = declarationUri;
                outResolved->declarationRange = declarationRange;
                outResolved->hasDeclaration = ZR_TRUE;
                outResolved->declarationSymbol =
                    ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, declarationRange);
                receiver_project_set_type_text_from_symbol(state, outResolved->declarationSymbol, outResolved);
            }
        }
    }

    if (!outResolved->hasDeclaration &&
        context != ZR_NULL &&
        projectIndex != ZR_NULL) {
        ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
        ZrLanguageServer_LspMetadataProvider_ResolveProjectTypeMemberDeclaration(&provider,
                                                                                 projectIndex,
                                                                                 typeName,
                                                                                 memberName,
                                                                                 outResolved);
    }

    if (outResolved->ownerTypeName == ZR_NULL && receiverTypeText[0] != '\0') {
        outResolved->ownerTypeName = ZrCore_String_Create(state,
                                                          (TZrNativeString)receiverTypeText,
                                                          strlen(receiverTypeText));
    }

    return outResolved->memberName != ZR_NULL &&
           outResolved->memberKind != ZR_LSP_METADATA_MEMBER_NONE;
}

static void append_type_prototype_member_completions(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     const SZrTypePrototypeInfo *prototype,
                                                     TZrBool wantStatic,
                                                     TZrSize depth,
                                                     SZrArray *result) {
    SZrArray membersSnapshot;
    SZrArray inheritsSnapshot;
    SZrString *prototypeName;
    SZrString *extendsTypeName;

    if (state == ZR_NULL || analyzer == ZR_NULL || prototype == ZR_NULL || result == ZR_NULL ||
        depth > ZR_LSP_MEMBER_RECURSION_MAX_DEPTH) {
        return;
    }

    membersSnapshot = prototype->members;
    inheritsSnapshot = prototype->inherits;
    prototypeName = prototype->name;
    extendsTypeName = prototype->extendsTypeName;

    if (prototype_array_layout_matches(&membersSnapshot, sizeof(SZrTypeMemberInfo))) {
        for (TZrSize index = 0; index < membersSnapshot.length; index++) {
            const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, index);
            const TZrChar *kind = ZR_NULL;
            SZrString *displayName = ZR_NULL;

            if (member == ZR_NULL || member->name == ZR_NULL || member->isMetaMethod ||
                member->isStatic != wantStatic) {
                continue;
            }

            displayName = completion_type_member_display_name(state, member, &kind);
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
                    if (kind == ZR_NULL) {
                        kind = "method";
                    }
                    break;
                default:
                    break;
            }

            if (kind != ZR_NULL && displayName != ZR_NULL) {
                append_completion_item_for_symbol_name(state, result, displayName, kind);
            }
        }
    }

    if (extendsTypeName != ZR_NULL) {
        const SZrTypePrototypeInfo *basePrototype =
            find_type_prototype_by_text(analyzer, ZrCore_String_GetNativeString(extendsTypeName));
        if (basePrototype != ZR_NULL && !type_prototype_matches_name(basePrototype, prototypeName)) {
            append_type_prototype_member_completions(state,
                                                     analyzer,
                                                     basePrototype,
                                                     wantStatic,
                                                     depth + 1,
                                                     result);
        }
    }

    if (prototype_array_layout_matches(&inheritsSnapshot, sizeof(SZrString *))) {
        for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
            SZrString **inheritPtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
            const SZrTypePrototypeInfo *basePrototype;

            if (inheritPtr == ZR_NULL || *inheritPtr == ZR_NULL) {
                continue;
            }

            basePrototype = find_type_prototype_by_text(analyzer, ZrCore_String_GetNativeString(*inheritPtr));
            if (basePrototype != ZR_NULL && !type_prototype_matches_name(basePrototype, prototypeName)) {
                append_type_prototype_member_completions(state,
                                                         analyzer,
                                                         basePrototype,
                                                         wantStatic,
                                                         depth + 1,
                                                         result);
            }
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
        if (result->length > 0) {
            return ZR_TRUE;
        }
    }

    if (append_type_symbol_member_completions_by_name(state, analyzer, typeText, wantStatic, result)) {
        return ZR_TRUE;
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
        if (result->length > 0) {
            ZrParser_InferredType_Free(state, &inferredType);
            return ZR_TRUE;
        }
    }

    if (append_type_symbol_member_completions_by_name(state, analyzer, typeText, wantStatic, result)) {
        ZrParser_InferredType_Free(state, &inferredType);
        return ZR_TRUE;
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

static TZrBool append_imported_type_receiver_completions(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrLspProjectIndex *projectIndex,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNode *ast,
                                                         SZrString *uri,
                                                         const TZrChar *content,
                                                         TZrSize contentLength,
                                                         TZrSize receiverStart,
                                                         SZrArray *result) {
    SZrArray bindings;
    SZrFilePosition queryPosition;
    SZrFileRange queryRange;
    SZrLspSemanticImportChainHit hit;
    const SZrTypePrototypeInfo *prototype;
    SZrSemanticAnalyzer *declarationAnalyzer;
    SZrSymbol *declarationSymbol;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL ||
        uri == ZR_NULL || content == ZR_NULL || result == ZR_NULL || receiverStart >= contentLength) {
        return ZR_FALSE;
    }

    if (projectIndex == ZR_NULL) {
        projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    }
    if (projectIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, ast, &bindings);

    queryPosition = lsp_interface_support_file_position_from_offset(content, contentLength, receiverStart);
    queryRange = ZrParser_FileRange_Create(queryPosition, queryPosition, uri);
    memset(&hit, 0, sizeof(hit));
    if (!ZrLanguageServer_LspSemanticImportChain_ResolveAtRange(state,
                                                                context,
                                                                projectIndex,
                                                                analyzer,
                                                                &bindings,
                                                                queryRange,
                                                                &hit) ||
        hit.resolvedMember.memberKind != ZR_LSP_METADATA_MEMBER_TYPE) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return ZR_FALSE;
    }

    declarationAnalyzer = hit.resolvedMember.declarationAnalyzer;
    declarationSymbol = hit.resolvedMember.declarationSymbol;
    if (declarationAnalyzer == ZR_NULL &&
        hit.resolvedMember.module.sourceRecord != ZR_NULL &&
        hit.resolvedMember.module.sourceRecord->uri != ZR_NULL) {
        declarationAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state,
                                                                       context,
                                                                       hit.resolvedMember.module.sourceRecord->uri);
    }
    if (declarationSymbol == ZR_NULL &&
        declarationAnalyzer != ZR_NULL &&
        declarationAnalyzer->symbolTable != ZR_NULL &&
        hit.resolvedMember.memberName != ZR_NULL) {
        declarationSymbol = ZrLanguageServer_SymbolTable_Lookup(declarationAnalyzer->symbolTable,
                                                                hit.resolvedMember.memberName,
                                                                ZR_NULL);
    }
    if (declarationAnalyzer == ZR_NULL || declarationSymbol == ZR_NULL) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return ZR_FALSE;
    }

    if (declarationSymbol->type == ZR_SYMBOL_CLASS) {
        append_class_member_completions_recursive(state,
                                                  declarationAnalyzer,
                                                  declarationSymbol,
                                                  ZR_TRUE,
                                                  0,
                                                  result);
        appended = result->length > 0;
    } else if (declarationSymbol->type == ZR_SYMBOL_STRUCT &&
               declarationSymbol->name != ZR_NULL) {
        prototype = find_type_prototype_by_text(declarationAnalyzer,
                                                ZrCore_String_GetNativeString(declarationSymbol->name));
        if (prototype != ZR_NULL) {
            append_type_prototype_member_completions(state,
                                                     declarationAnalyzer,
                                                     prototype,
                                                     ZR_TRUE,
                                                     0,
                                                     result);
            appended = result->length > 0;
        } else {
            append_struct_member_completions_recursive(state,
                                                       declarationAnalyzer,
                                                       declarationSymbol,
                                                       ZR_TRUE,
                                                       0,
                                                       result);
            appended = result->length > 0;
        }
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
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
                hasType = ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(analyzer,
                                                                                          ZR_NULL,
                                                                                          ZR_NULL,
                                                                                          varDecl->typeInfo,
                                                                                          &inferredType);
            } else if (varDecl->value != ZR_NULL && varDecl->value->type == ZR_AST_CONSTRUCT_EXPRESSION) {
                prototype = resolve_construct_target_prototype_from_node(
                        analyzer,
                        varDecl->value->data.constructExpression.target,
                        typeBuffer,
                        sizeof(typeBuffer));
                if (prototype != ZR_NULL) {
                    typeText = prototype->name != ZR_NULL ? ZrCore_String_GetNativeString(prototype->name) : typeBuffer;
                } else if (typeBuffer[0] != '\0') {
                    typeText = typeBuffer;
                }
            } else if (varDecl->value != ZR_NULL) {
                hasType = ZrParser_ExpressionType_Infer(analyzer->compilerState,
                                                        varDecl->value,
                                                        &inferredType);
            }

            if (!hasType) {
                if (varDecl->value != ZR_NULL && varDecl->value->type == ZR_AST_CONSTRUCT_EXPRESSION) {
                    prototype = resolve_construct_target_prototype_from_node(
                            analyzer,
                            varDecl->value->data.constructExpression.target,
                            typeBuffer,
                            sizeof(typeBuffer));
                    if (prototype != ZR_NULL) {
                        typeText =
                                prototype->name != ZR_NULL ? ZrCore_String_GetNativeString(prototype->name) : typeBuffer;
                    } else if (typeBuffer[0] != '\0') {
                        typeText = typeBuffer;
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
                prototype = resolve_construct_target_prototype_from_node(analyzer,
                                                                         varDecl->value->data.constructExpression.target,
                                                                         typeBuffer,
                                                                         sizeof(typeBuffer));
                if (prototype != ZR_NULL) {
                    typeText = prototype->name != ZR_NULL ? ZrCore_String_GetNativeString(prototype->name) : typeBuffer;
                } else if (typeBuffer[0] != '\0') {
                    typeText = typeBuffer;
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

            if (varDecl->value->data.constructExpression.target != ZR_NULL) {
                SZrString *className =
                    extract_construct_target_type_name_from_node(varDecl->value->data.constructExpression.target);
                SZrSymbol *classSymbol = className != ZR_NULL
                                             ? ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable,
                                                                                   className,
                                                                                   ZR_NULL)
                                             : ZR_NULL;
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
                                                           SZrLspContext *context,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrString *uri,
                                                           SZrAstNode *ast,
                                                           const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrSize cursorOffset,
                                                           SZrArray *result) {
    TZrSize memberDotOffset;
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
        result == ZR_NULL || cursorOffset > contentLength) {
        return ZR_FALSE;
    }

    if (cursorOffset < contentLength && content[cursorOffset] == '.') {
        memberDotOffset = cursorOffset;
    } else if (cursorOffset > 0 && content[cursorOffset - 1] == '.') {
        memberDotOffset = cursorOffset - 1;
    } else {
        return ZR_FALSE;
    }

    receiverEnd = memberDotOffset;
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

    receiverSymbol = lookup_receiver_symbol_at_offset(analyzer,
                                                      uri,
                                                      content,
                                                      contentLength,
                                                      cursorOffset,
                                                      receiverName);
    if (receiverSymbol != ZR_NULL && receiverSymbol->type == ZR_SYMBOL_CLASS) {
        classSymbol = receiverSymbol;
        wantStatic = ZR_TRUE;
    } else if (receiverSymbol == ZR_NULL &&
               receiver_name_is_explicit_type_binding(analyzer, receiverName)) {
        wantStatic = ZR_TRUE;
        if (append_receiver_explicit_type_binding_completions(state,
                                                              projectIndex,
                                                              analyzer,
                                                              ast,
                                                              receiverName,
                                                               result)) {
            return ZR_TRUE;
        }
    }

    if (receiverSymbol == ZR_NULL &&
        append_imported_type_receiver_completions(state,
                                                  context,
                                                  projectIndex,
                                                  analyzer,
                                                  ast,
                                                  uri,
                                                  content,
                                                  contentLength,
                                                  receiverStart,
                                                  result)) {
        return ZR_TRUE;
    }

    if (!wantStatic) {
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
        if (receiverTypeName[0] != '\0') {
            receiverPrototype = find_type_prototype_by_text(analyzer, receiverTypeName);
        }
        if (receiverTypeName[0] != '\0' &&
            (receiverPrototype == ZR_NULL || receiverPrototype->isImportedNative) &&
            append_receiver_native_type_completions(state,
                                                    projectIndex,
                                                    analyzer,
                                                    ast,
                                                    receiverTypeName,
                                                    ZR_FALSE,
                                                    result)) {
            return ZR_TRUE;
        }
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
        if (receiverTypeName[0] != '\0' &&
            append_type_symbol_member_completions_by_name(state,
                                                          analyzer,
                                                          receiverTypeName,
                                                          ZR_FALSE,
                                                          result)) {
            return ZR_TRUE;
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
            if (receiverTypeName[0] != '\0' &&
                append_type_symbol_member_completions_by_name(state,
                                                              analyzer,
                                                              receiverTypeName,
                                                              ZR_FALSE,
                                                              result)) {
                return ZR_TRUE;
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

    if (append_receiver_symbol_type_completions(state,
                                                projectIndex,
                                                analyzer,
                                                ast,
                                                receiverSymbol,
                                                wantStatic,
                                                result)) {
        return ZR_TRUE;
    } else if (append_receiver_name_type_env_completions(state,
                                                         projectIndex,
                                                         analyzer,
                                                         ast,
                                                         receiverName,
                                                         wantStatic,
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
        if (receiverTypeName[0] != '\0') {
            receiverPrototype = find_type_prototype_by_text(analyzer, receiverTypeName);
        }
        if (receiverTypeName[0] != '\0' &&
            (receiverPrototype == ZR_NULL || receiverPrototype->isImportedNative) &&
            append_receiver_native_type_completions(state,
                                                    projectIndex,
                                                    analyzer,
                                                    ast,
                                                    receiverTypeName,
                                                    ZR_FALSE,
                                                    result)) {
            return ZR_TRUE;
        }
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
            append_type_symbol_member_completions_by_name(state,
                                                          analyzer,
                                                          receiverTypeName,
                                                          ZR_FALSE,
                                                          result)) {
            return ZR_TRUE;
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
            if (receiverTypeName[0] != '\0' &&
                append_type_symbol_member_completions_by_name(state,
                                                              analyzer,
                                                              receiverTypeName,
                                                              ZR_FALSE,
                                                              result)) {
                return ZR_TRUE;
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
            if (append_imported_type_receiver_completions(state,
                                                          context,
                                                          projectIndex,
                                                          analyzer,
                                                          ast,
                                                          uri,
                                                          content,
                                                          contentLength,
                                                          receiverStart,
                                                          result)) {
                return ZR_TRUE;
            }

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
