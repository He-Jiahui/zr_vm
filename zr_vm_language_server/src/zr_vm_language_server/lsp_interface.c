//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

static TZrInt32 file_line_to_lsp_line(TZrInt32 fileLine) {
    return fileLine > 0 ? fileLine - 1 : 0;
}

static TZrInt32 lsp_line_to_file_line(TZrInt32 lspLine) {
    return lspLine + 1;
}

static TZrInt32 file_column_to_lsp_character(TZrInt32 fileColumn) {
    return fileColumn > 0 ? fileColumn - 1 : 0;
}

static TZrInt32 lsp_character_to_file_column(TZrInt32 lspCharacter) {
    return lspCharacter + 1;
}

// 转换 FileRange 到 LspRange
SZrLspRange ZrLanguageServer_LspRange_FromFileRange(SZrFileRange fileRange) {
    SZrLspRange lspRange;
    lspRange.start.line = file_line_to_lsp_line(fileRange.start.line);
    lspRange.start.character = file_column_to_lsp_character(fileRange.start.column);
    lspRange.end.line = file_line_to_lsp_line(fileRange.end.line);
    lspRange.end.character = file_column_to_lsp_character(fileRange.end.column);
    return lspRange;
}

// 辅助函数：从行号和列号计算偏移量
static TZrSize calculate_offset_from_line_column(const TZrChar *content, TZrSize contentLength, 
                                                   TZrInt32 line, TZrInt32 column) {
    if (content == ZR_NULL) {
        return 0;
    }
    
    TZrSize offset = 0;
    TZrInt32 currentLine = 0;
    TZrInt32 currentColumn = 0;
    
    // 遍历内容直到到达目标行和列
    for (TZrSize i = 0; i < contentLength && currentLine < line; i++) {
        if (content[i] == '\n') {
            currentLine++;
            currentColumn = 0;
            offset = i + 1; // 跳过换行符
        } else {
            currentColumn++;
        }
    }
    
    // 如果已经到达目标行，添加列偏移
    if (currentLine == line) {
        // 在当前行中查找列位置
        TZrSize lineStart = offset;
        for (TZrSize i = lineStart; i < contentLength && currentColumn < column; i++) {
            if (content[i] == '\n') {
                break; // 到达行尾
            }
            currentColumn++;
            offset = i + 1;
        }
    }
    
    return offset;
}

// 转换 LspRange 到 FileRange
SZrFileRange ZrLanguageServer_LspRange_ToFileRange(SZrLspRange lspRange, SZrString *uri) {
    SZrFileRange fileRange;
    fileRange.start.line = lsp_line_to_file_line(lspRange.start.line);
    fileRange.start.column = lsp_character_to_file_column(lspRange.start.character);
    fileRange.start.offset = 0; // 需要文件内容才能计算，暂时设为0
    fileRange.end.line = lsp_line_to_file_line(lspRange.end.line);
    fileRange.end.column = lsp_character_to_file_column(lspRange.end.character);
    fileRange.end.offset = 0; // TODO: 需要文件内容才能计算，暂时设为0
    fileRange.source = uri;
    return fileRange;
}

// 转换 LspRange 到 FileRange（带文件内容）
SZrFileRange ZrLanguageServer_LspRange_ToFileRangeWithContent(SZrLspRange lspRange, SZrString *uri, 
                                                const TZrChar *content, TZrSize contentLength) {
    SZrFileRange fileRange;
    fileRange.start.line = lsp_line_to_file_line(lspRange.start.line);
    fileRange.start.column = lsp_character_to_file_column(lspRange.start.character);
    fileRange.start.offset = calculate_offset_from_line_column(content, contentLength, 
                                                                 lspRange.start.line,
                                                                 lspRange.start.character);
    fileRange.end.line = lsp_line_to_file_line(lspRange.end.line);
    fileRange.end.column = lsp_character_to_file_column(lspRange.end.character);
    fileRange.end.offset = calculate_offset_from_line_column(content, contentLength, 
                                                              lspRange.end.line,
                                                              lspRange.end.character);
    fileRange.source = uri;
    return fileRange;
}

