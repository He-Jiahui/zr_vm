#include "lsp_editor_features_internal.h"
#include "semantic/lsp_semantic_implementation_query.h"
#include "zr_vm_parser/legacy_migration.h"

#include <stdlib.h>
#include <string.h>

/* 统一编辑结果的 VM 字符串创建路径，供 CodeLens、链接和 code action 使用。 */
SZrString *lsp_editor_create_string(SZrState *state, const TZrChar *text, TZrSize length) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, (TZrNativeString)text, length);
}

/* 只定位增量解析器中的版本；调用者不能把该句柄当成内容的长期所有权。 */
SZrFileVersion *lsp_editor_get_file_version(SZrLspContext *context, SZrString *uri) {
    if (context == ZR_NULL || context->parser == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLanguageServer_IncrementalParser_GetFileVersion(context->parser, uri);
}

/* 格式化和选择范围从同一版本的 owned 内容取数，所有成功路径都须配对 Free。 */
static TZrBool lsp_editor_acquire_content_snapshot(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri,
                                                   SZrFileVersionContentSnapshot *outSnapshot) {
    SZrFileVersion *fileVersion;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = lsp_editor_get_file_version(context, uri);
    return ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, outSnapshot);
}

/* 格式化入口复用解析器迁移计划过滤已登记的旧语法，避免自动改写这些输入。 */
static TZrBool lsp_editor_source_is_current_syntax(
        SZrState *state,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        TZrBool *outIsCurrent) {
    SZrLegacyMigrationPlan plan = {0};

    if (state == ZR_NULL || uri == ZR_NULL || content == ZR_NULL ||
        outIsCurrent == ZR_NULL) {
        return ZR_FALSE;
    }
    *outIsCurrent = ZR_FALSE;
    if (!ZrParser_LegacyMigration_PlanSource(
                state, content, contentLength, uri, &plan)) {
        return ZR_FALSE;
    }
    *outIsCurrent = plan.items.length == 0U;
    ZrParser_LegacyMigration_PlanFree(state, &plan);
    return ZR_TRUE;
}

/* 格式化、折叠和编辑提案共用此投影，输入偏移必须属于同一份快照。 */
/* BUG: 非 ASCII 字节被逐个计作 UTF-16 列；例如格式化“中 ”时全文编辑 end.character 得到 4 而非 2，默认 UTF-16 客户端会得到错位范围。 */
SZrLspPosition lsp_editor_position_from_offset(const TZrChar *content,
                                               TZrSize contentLength,
                                               TZrSize offset) {
    SZrLspPosition position = {0, 0};
    TZrSize limit = offset < contentLength ? offset : contentLength;

    for (TZrSize index = 0; index < limit; index++) {
        if (content[index] == '\n') {
            position.line++;
            position.character = 0;
        } else {
            position.character++;
        }
    }

    return position;
}

/* 按整行规划编辑的入口先对齐行首；越界请求被限制在文末。 */
TZrSize lsp_editor_line_start_offset(const TZrChar *content,
                                     TZrSize contentLength,
                                     TZrInt32 line) {
    TZrInt32 currentLine = 0;

    if (content == ZR_NULL || line <= 0) {
        return 0;
    }

    for (TZrSize index = 0; index < contentLength; index++) {
        if (content[index] == '\n') {
            currentLine++;
            if (currentLine == line) {
                return index + 1;
            }
        }
    }

    return contentLength;
}

/* 与行首 helper 配对，保持替换区间不含 CRLF，除非上层显式扩到下一行。 */
TZrSize lsp_editor_line_end_offset(const TZrChar *content,
                                   TZrSize contentLength,
                                   TZrInt32 line) {
    TZrSize offset = lsp_editor_line_start_offset(content, contentLength, line);

    while (offset < contentLength && content[offset] != '\n') {
        offset++;
    }
    if (offset > 0 && content[offset - 1] == '\r') {
        offset--;
    }
    return offset;
}

/* 全文格式化返回单个替换编辑，范围须覆盖输入快照的整个可见文档。 */
static SZrLspRange lsp_editor_full_document_range(const TZrChar *content, TZrSize contentLength) {
    SZrLspRange range;
    range.start.line = 0;
    range.start.character = 0;
    range.end = lsp_editor_position_from_offset(content, contentLength, contentLength);
    return range;
}

