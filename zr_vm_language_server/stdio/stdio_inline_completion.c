#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/** @brief 固定关键字候选的显示前缀与插入文本配对，供轻量行内补全筛选。 */
typedef struct SZrInlineCompletionKeyword {
    const char *keyword;
    const char *insertText;
} SZrInlineCompletionKeyword;

/* 该表只提供固定关键字模板；并不承诺当前语法上下文允许每个候选。 */
static const SZrInlineCompletionKeyword ZR_INLINE_COMPLETION_KEYWORDS[] = {
    {"return", "return "},
    {"fn", "fn "},
    {"class", "class "},
    {"pub", "pub "},
    {"pri", "pri "},
    {"sta", "static "},
    {"var", "var "},
};

/** @brief 把内部光标位置定位到内容快照字节偏移，供行内关键字前缀读取。
 *  BUG: parse_position_for_uri 返回 UTF-16 列，本函数却按 UTF-8 字节递增列号；
 *  非 ASCII 文本后的请求会错取前缀，可能返回错误候选或错误替换范围。
 *  解析器认可单独 CR 换行而本扫描只认 LF；CR 后第二行的请求会返回空候选。
 */
static int inline_completion_offset_from_position(const char *content,
                                                  size_t contentLength,
                                                  SZrLspPosition position,
                                                  size_t *outOffset) {
    TZrInt32 line = 0;
    TZrInt32 character = 0;

    if (content == NULL || outOffset == NULL || position.line < 0 || position.character < 0) {
        return 0;
    }

    for (size_t offset = 0; offset < contentLength; offset++) {
        if (line == position.line && character == position.character) {
            *outOffset = offset;
            return 1;
        }
        if (content[offset] == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    if (line == position.line && character == position.character) {
        *outOffset = contentLength;
        return 1;
    }

    return 0;
}

/** @brief 把行内候选限制在 ASCII 关键字前缀，不跨标点或空白向左搜索。 */
static int inline_completion_is_identifier_part(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
}

/** @brief 在文本快照中排除注释和引号内的光标前缀，防止把模板插入非代码区域。
 *  这是局部词法门禁；完整语法与作用域仍由未来的语义上下文决定。
 */
static int inline_completion_offset_is_in_code(const char *content,
                                               size_t contentLength,
                                               size_t targetOffset) {
    int inLineComment = 0;
    int inBlockComment = 0;
    char quote = '\0';
    int escaped = 0;

    if (content == NULL || targetOffset >= contentLength) {
        return 0;
    }

    for (size_t offset = 0; offset <= targetOffset && offset < contentLength; offset++) {
        char current = content[offset];
        char next = offset + 1 < contentLength ? content[offset + 1] : '\0';

        if (inLineComment) {
            if (offset == targetOffset) {
                return 0;
            }
            if (current == '\n' || current == '\r') {
                inLineComment = 0;
            }
            continue;
        }

        if (inBlockComment) {
            if (offset == targetOffset) {
                return 0;
            }
            if (current == '*' && next == '/') {
                if (offset + 1 >= targetOffset) {
                    return 0;
                }
                inBlockComment = 0;
                offset++;
            }
            continue;
        }

        if (quote != '\0') {
            if (offset == targetOffset) {
                return 0;
            }
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (current == '\\') {
                escaped = 1;
                continue;
            }
            if (current == quote) {
                quote = '\0';
            }
            continue;
        }

        if (offset == targetOffset) {
            return 1;
        }
        if (current == '/' && next == '/') {
            if (offset + 1 >= targetOffset) {
                return 0;
            }
            inLineComment = 1;
            offset++;
            continue;
        }
        if (current == '/' && next == '*') {
            if (offset + 1 >= targetOffset) {
                return 0;
            }
            inBlockComment = 1;
            offset++;
            continue;
        }
        if (current == '"' || current == '\'' || current == '`') {
            quote = current;
        }
    }

    return 0;
}

/** @brief 将匹配的固定关键字包装为编辑器可应用的行内补全项。返回新建 JSON 节点。 */
static cJSON *inline_completion_create_item(SZrLspPosition position,
                                            TZrInt32 prefixLength,
                                            const char *filterText,
                                            const char *insertText) {
    cJSON *item;
    SZrLspRange range;

    if (filterText == NULL || insertText == NULL || prefixLength <= 0) {
        return NULL;
    }

    item = cJSON_CreateObject();
    if (item == NULL) {
        return NULL;
    }

    range.start.line = position.line;
    range.start.character = position.character - prefixLength;
    range.end = position;
    cJSON_AddStringToObject(item, ZR_LSP_FIELD_INSERT_TEXT, insertText);
    cJSON_AddStringToObject(item, ZR_LSP_FIELD_FILTER_TEXT, filterText);
    cJSON_AddItemToObject(item, ZR_LSP_FIELD_RANGE, serialize_range(range));
    return item;
}

/** @brief 在同步后的文档快照中寻找代码区关键字前缀并返回行内候选数组。
 *  仅在 initialize 宣告支持且请求分发允许时进入；获取成功的文本快照在返回前释放。
 */
SZrLspHandlerResult handle_inline_completion_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t prefixStart;
    size_t prefixLength;
    cJSON *result;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }
    ZR_UNUSED_PARAMETER(uriText);

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    content = snapshot.content;
    contentLength = snapshot.contentLength;
    if (!inline_completion_offset_from_position(content, contentLength, position, &offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    prefixStart = offset;
    while (prefixStart > 0 && inline_completion_is_identifier_part(content[prefixStart - 1])) {
        prefixStart--;
    }
    prefixLength = offset - prefixStart;
    if (prefixLength == 0 || prefixLength > 16) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    if (!inline_completion_offset_is_in_code(content, contentLength, offset - 1)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }

    /* 固定候选按表顺序择首个匹配；当前不使用 AST、触发上下文或语义事实。 */
    for (size_t index = 0;
         index < sizeof(ZR_INLINE_COMPLETION_KEYWORDS) / sizeof(ZR_INLINE_COMPLETION_KEYWORDS[0]);
         index++) {
        const SZrInlineCompletionKeyword *keyword = &ZR_INLINE_COMPLETION_KEYWORDS[index];
        size_t keywordLength = strlen(keyword->keyword);

        if (prefixLength > 0 && prefixLength <= keywordLength &&
            strncmp(content + prefixStart, keyword->keyword, prefixLength) == 0) {
            cJSON *item = inline_completion_create_item(position,
                                                        (TZrInt32)prefixLength,
                                                        keyword->keyword,
                                                        keyword->insertText);
            if (item != NULL) {
                cJSON_AddItemToArray(result, item);
            }
            break;
        }
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    return stdio_handler_result_from_json(server->context, result);
}