// 转换 FilePosition 到 LspPosition
SZrLspPosition ZrLanguageServer_LspPosition_FromFilePosition(SZrFilePosition filePosition) {
    SZrLspPosition lspPosition;
    lspPosition.line = file_line_to_lsp_line(filePosition.line);
    lspPosition.character = file_column_to_lsp_character(filePosition.column);
    return lspPosition;
}

// 转换 LspPosition 到 FilePosition
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePosition(SZrLspPosition lspPosition) {
    SZrFilePosition filePosition;
    filePosition.line = lsp_line_to_file_line(lspPosition.line);
    filePosition.column = lsp_character_to_file_column(lspPosition.character);
    filePosition.offset = 0; // TODO: 需要文件内容才能计算，暂时设为0
    return filePosition;
}

// 转换 LspPosition 到 FilePosition（带文件内容）
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                        const TZrChar *content, TZrSize contentLength) {
    SZrFilePosition filePosition;
    filePosition.line = lsp_line_to_file_line(lspPosition.line);
    filePosition.column = lsp_character_to_file_column(lspPosition.character);
    filePosition.offset = calculate_offset_from_line_column(content, contentLength, 
                                                              lspPosition.line, 
                                                              lspPosition.character);
    return filePosition;
}

// 创建 LSP 上下文
SZrLspContext *ZrLanguageServer_LspContext_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrLspContext *context = (SZrLspContext *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspContext));
    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    
    context->state = state;
    context->parser = ZrLanguageServer_IncrementalParser_New(state);
    context->analyzer = ZR_NULL; // 延迟创建，每个文件一个分析器
    context->uriToAnalyzerMap.buckets = ZR_NULL;
    context->uriToAnalyzerMap.bucketSize = 0;
    context->uriToAnalyzerMap.elementCount = 0;
    context->uriToAnalyzerMap.capacity = 0;
    context->uriToAnalyzerMap.resizeThreshold = 0;
    context->uriToAnalyzerMap.isValid = ZR_FALSE;
    ZrCore_HashSet_Init(state, &context->uriToAnalyzerMap, 4); // capacityLog2 = 4
    
    if (context->parser == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
        return ZR_NULL;
    }
    
    return context;
}

// 释放 LSP 上下文
void ZrLanguageServer_LspContext_Free(SZrState *state, SZrLspContext *context) {
    if (state == ZR_NULL || context == ZR_NULL) {
        return;
    }

    // 释放所有分析器
    if (context->uriToAnalyzerMap.isValid && context->uriToAnalyzerMap.buckets != ZR_NULL) {
        // 遍历哈希表释放所有分析器和节点
        for (TZrSize i = 0; i < context->uriToAnalyzerMap.capacity; i++) {
            SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[i];
            while (pair != ZR_NULL) {
                // 释放节点中存储的数据
                if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrSemanticAnalyzer *analyzer = 
                            (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                        if (analyzer != ZR_NULL) {
                            ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
                        }
                    }
                }
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrCore_Memory_RawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            context->uriToAnalyzerMap.buckets[i] = ZR_NULL;
        }
        // 释放 buckets 数组
        ZrCore_HashSet_Deconstruct(state, &context->uriToAnalyzerMap);
    }

    if (context->parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, context->parser);
    }

    if (context->analyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, context->analyzer);
    }

    ZrCore_Memory_RawFree(state->global, context, sizeof(SZrLspContext));
}

// 获取或创建分析器
static SZrSemanticAnalyzer *get_or_create_analyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从哈希表中查找
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        // 从原生指针中获取 SZrSemanticAnalyzer
        return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    }
    
    // 创建新分析器
    SZrSemanticAnalyzer *analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (analyzer != ZR_NULL) {
        // 添加到哈希表
        SZrHashKeyValuePair *newPair = ZrCore_HashSet_Add(state, &context->uriToAnalyzerMap, &key);
        if (newPair != ZR_NULL) {
            // 将 SZrSemanticAnalyzer 指针存储为原生指针
            SZrTypeValue value;
            ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)analyzer);
            ZrCore_Value_Copy(state, &newPair->value, &value);
        }
    }
    
    return analyzer;
}

static SZrSemanticAnalyzer *find_analyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        return (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
    }

    return ZR_NULL;
}