/* 把字节范围交给编辑器协议；非 ASCII 列的已知偏差继承 position_from_offset。 */
SZrLspRange lsp_editor_range_from_offsets(const TZrChar *content,
                                          TZrSize contentLength,
                                          TZrSize startOffset,
                                          TZrSize endOffset) {
    SZrLspRange range;
    range.start = lsp_editor_position_from_offset(content, contentLength, startOffset);
    range.end = lsp_editor_position_from_offset(content, contentLength, endOffset);
    return range;
}

/* 临时格式化输出在转成 VM 字符串前使用 malloc 所有权，失败时保留旧缓冲。 */
static TZrBool lsp_text_builder_reserve(SZrLspTextBuilder *builder, TZrSize extra) {
    TZrSize required;
    TZrSize nextCapacity;
    TZrChar *nextData;

    if (builder == ZR_NULL) {
        return ZR_FALSE;
    }

    /* TODO: required 和倍增容量未检查 TZrSize 溢出；核对可接受的文档上界与分配器上限。 */
    required = builder->length + extra + 1;
    if (required <= builder->capacity) {
        return ZR_TRUE;
    }

    nextCapacity = builder->capacity > 0 ? builder->capacity : 128;
    while (nextCapacity < required) {
        nextCapacity *= 2;
    }

    nextData = (TZrChar *)realloc(builder->data, nextCapacity);
    if (nextData == ZR_NULL) {
        return ZR_FALSE;
    }

    builder->data = nextData;
    builder->capacity = nextCapacity;
    return ZR_TRUE;
}

/* 格式化和导入整理共用追加契约；零长度输入无需读取 text。 */
TZrBool lsp_text_builder_append_range(SZrLspTextBuilder *builder,
                                      const TZrChar *text,
                                      TZrSize length) {
    if (length == 0) {
        return ZR_TRUE;
    }
    if (builder == ZR_NULL || text == ZR_NULL || !lsp_text_builder_reserve(builder, length)) {
        return ZR_FALSE;
    }

    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
    return ZR_TRUE;
}

/* 单字符追加保持与文本追加相同的容量及失败处理。 */
TZrBool lsp_text_builder_append_char(SZrLspTextBuilder *builder, TZrChar value) {
    return lsp_text_builder_append_range(builder, &value, 1);
}

