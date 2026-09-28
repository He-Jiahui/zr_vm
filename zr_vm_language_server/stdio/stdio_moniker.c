#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_handler_result.h"

/* 轻量 moniker 仅在形似标识符的代码区词上生成文档内身份；词法范围不代表语义绑定。 */
static int moniker_is_identifier_start(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

/* 与 moniker_is_identifier_start 共同决定词的范围，不尝试推断跨文件符号。 */
static int moniker_is_identifier_part(char ch) {
    return moniker_is_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

/* 避免把注释和双引号字符串中的普通词当成符号身份；调用前快照必须保持有效。
 * BUG: ZR 的单引号字符和反引号模板字符串未被视为非代码，定位到 'x' 或 `abc` 内会返回 moniker；
 * lexer.c 接受这两种字面量，而 stdio_smoke 目前只覆盖注释与双引号。 */
static int moniker_offset_is_in_code(const char *content, size_t contentLength, size_t targetOffset) {
    int inLineComment = 0;
    int inBlockComment = 0;
    int inString = 0;
    int escaped = 0;

    if (content == NULL || targetOffset >= contentLength) {
        return 0;
    }

    for (size_t offset = 0; offset <= targetOffset && offset < contentLength; offset++) {
        char ch = content[offset];
        char next = offset + 1 < contentLength ? content[offset + 1] : '\0';

        if (inLineComment) {
            if (offset == targetOffset) {
                return 0;
            }
            if (ch == '\n') {
                inLineComment = 0;
            }
            continue;
        }

        if (inBlockComment) {
            if (offset == targetOffset) {
                return 0;
            }
            if (ch == '*' && next == '/') {
                if (offset + 1 >= targetOffset) {
                    return 0;
                }
                inBlockComment = 0;
                offset++;
            }
            continue;
        }

        if (inString) {
            if (offset == targetOffset) {
                return 0;
            }
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (ch == '\\') {
                escaped = 1;
                continue;
            }
            if (ch == '"') {
                inString = 0;
            }
            continue;
        }

        if (offset == targetOffset) {
            return 1;
        }
        if (ch == '/' && next == '/') {
            if (offset + 1 >= targetOffset) {
                return 0;
            }
            inLineComment = 1;
            offset++;
            continue;
        }
        if (ch == '/' && next == '*') {
            if (offset + 1 >= targetOffset) {
                return 0;
            }
            inBlockComment = 1;
            offset++;
            continue;
        }
        if (ch == '"') {
            inString = 1;
        }
    }

    return 0;
}

/* 把请求坐标定位到当前不可变文本快照，再交给词法过滤和 moniker 构造。
 * BUG: get_uri_and_position 给出内部 UTF-16 列，而这里逐 UTF-8 字节递增列；
 * 同一行若标识符前有非 ASCII 字符，合法请求可命中错误字节或返回空数组。 */
static int moniker_offset_from_position(const char *content,
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

/* 用 URI 和词文本构造 document-scoped 身份，cJSON 接管复制后的字段。
 * TODO: 四次字段添加失败未检查，需以分配失败注入核对是否会发布缺少身份字段的对象。 */
static cJSON *moniker_create_for_word(const char *uriText,
                                      const char *wordStart,
                                      size_t wordLength) {
    cJSON *moniker = cJSON_CreateObject();
    char *identifier;
    size_t uriLength;

    if (moniker == NULL || uriText == NULL || wordStart == NULL || wordLength == 0) {
        cJSON_Delete(moniker);
        return NULL;
    }

    uriLength = strlen(uriText);
    identifier = (char *)malloc(uriLength + 1 + wordLength + 1);
    if (identifier == NULL) {
        cJSON_Delete(moniker);
        return NULL;
    }

    memcpy(identifier, uriText, uriLength);
    identifier[uriLength] = '#';
    memcpy(identifier + uriLength + 1, wordStart, wordLength);
    identifier[uriLength + 1 + wordLength] = '\0';

    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_SCHEME, "zr");
    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_IDENTIFIER, identifier);
    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_UNIQUE, "document");
    cJSON_AddStringToObject(moniker, ZR_LSP_FIELD_KIND, "local");

    free(identifier);
    return moniker;
}

/* textDocument/moniker 读取 parser 版本快照，只为形似标识符的代码区词返回轻量本地身份；
 * 不查询跨文件符号，任何分支退出前都必须释放已取得的快照。 */
SZrLspHandlerResult handle_moniker_request(SZrStdioServer *server, const cJSON *params) {
    SZrLspPosition position;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *content;
    size_t contentLength;
    size_t offset;
    size_t wordStart;
    size_t wordEnd;
    cJSON *result;
    cJSON *moniker;

    if (!get_uri_and_position(server, params, &uriText, &uri, &position)) {
        return stdio_handler_error(ZR_LSP_HANDLER_INVALID_PARAMS);
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    content = snapshot.content;
    contentLength = snapshot.contentLength;
    if (!moniker_offset_from_position(content, contentLength, position, &offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }
    if (offset >= contentLength || !moniker_is_identifier_part(content[offset])) {
        if (offset == 0 || !moniker_is_identifier_part(content[offset - 1])) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
            return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
        }
        offset--;
    }
    if (!moniker_offset_is_in_code(content, contentLength, offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    wordStart = offset;
    while (wordStart > 0 && moniker_is_identifier_part(content[wordStart - 1])) {
        wordStart--;
    }
    if (!moniker_is_identifier_start(content[wordStart])) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return stdio_handler_result_from_json(server->context, cJSON_CreateArray());
    }

    wordEnd = offset + 1;
    while (wordEnd < contentLength && moniker_is_identifier_part(content[wordEnd])) {
        wordEnd++;
    }

    result = cJSON_CreateArray();
    moniker = moniker_create_for_word(uriText, content + wordStart, wordEnd - wordStart);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    if (result == NULL || moniker == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(moniker);
        return stdio_handler_result_from_json(server->context, ZR_NULL);
    }

    cJSON_AddItemToArray(result, moniker);
    return stdio_handler_result_from_json(server->context, result);
}