static void remove_analyzer(SZrState *state, SZrLspContext *context, SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    SZrTypeValue key;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    analyzer = find_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, &uri->super);
    ZrCore_HashSet_Remove(state, &context->uriToAnalyzerMap, &key);
    ZrLanguageServer_SemanticAnalyzer_Free(state, analyzer);
}

static SZrFileVersion *get_document_file_version(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
}

static SZrFilePosition get_document_file_position(SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position) {
    SZrFileVersion *fileVersion = get_document_file_version(context, uri);

    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        return ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                      fileVersion->content,
                                                                      fileVersion->contentLength);
    }

    return ZrLanguageServer_LspPosition_ToFilePosition(position);
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

static TZrBool strings_equal(SZrString *left, SZrString *right) {
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

static TZrBool string_contains_case_insensitive(SZrString *haystack, SZrString *needle) {
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

static SZrFileRange get_symbol_lookup_range(SZrSymbol *symbol) {
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

    return calculate_offset_from_line_column(content,
                                             contentLength,
                                             file_line_to_lsp_line(position.line),
                                             file_column_to_lsp_character(position.column));
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

static SZrString *build_symbol_markdown_documentation(SZrState *state,
                                                      SZrSymbol *symbol,
                                                      const TZrChar *content,
                                                      TZrSize contentLength) {
    TZrNativeString nameText;
    TZrSize nameLength;
    const TZrChar *kindText;
    TZrChar commentBuffer[1024];
    TZrChar markdownBuffer[2048];
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

static void enrich_completion_item_metadata(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrCompletionItem *item,
                                            const TZrChar *content,
                                            TZrSize contentLength) {
    SZrSymbol *symbol;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || item == ZR_NULL ||
        item->label == ZR_NULL) {
        return;
    }

    symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, item->label, ZR_NULL);
    if (symbol == ZR_NULL) {
        return;
    }

    if (item->detail == ZR_NULL) {
        const TZrChar *kindText = symbol_type_to_display_name(symbol->type);
        item->detail = ZrCore_String_Create(state, kindText, strlen(kindText));
    }

    if (item->documentation == ZR_NULL) {
        item->documentation = build_symbol_markdown_documentation(state,
                                                                  symbol,
                                                                  content,
                                                                  contentLength);
    }
}

static void append_lsp_diagnostic(SZrState *state, SZrArray *result, SZrDiagnostic *diag) {
    SZrLspDiagnostic *lspDiag;

    if (state == ZR_NULL || result == ZR_NULL || diag == ZR_NULL) {
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

static SZrLspSymbolInformation *create_symbol_information(SZrState *state, SZrSymbol *symbol) {
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
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL && strings_equal((*itemPtr)->label, label)) {
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

static TZrBool try_collect_receiver_completions(SZrState *state,
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

    receiverName = ZrCore_String_Create(state, content + receiverStart, receiverLength);
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
    if (!strings_equal(position.source, targetRange.source) &&
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

static SZrSymbol *find_symbol_at_usage_or_definition(SZrSemanticAnalyzer *analyzer, SZrFileRange position) {
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
            if (file_position_is_in_range(position, get_symbol_lookup_range(symbol))) {
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
TZrBool ZrLanguageServer_Lsp_UpdateDocument(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          const TZrChar *content,
                          TZrSize contentLength,
                          TZrSize version) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 更新文件
    if (!ZrLanguageServer_IncrementalParser_UpdateFile(state, context->parser, uri, content, contentLength, version)) {
        return ZR_FALSE;
    }
    
    // 重新解析
    if (!ZrLanguageServer_IncrementalParser_Parse(state, context->parser, uri)) {
        return ZR_FALSE;
    }
    
    {
        SZrFileVersion *fileVersion = get_document_file_version(context, uri);
        SZrSemanticAnalyzer *analyzer;
        SZrAstNode *ast;

        if (fileVersion == ZR_NULL) {
            return ZR_FALSE;
        }

        ast = fileVersion->ast;
        if (ast == ZR_NULL) {
            remove_analyzer(state, context, uri);
            return fileVersion->parserDiagnostics.length > 0;
        }

        analyzer = get_or_create_analyzer(state, context, uri);
        if (analyzer == ZR_NULL) {
            return ZR_FALSE;
        }

        return ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, ast);
    }
}

// 获取诊断
TZrBool ZrLanguageServer_Lsp_GetDiagnostics(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;
    SZrArray diagnostics;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDiagnostic *), 8);
    }
    
    fileVersion = get_document_file_version(context, uri);
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < fileVersion->parserDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&fileVersion->parserDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            append_lsp_diagnostic(state, result, *diagPtr);
        }
    }

    analyzer = find_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &diagnostics, sizeof(SZrDiagnostic *), 8);
    if (!ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(state, analyzer, &diagnostics)) {
        ZrCore_Array_Free(state, &diagnostics);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            append_lsp_diagnostic(state, result, *diagPtr);
        }
    }

    ZrCore_Array_Free(state, &diagnostics);
    return ZR_TRUE;
}