/* 将花括号作为仅在代码区有效的事件，供缩进和折叠调用者共享。 */
TZrBool lsp_editor_scan_structural_chars(const TZrChar *content,
                                         TZrSize contentLength,
                                         TZrSize startOffset,
                                         TZrSize endOffset,
                                         SZrLspEditorScanState *scanState,
                                         TZrLspEditorStructuralCharCallback callback,
                                         void *userData) {
    SZrLspEditorScanState localState = {ZR_LSP_EDITOR_SCAN_CODE, ZR_FALSE};
    SZrLspEditorScanState *state = scanState != ZR_NULL ? scanState : &localState;

    if (content == ZR_NULL || startOffset > contentLength) {
        return ZR_FALSE;
    }
    if (endOffset > contentLength) {
        endOffset = contentLength;
    }
    if (endOffset < startOffset) {
        endOffset = startOffset;
    }

    for (TZrSize index = startOffset; index < endOffset; index++) {
        TZrChar current = content[index];

        switch (state->mode) {
            case ZR_LSP_EDITOR_SCAN_CODE:
                if (current == '/' && index + 1 < endOffset && content[index + 1] == '/') {
                    state->mode = ZR_LSP_EDITOR_SCAN_LINE_COMMENT;
                    index++;
                } else if (current == '/' && index + 1 < endOffset && content[index + 1] == '*') {
                    state->mode = ZR_LSP_EDITOR_SCAN_BLOCK_COMMENT;
                    index++;
                } else if (current == '"') {
                    state->mode = ZR_LSP_EDITOR_SCAN_STRING;
                    state->escaped = ZR_FALSE;
                } else if (current == '\'') {
                    state->mode = ZR_LSP_EDITOR_SCAN_CHAR;
                    state->escaped = ZR_FALSE;
                } else if (current == '`') {
                    state->mode = ZR_LSP_EDITOR_SCAN_TEMPLATE_STRING;
                    state->escaped = ZR_FALSE;
                } else if ((current == '{' || current == '}') && callback != ZR_NULL) {
                    if (!callback(current, index, userData)) {
                        return ZR_FALSE;
                    }
                }
                break;

            case ZR_LSP_EDITOR_SCAN_LINE_COMMENT:
                if (current == '\n') {
                    state->mode = ZR_LSP_EDITOR_SCAN_CODE;
                    state->escaped = ZR_FALSE;
                }
                break;

            case ZR_LSP_EDITOR_SCAN_BLOCK_COMMENT:
                if (current == '*' && index + 1 < endOffset && content[index + 1] == '/') {
                    state->mode = ZR_LSP_EDITOR_SCAN_CODE;
                    state->escaped = ZR_FALSE;
                    index++;
                }
                break;

            case ZR_LSP_EDITOR_SCAN_STRING:
                if (state->escaped) {
                    state->escaped = ZR_FALSE;
                } else if (current == '\\') {
                    state->escaped = ZR_TRUE;
                } else if (current == '"' || current == '\n' || current == '\r') {
                    state->mode = ZR_LSP_EDITOR_SCAN_CODE;
                }
                break;

            case ZR_LSP_EDITOR_SCAN_CHAR:
                if (state->escaped) {
                    state->escaped = ZR_FALSE;
                } else if (current == '\\') {
                    state->escaped = ZR_TRUE;
                } else if (current == '\'' || current == '\n' || current == '\r') {
                    state->mode = ZR_LSP_EDITOR_SCAN_CODE;
                }
                break;

            case ZR_LSP_EDITOR_SCAN_TEMPLATE_STRING:
                if (state->escaped) {
                    state->escaped = ZR_FALSE;
                } else if (current == '\\') {
                    state->escaped = ZR_TRUE;
                } else if (current == '`') {
                    state->mode = ZR_LSP_EDITOR_SCAN_CODE;
                }
                break;
        }
    }

    return ZR_TRUE;
}

/* 文本搜索到候选后再核对词法上下文，避免向编辑器暴露注释/字面量中的假目标。 */
TZrBool lsp_editor_offset_is_code(const TZrChar *content,
                                  TZrSize contentLength,
                                  TZrSize offset) {
    SZrLspEditorScanState scanState = {ZR_LSP_EDITOR_SCAN_CODE, ZR_FALSE};

    if (content == ZR_NULL || offset > contentLength) {
        return ZR_FALSE;
    }
    if (!lsp_editor_scan_structural_chars(content,
                                          contentLength,
                                          0,
                                          offset,
                                          &scanState,
                                          ZR_NULL,
                                          ZR_NULL)) {
        return ZR_FALSE;
    }

    return scanState.mode == ZR_LSP_EDITOR_SCAN_CODE;
}