// 获取补全
TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspCompletionItem *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = get_document_file_position(context, uri, position);
    SZrFileRange fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);

    // 获取补全
    SZrArray completions;
    SZrFileVersion *fileVersion = get_document_file_version(context, uri);
    TZrBool hasReceiverCompletions = ZR_FALSE;
    ZrCore_Array_Init(state, &completions, sizeof(SZrCompletionItem *), 8);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL && analyzer->ast != ZR_NULL) {
        hasReceiverCompletions = try_collect_receiver_completions(state,
                                                                  analyzer,
                                                                  analyzer->ast,
                                                                  fileVersion->content,
                                                                  fileVersion->contentLength,
                                                                  filePos.offset,
                                                                  &completions);
    }
    if (!hasReceiverCompletions &&
        !ZrLanguageServer_SemanticAnalyzer_GetCompletions(state, analyzer, fileRange, &completions)) {
        ZrCore_Array_Free(state, &completions);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 补全项
    for (TZrSize i = 0; i < completions.length; i++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(&completions, i);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            SZrCompletionItem *item = *itemPtr;
            if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
                enrich_completion_item_metadata(state,
                                                analyzer,
                                                item,
                                                fileVersion->content,
                                                fileVersion->contentLength);
            }
            
            SZrLspCompletionItem *lspItem = (SZrLspCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspCompletionItem));
            if (lspItem != ZR_NULL) {
                lspItem->label = item->label;
                
                // 转换 kind 字符串到整数
                TZrInt32 kindValue = 1; // 默认 Text
                if (item->kind != ZR_NULL) {
                    TZrNativeString kindStr;
                    TZrSize kindLen;
                    if (item->kind->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                        kindStr = ZrCore_String_GetNativeStringShort(item->kind);
                        kindLen = item->kind->shortStringLength;
                    } else {
                        kindStr = ZrCore_String_GetNativeString(item->kind);
                        kindLen = item->kind->longStringLength;
                    }
                    
                    // LSP CompletionItemKind 映射
                    if (kindLen == 8 && memcmp(kindStr, "function", 8) == 0) {
                        kindValue = 3; // Function
                    } else if (kindLen == 5 && memcmp(kindStr, "class", 5) == 0) {
                        kindValue = 7; // Class
                    } else if (kindLen == 8 && memcmp(kindStr, "variable", 8) == 0) {
                        kindValue = 6; // Variable
                    } else if (kindLen == 6 && memcmp(kindStr, "struct", 6) == 0) {
                        kindValue = 22; // Struct
                    } else if (kindLen == 6 && memcmp(kindStr, "method", 6) == 0) {
                        kindValue = 2; // Method
                    } else if (kindLen == 9 && memcmp(kindStr, "interface", 9) == 0) {
                        kindValue = 8; // Interface
                    } else if (kindLen == 8 && memcmp(kindStr, "property", 8) == 0) {
                        kindValue = 10; // Property
                    } else if (kindLen == 5 && memcmp(kindStr, "field", 5) == 0) {
                        kindValue = 5; // Field
                    } else if (kindLen == 6 && memcmp(kindStr, "module", 6) == 0) {
                        kindValue = 9; // Module
                    } else if (kindLen == 4 && memcmp(kindStr, "enum", 4) == 0) {
                        kindValue = 13; // Enum
                    } else if (kindLen == 8 && memcmp(kindStr, "constant", 8) == 0) {
                        kindValue = 21; // Constant
                    } else {
                        kindValue = 1; // Text (默认)
                    }
                }
                lspItem->kind = kindValue;
                lspItem->detail = item->detail;
                lspItem->documentation = item->documentation;
                lspItem->insertText = item->label;
                lspItem->insertTextFormat = ZrCore_String_Create(state, "plaintext", 9);
                
                ZrCore_Array_Push(state, result, &lspItem);
            }
        }
    }
    
    ZrCore_Array_Free(state, &completions);
    return ZR_TRUE;
}

// 获取悬停信息
TZrBool ZrLanguageServer_Lsp_GetHover(SZrState *state,
                    SZrLspContext *context,
                    SZrString *uri,
                    SZrLspPosition position,
                    SZrLspHover **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrFileVersion *fileVersion;
    SZrSymbol *symbol;
    SZrString *content;
    SZrHoverInfo *hoverInfo = ZR_NULL;
    SZrLspHover *lspHover;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取分析器
    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    filePos = get_document_file_position(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    fileVersion = get_document_file_version(context, uri);
    symbol = find_symbol_at_usage_or_definition(analyzer, fileRange);

    if (symbol != ZR_NULL && fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        content = build_symbol_markdown_documentation(state,
                                                      symbol,
                                                      fileVersion->content,
                                                      fileVersion->contentLength);
        if (content == ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        if (!ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(state, analyzer, fileRange, &hoverInfo) ||
            hoverInfo == ZR_NULL) {
            return ZR_FALSE;
        }
        content = hoverInfo->contents;
    }
    
    // 转换为 LSP 悬停
    lspHover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (lspHover == ZR_NULL) {
        if (hoverInfo != ZR_NULL) {
            ZrLanguageServer_HoverInfo_Free(state, hoverInfo);
        }
        return ZR_FALSE;
    }
    
    ZrCore_Array_Init(state, &lspHover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &lspHover->contents, &content);
    lspHover->range = ZrLanguageServer_LspRange_FromFileRange(
        symbol != ZR_NULL ? get_symbol_lookup_range(symbol) : hoverInfo->range);
    
    *result = lspHover;
    return ZR_TRUE;
}

// 获取定义位置
TZrBool ZrLanguageServer_Lsp_GetDefinition(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 1);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = get_document_file_position(context, uri, position);
    SZrFileRange fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = find_symbol_at_usage_or_definition(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建位置
    SZrLspLocation *location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }
    
    location->uri = symbol->location.source;
    location->range = ZrLanguageServer_LspRange_FromFileRange(get_symbol_lookup_range(symbol));
    
    ZrCore_Array_Push(state, result, &location);
    
    return ZR_TRUE;
}

// 查找引用
TZrBool ZrLanguageServer_Lsp_FindReferences(SZrState *state,
                          SZrLspContext *context,
                          SZrString *uri,
                          SZrLspPosition position,
                          TZrBool includeDeclaration,
                          SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = get_document_file_position(context, uri, position);
    SZrFileRange fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = find_symbol_at_usage_or_definition(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取所有引用
    SZrArray references;
    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 8);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 位置
    for (TZrSize i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            
            // 如果是定义引用且不包含定义，跳过
            if (ref->type == ZR_REFERENCE_DEFINITION && !includeDeclaration) {
                continue;
            }
            
            SZrLspLocation *location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
            if (location != ZR_NULL) {
                location->uri = ref->location.source;
                location->range = ZrLanguageServer_LspRange_FromFileRange(ref->location);
                
                ZrCore_Array_Push(state, result, &location);
            }
        }
    }
    
    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

// 重命名符号
TZrBool ZrLanguageServer_Lsp_Rename(SZrState *state,
                  SZrLspContext *context,
                  SZrString *uri,
                  SZrLspPosition position,
                  SZrString *newName,
                  SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || newName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 8);
    }
    
    // 获取分析器
    SZrSemanticAnalyzer *analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 转换位置
    SZrFilePosition filePos = get_document_file_position(context, uri, position);
    SZrFileRange fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    
    // 查找符号
    SZrSymbol *symbol = find_symbol_at_usage_or_definition(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取所有引用（包括定义）
    SZrArray references;
    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 8);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }
    
    // 转换为 LSP 位置（所有需要重命名的位置）
    for (TZrSize i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            
            SZrLspLocation *location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
            if (location != ZR_NULL) {
                location->uri = ref->location.source;
                location->range = ZrLanguageServer_LspRange_FromFileRange(ref->location);
                
                ZrCore_Array_Push(state, result, &location);
            }
        }
    }
    
    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetDocumentSymbols(SZrState *state,
                              SZrLspContext *context,
                              SZrString *uri,
                              SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrSymbolScope *globalScope;
    TZrSize i;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), 8);
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    globalScope = analyzer->symbolTable->globalScope;
    if (globalScope == ZR_NULL) {
        return ZR_TRUE;
    }

    for (i = 0; i < globalScope->symbols.length; i++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&globalScope->symbols, i);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            SZrSymbol *symbol = *symbolPtr;
            if (symbol->location.source != ZR_NULL && strings_equal(symbol->location.source, uri)) {
                SZrLspSymbolInformation *info = create_symbol_information(state, symbol);
                if (info != ZR_NULL) {
                    ZrCore_Array_Push(state, result, &info);
                }
            }
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetWorkspaceSymbols(SZrState *state,
                               SZrLspContext *context,
                               SZrString *query,
                               SZrArray *result) {
    TZrSize bucketIndex;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), 8);
    }

    if (!context->uriToAnalyzerMap.isValid || context->uriToAnalyzerMap.buckets == ZR_NULL) {
        return ZR_TRUE;
    }

    for (bucketIndex = 0; bucketIndex < context->uriToAnalyzerMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = context->uriToAnalyzerMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                if (analyzer != ZR_NULL && analyzer->symbolTable != ZR_NULL &&
                    analyzer->symbolTable->globalScope != ZR_NULL) {
                    TZrSize symbolIndex;
                    for (symbolIndex = 0; symbolIndex < analyzer->symbolTable->globalScope->symbols.length; symbolIndex++) {
                        SZrSymbol **symbolPtr =
                            (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, symbolIndex);
                        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                            SZrSymbol *symbol = *symbolPtr;
                            if (string_contains_case_insensitive(symbol->name, query)) {
                                SZrLspSymbolInformation *info = create_symbol_information(state, symbol);
                                if (info != ZR_NULL) {
                                    ZrCore_Array_Push(state, result, &info);
                                }
                            }
                        }
                    }
                }
            }
            pair = pair->next;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetDocumentHighlights(SZrState *state,
                                  SZrLspContext *context,
                                  SZrString *uri,
                                  SZrLspPosition position,
                                  SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrSymbol *symbol;
    SZrLspDocumentHighlight *highlight;
    SZrArray references;
    TZrSize i;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), 8);
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = get_document_file_position(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    symbol = find_symbol_at_usage_or_definition(analyzer, fileRange);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight != ZR_NULL) {
        highlight->range = ZrLanguageServer_LspRange_FromFileRange(get_symbol_lookup_range(symbol));
        highlight->kind = 3;
        ZrCore_Array_Push(state, result, &highlight);
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), 8);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_TRUE;
    }

    for (i = 0; i < references.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&references, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            if (ref->location.source != ZR_NULL && strings_equal(ref->location.source, uri)) {
                SZrLspDocumentHighlight *item =
                    (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
                if (item != ZR_NULL) {
                    item->range = ZrLanguageServer_LspRange_FromFileRange(ref->location);
                    item->kind = ref->type == ZR_REFERENCE_WRITE ? 3 : 2;
                    ZrCore_Array_Push(state, result, &item);
                }
            }
        }
    }

    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_PrepareRename(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrLspPosition position,
                         SZrLspRange *outRange,
                         SZrString **outPlaceholder) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrSymbol *symbol;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outRange == ZR_NULL ||
        outPlaceholder == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = get_or_create_analyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = get_document_file_position(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    symbol = find_symbol_at_usage_or_definition(analyzer, fileRange);
    if (symbol == ZR_NULL || symbol->name == ZR_NULL) {
        return ZR_FALSE;
    }

    *outRange = ZrLanguageServer_LspRange_FromFileRange(get_symbol_lookup_range(symbol));
    *outPlaceholder = symbol->name;
    return ZR_TRUE;
}