/* 当前格式化策略固定四空格缩进，调用方在此之前已确定代码块深度。 */
static TZrBool lsp_text_builder_append_indent(SZrLspTextBuilder *builder, TZrInt32 indentLevel) {
    for (TZrInt32 level = 0; level < indentLevel; level++) {
        if (!lsp_text_builder_append_range(builder, "    ", 4)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* code action 与格式化都经此处生成原生编辑；调用方随后用 FreeTextEdits 清理。 */
TZrBool lsp_editor_append_text_edit(SZrState *state,
                                    SZrArray *result,
                                    SZrLspRange range,
                                    const TZrChar *newText,
                                    TZrSize newTextLength) {
    SZrLspTextEdit *edit;

    if (state == ZR_NULL || result == ZR_NULL || newText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    edit = (SZrLspTextEdit *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspTextEdit));
    if (edit == ZR_NULL) {
        return ZR_FALSE;
    }

    edit->range = range;
    edit->newText = lsp_editor_create_string(state, newText, newTextLength);
    if (edit->newText == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, edit, sizeof(SZrLspTextEdit));
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, result, &edit);
    return ZR_TRUE;
}

/* 折叠式扫描回调的短期上下文；只在一次扫描调用期间借用 indent。 */
typedef struct SZrLspIndentScanData {
    TZrInt32 *indent;
} SZrLspIndentScanData;

/* 只消费代码区花括号，确保字符串和注释不改变下一行的缩进提案。 */
static TZrBool lsp_editor_indent_scan_callback(TZrChar value, TZrSize offset, void *userData) {
    SZrLspIndentScanData *data = (SZrLspIndentScanData *)userData;

    ZR_UNUSED_PARAMETER(offset);
    if (data == ZR_NULL || data->indent == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value == '{') {
        (*data->indent)++;
    } else if (value == '}' && *data->indent > 0) {
        (*data->indent)--;
    }
    return ZR_TRUE;
}

/* 区间格式化从此前的完整文档重建缩进状态，防止选中段丢失外层块级。 */
static TZrInt32 lsp_editor_indent_before_offset(const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize offset,
                                                SZrLspEditorScanState *scanState) {
    TZrInt32 indent = 0;
    SZrLspIndentScanData data;

    if (scanState != ZR_NULL) {
        scanState->mode = ZR_LSP_EDITOR_SCAN_CODE;
        scanState->escaped = ZR_FALSE;
    }
    if (content == ZR_NULL) {
        return indent;
    }
    if (offset > contentLength) {
        offset = contentLength;
    }

    data.indent = &indent;
    (void)lsp_editor_scan_structural_chars(content,
                                           contentLength,
                                           0,
                                           offset,
                                           scanState,
                                           lsp_editor_indent_scan_callback,
                                           &data);
    return indent;
}

/* 全文和区间格式化共享逐行文本提案；结果为 malloc 缓冲，由入口转成 VM 编辑后释放。 */
static TZrChar *lsp_editor_format_segment(const TZrChar *content,
                                          TZrSize contentLength,
                                          TZrSize startOffset,
                                          TZrSize endOffset,
                                          TZrSize *outLength) {
    SZrLspTextBuilder builder = {0};
    TZrSize cursor;
    TZrInt32 indent;
    SZrLspEditorScanState scanState;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (content == ZR_NULL || outLength == ZR_NULL || startOffset > contentLength) {
        return ZR_NULL;
    }
    if (endOffset > contentLength) {
        endOffset = contentLength;
    }
    if (endOffset < startOffset) {
        endOffset = startOffset;
    }

    indent = lsp_editor_indent_before_offset(content, contentLength, startOffset, &scanState);
    cursor = startOffset;
    while (cursor < endOffset) {
        TZrSize lineStart = cursor;
        TZrSize lineEnd = cursor;
        TZrSize trimStart;
        TZrSize trimEnd;
        TZrBool hasNewline;

        while (lineEnd < endOffset && content[lineEnd] != '\n') {
            lineEnd++;
        }
        hasNewline = lineEnd < endOffset && content[lineEnd] == '\n';

        trimStart = lineStart;
        trimEnd = lineEnd;
        if (trimEnd > trimStart && content[trimEnd - 1] == '\r') {
            trimEnd--;
        }
        /* BUG: parser 允许反引号模板跨行；TEMPLATE_STRING 模式下也剪空白并重加缩进，会改变模板字符串值。 */
        while (trimStart < trimEnd &&
               (content[trimStart] == ' ' || content[trimStart] == '\t')) {
            trimStart++;
        }
        while (trimEnd > trimStart &&
               (content[trimEnd - 1] == ' ' || content[trimEnd - 1] == '\t')) {
            trimEnd--;
        }

        if (trimStart < trimEnd) {
            TZrSize scanStart = trimStart;
            SZrLspIndentScanData data;

            /* 闭合块须先降低该行缩进，再让剩余结构字符更新后续行的状态。 */
            if (scanState.mode == ZR_LSP_EDITOR_SCAN_CODE && content[trimStart] == '}') {
                if (indent > 0) {
                    indent--;
                }
                scanStart++;
            }

            if (!lsp_text_builder_append_indent(&builder, indent) ||
                !lsp_text_builder_append_range(&builder, content + trimStart, trimEnd - trimStart)) {
                free(builder.data);
                return ZR_NULL;
            }

            data.indent = &indent;
            if (!lsp_editor_scan_structural_chars(content,
                                                  contentLength,
                                                  scanStart,
                                                  trimEnd,
                                                  &scanState,
                                                  lsp_editor_indent_scan_callback,
                                                  &data)) {
                free(builder.data);
                return ZR_NULL;
            }
        }

        if (hasNewline && !lsp_text_builder_append_char(&builder, '\n')) {
            free(builder.data);
            return ZR_NULL;
        }
        if (hasNewline && scanState.mode == ZR_LSP_EDITOR_SCAN_LINE_COMMENT) {
            scanState.mode = ZR_LSP_EDITOR_SCAN_CODE;
            scanState.escaped = ZR_FALSE;
        }
        cursor = hasNewline ? lineEnd + 1 : lineEnd;
    }

    if (builder.data == ZR_NULL) {
        builder.data = (TZrChar *)malloc(1);
        if (builder.data == ZR_NULL) {
            return ZR_NULL;
        }
        builder.data[0] = '\0';
    }

    *outLength = builder.length;
    return builder.data;
}

/**
 * @brief 供 stdio、WASM 和测试请求全文格式化；未检出旧语法迁移项且文本有差异时追加单个编辑。
 * @note result 可为已初始化数组；无修改时成功返回空增量，调用方须用 FreeTextEdits 清理。
 * @note TODO: stdio 调用方未传入 FormattingOptions；需确认固定四空格策略是否应覆盖客户端 tabSize/insertSpaces。
 */
TZrBool ZrLanguageServer_Lsp_GetFormatting(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrArray *result) {
    SZrFileVersionContentSnapshot snapshot;
    TZrChar *formatted;
    TZrSize formattedLength;
    SZrLspRange range;
    TZrBool isCurrentSyntax;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    if (!lsp_editor_acquire_content_snapshot(state, context, uri, &snapshot)) {
        return ZR_FALSE;
    }
    if (!lsp_editor_source_is_current_syntax(
                state,
                uri,
                snapshot.content,
                snapshot.contentLength,
                &isCurrentSyntax)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    if (!isCurrentSyntax) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    formatted = lsp_editor_format_segment(snapshot.content,
                                          snapshot.contentLength,
                                          0,
                                          snapshot.contentLength,
                                          &formattedLength);
    if (formatted == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    if (formattedLength == snapshot.contentLength &&
        memcmp(formatted, snapshot.content, formattedLength) == 0) {
        free(formatted);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    range = lsp_editor_full_document_range(snapshot.content, snapshot.contentLength);
    if (!lsp_editor_append_text_edit(state, result, range, formatted, formattedLength)) {
        free(formatted);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    free(formatted);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/**
 * @brief 供 stdio 单区间和多区间适配器请求整行范围格式化。
 * @note 结果替换区间可能扩到请求边界之外；多区间调用方须处理编辑重叠。
 * @note TODO: stdio 调用方只传 range，未传 FormattingOptions；需与全文格式化统一核对客户端选项契约。
 */
TZrBool ZrLanguageServer_Lsp_GetRangeFormatting(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrLspRange range,
                                                SZrArray *result) {
    SZrFileVersionContentSnapshot snapshot;
    TZrSize startOffset;
    TZrSize endOffset;
    TZrChar *formatted;
    TZrSize formattedLength;
    SZrLspRange editRange;
    TZrBool isCurrentSyntax;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspTextEdit *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    if (!lsp_editor_acquire_content_snapshot(state, context, uri, &snapshot)) {
        return ZR_FALSE;
    }
    if (!lsp_editor_source_is_current_syntax(
                state,
                uri,
                snapshot.content,
                snapshot.contentLength,
                &isCurrentSyntax)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    if (!isCurrentSyntax) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    startOffset = lsp_editor_line_start_offset(snapshot.content, snapshot.contentLength, range.start.line);
    endOffset = lsp_editor_line_end_offset(snapshot.content, snapshot.contentLength, range.end.line);
    if (endOffset < snapshot.contentLength && snapshot.content[endOffset] == '\n') {
        endOffset++;
    }

    formatted = lsp_editor_format_segment(snapshot.content,
                                          snapshot.contentLength,
                                          startOffset,
                                          endOffset,
                                          &formattedLength);
    if (formatted == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    if (formattedLength == endOffset - startOffset &&
        memcmp(formatted, snapshot.content + startOffset, formattedLength) == 0) {
        free(formatted);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }

    editRange = lsp_editor_range_from_offsets(snapshot.content,
                                              snapshot.contentLength,
                                              startOffset,
                                              endOffset);
    if (!lsp_editor_append_text_edit(state, result, editRange, formatted, formattedLength)) {
        free(formatted);
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    free(formatted);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/* 选择范围的最小层只按 ASCII 标识符字符扩展；由更大层覆盖整行。 */
/* TODO: 语言词法器可接受的非 ASCII 标识符边界是否也应作为最小选择层，需与 parser 规则核对。 */
static TZrBool lsp_editor_is_word_char(TZrChar value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') ||
           value == '_';
}

/* 祖父层试图筛掉与整行相同的选择范围，避免给客户端重复层级。 */
/* BUG: 仅任一端向外扩展就被接受；`fn main() {\n}\n` 的 main 选择会把从行中 `{` 开始的块放在整行 parent 上方，grandParent 不包含 parent。 */
static TZrBool lsp_editor_range_extends(SZrLspRange outer, SZrLspRange inner) {
    if (outer.start.line < inner.start.line || outer.end.line > inner.end.line) {
        return ZR_TRUE;
    }
    if (outer.start.line == inner.start.line && outer.start.character < inner.start.character) {
        return ZR_TRUE;
    }
    return outer.end.line == inner.end.line && outer.end.character > inner.end.character;
}

/* 为选择范围找最近的花括号块，调用结果会成为整行范围的上层候选。 */
/* BUG: 此处直接扫描原始字节；字符串或注释里的“{”会与后续真实“}”配对，使光标得到不存在的块选择范围。见同文件的结构字符扫描器。 */
static TZrBool lsp_editor_find_selection_block_range(const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize offset,
                                                     TZrSize lineStart,
                                                     TZrSize lineEnd,
                                                     SZrLspRange *outRange) {
    TZrSize stack[ZR_LSP_AST_RECURSION_MAX_DEPTH];
    TZrSize stackLength = 0;
    TZrSize bestStart = 0;
    TZrSize bestEnd = 0;
    TZrBool found = ZR_FALSE;

    if (content == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < contentLength; index++) {
        TZrChar current = content[index];
        if (current == '{' && stackLength < ZR_LSP_AST_RECURSION_MAX_DEPTH) {
            stack[stackLength++] = index;
        } else if (current == '}' && stackLength > 0) {
            TZrSize openOffset = stack[--stackLength];
            TZrBool containsOffset = openOffset <= offset && offset <= index;
            TZrBool opensOnLine = openOffset >= lineStart && openOffset <= lineEnd && index >= offset;
            if ((containsOffset || opensOnLine) && (!found || openOffset > bestStart)) {
                bestStart = openOffset;
                bestEnd = index + 1;
                found = ZR_TRUE;
            }
        }
    }

    if (!found) {
        return ZR_FALSE;
    }

    *outRange = lsp_editor_range_from_offsets(content, contentLength, bestStart, bestEnd);
    return ZR_TRUE;
}

/**
 * @brief 为 stdio 和 WASM 的每个光标生成词层及可用的行、块上层选择建议。
 * @note 输入 positions 在调用期间借用；输出与输入顺序一致，由 FreeSelectionRanges 回收。
 */
TZrBool ZrLanguageServer_Lsp_GetSelectionRanges(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                const SZrLspPosition *positions,
                                                TZrSize positionCount,
                                                SZrArray *result) {
    SZrFileVersionContentSnapshot snapshot;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        positions == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSelectionRange *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    if (!lsp_editor_acquire_content_snapshot(state, context, uri, &snapshot)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < positionCount; index++) {
        SZrLspSelectionRange *selection;
        TZrSize offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(snapshot.content,
                                                                            snapshot.contentLength,
                                                                            positions[index].line,
                                                                            positions[index].character);
        TZrSize wordStart = offset;
        TZrSize wordEnd = offset;
        TZrSize lineStart = lsp_editor_line_start_offset(snapshot.content,
                                                         snapshot.contentLength,
                                                         positions[index].line);
        TZrSize lineEnd = lsp_editor_line_end_offset(snapshot.content,
                                                     snapshot.contentLength,
                                                     positions[index].line);
        TZrSize blockSearchLineStart = lineStart;
        TZrSize blockSearchLineEnd = lineEnd;
        SZrLspRange blockRange;
        TZrBool hasBlockRange;

        while (wordStart > lineStart && lsp_editor_is_word_char(snapshot.content[wordStart - 1])) {
            wordStart--;
        }
        while (wordEnd < lineEnd && lsp_editor_is_word_char(snapshot.content[wordEnd])) {
            wordEnd++;
        }
        if (wordEnd == wordStart && wordEnd < lineEnd) {
            wordEnd++;
        }

        /* BUG: 光标在行首缩进内时，词范围仍在缩进里，但父范围起点被推进到正文；返回的 parent 不包含 child。 */
        while (lineStart < lineEnd &&
               (snapshot.content[lineStart] == ' ' || snapshot.content[lineStart] == '\t')) {
            lineStart++;
        }

        selection = (SZrLspSelectionRange *)ZrCore_Memory_RawMalloc(state->global,
                                                                    sizeof(SZrLspSelectionRange));
        if (selection == ZR_NULL) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
            return ZR_FALSE;
        }
        selection->range = lsp_editor_range_from_offsets(snapshot.content,
                                                         snapshot.contentLength,
                                                         wordStart,
                                                         wordEnd);
        selection->parentRange = lsp_editor_range_from_offsets(snapshot.content,
                                                               snapshot.contentLength,
                                                               lineStart,
                                                               lineEnd);
        selection->hasParent = selection->parentRange.start.line != selection->range.start.line ||
                               selection->parentRange.start.character < selection->range.start.character ||
                               selection->parentRange.end.character > selection->range.end.character;
        hasBlockRange = lsp_editor_find_selection_block_range(snapshot.content,
                                                              snapshot.contentLength,
                                                              offset,
                                                              blockSearchLineStart,
                                                              blockSearchLineEnd,
                                                              &blockRange);
        selection->hasGrandParent = selection->hasParent &&
                                    hasBlockRange &&
                                    lsp_editor_range_extends(blockRange, selection->parentRange);
        selection->grandParentRange = selection->hasGrandParent ? blockRange : selection->parentRange;
        ZrCore_Array_Push(state, result, &selection);
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/**
 * @brief 暴露声明导航的公共入口，沿用定义查询的 Location 所有权契约。
 * @note TODO: 仓内未见独立调用方；后续需确认声明与定义是否应使用不同语义目标。
 */
TZrBool ZrLanguageServer_Lsp_GetDeclaration(SZrState *state,
                                            SZrLspContext *context,
                                            SZrString *uri,
                                            SZrLspPosition position,
                                            SZrArray *result) {
    return ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, result);
}

/**
 * @brief 暴露类型定义导航入口，目前沿用普通定义查询。
 * @note TODO: 仓内未见独立调用方；需核对类型定义与符号定义目标分离的产品约定。
 */
TZrBool ZrLanguageServer_Lsp_GetTypeDefinition(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position,
                                               SZrArray *result) {
    return ZrLanguageServer_Lsp_GetDefinition(state, context, uri, position, result);
}

/** @brief stdio implementation 请求经语义关系查询器取实际实现；结果按 Location 契约释放。 */
TZrBool ZrLanguageServer_Lsp_GetImplementation(SZrState *state,
                                               SZrLspContext *context,
                                               SZrString *uri,
                                               SZrLspPosition position,
                                               SZrArray *result) {
    return ZrLanguageServer_LspSemanticImplementationQuery_Append(
            state, context, uri, position, result);
}

/* stdio/WASM 序列化后释放原生编辑；嵌套 newText 随 VM state 生命周期管理。 */
void ZrLanguageServer_Lsp_FreeTextEdits(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspTextEdit **editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(result, index);
        if (editPtr != ZR_NULL && *editPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *editPtr, sizeof(SZrLspTextEdit));
        }
    }
    ZrCore_Array_Free(state, result);
}

/* 诊断查询失败或响应完成后统一回收原生项及其中的值数组。 */
void ZrLanguageServer_Lsp_FreeDiagnostics(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0U; index < result->length; index++) {
        SZrLspDiagnostic **diagnosticPtr =
                (SZrLspDiagnostic **)ZrCore_Array_Get(result, index);
        if (diagnosticPtr == ZR_NULL || *diagnosticPtr == ZR_NULL) {
            continue;
        }
        if ((*diagnosticPtr)->relatedInformation.isValid) {
            ZrCore_Array_Free(
                    state, &(*diagnosticPtr)->relatedInformation);
        }
        if ((*diagnosticPtr)->fixes.isValid) {
            ZrCore_Array_Free(state, &(*diagnosticPtr)->fixes);
        }
        ZrCore_Memory_RawFree(
                state->global,
                *diagnosticPtr,
                sizeof(SZrLspDiagnostic));
    }
    ZrCore_Array_Free(state, result);
}

/* CodeAction 拥有编辑数组，必须先释放嵌套编辑再释放动作本体。 */
void ZrLanguageServer_Lsp_FreeCodeActions(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspCodeAction **actionPtr = (SZrLspCodeAction **)ZrCore_Array_Get(result, index);
        if (actionPtr != ZR_NULL && *actionPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_FreeTextEdits(state, &(*actionPtr)->edits);
            ZrCore_Memory_RawFree(state->global, *actionPtr, sizeof(SZrLspCodeAction));
        }
    }
    ZrCore_Array_Free(state, result);
}

/* 折叠范围在 stdio/WASM 复制到响应后回收；kind 字符串归 VM state。 */
void ZrLanguageServer_Lsp_FreeFoldingRanges(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspFoldingRange **rangePtr = (SZrLspFoldingRange **)ZrCore_Array_Get(result, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *rangePtr, sizeof(SZrLspFoldingRange));
        }
    }
    ZrCore_Array_Free(state, result);
}

/* 选择范围的父、祖父层为内嵌值，释放每个顶层原生项即可。 */
void ZrLanguageServer_Lsp_FreeSelectionRanges(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspSelectionRange **rangePtr = (SZrLspSelectionRange **)ZrCore_Array_Get(result, index);
        if (rangePtr != ZR_NULL && *rangePtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *rangePtr, sizeof(SZrLspSelectionRange));
        }
    }
    ZrCore_Array_Free(state, result);
}

/* 文档链接的目标和提示为 VM 字符串；此处只回收原生链接项与数组。 */
void ZrLanguageServer_Lsp_FreeDocumentLinks(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspDocumentLink **linkPtr = (SZrLspDocumentLink **)ZrCore_Array_Get(result, index);
        if (linkPtr != ZR_NULL && *linkPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *linkPtr, sizeof(SZrLspDocumentLink));
        }
    }
    ZrCore_Array_Free(state, result);
}

/* prepareCall/TypeHierarchy 的独立导航项由调用方在序列化后集中释放。 */
void ZrLanguageServer_Lsp_FreeHierarchyItems(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspHierarchyItem **itemPtr = (SZrLspHierarchyItem **)ZrCore_Array_Get(result, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *itemPtr, sizeof(SZrLspHierarchyItem));
        }
    }
    ZrCore_Array_Free(state, result);
}

/* incoming/outgoing 调用边拥有子项和 fromRanges；与 prepare 阶段的独立数组分开释放。 */
void ZrLanguageServer_Lsp_FreeHierarchyCalls(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspHierarchyCall **callPtr = (SZrLspHierarchyCall **)ZrCore_Array_Get(result, index);
        if (callPtr != ZR_NULL && *callPtr != ZR_NULL) {
            ZrCore_Array_Free(state, &(*callPtr)->fromRanges);
            if ((*callPtr)->item != ZR_NULL) {
                ZrCore_Memory_RawFree(state->global, (*callPtr)->item, sizeof(SZrLspHierarchyItem));
            }
            ZrCore_Memory_RawFree(state->global, *callPtr, sizeof(SZrLspHierarchyCall));
        }
    }
    ZrCore_Array_Free(state, result);
}
