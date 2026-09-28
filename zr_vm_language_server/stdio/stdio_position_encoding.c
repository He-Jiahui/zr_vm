#include "zr_vm_language_server_stdio_internal.h"
#include "zr_vm_core/utf8.h"

/** 将协商状态集中解释给请求解码与响应编码两条路径；空 server 沿用协议默认 UTF-16。 */
static TZrBool position_encoding_is_utf8(const SZrStdioServer *server) {
    return server != ZR_NULL && server->positionEncoding == ZR_STDIO_POSITION_ENCODING_UTF8;
}

/** initialize 响应复用与实际转换相同的状态，避免向客户端宣告另一种坐标编码。 */
const char *position_encoding_name(const SZrStdioServer *server) {
    return position_encoding_is_utf8(server) ? ZR_LSP_POSITION_ENCODING_UTF8 : ZR_LSP_POSITION_ENCODING_UTF16;
}

/** 请求与响应的宽松映射按快照向前扫描；无法识别的前导字节退化为一步。
 * TODO: 已识别的前导只检查剩余长度，未验续字节或过长编码；需核对非法磁盘文本的坐标约定。 */
static TZrSize utf8_codepoint_length(const char *content, size_t contentLength, size_t offset) {
    unsigned char first;

    if (content == NULL || offset >= contentLength) {
        return 0;
    }

    first = (unsigned char)content[offset];
    if (first < 0x80u) {
        return 1;
    }
    if ((first & 0xE0u) == 0xC0u && offset + 1 < contentLength) {
        return 2;
    }
    if ((first & 0xF0u) == 0xE0u && offset + 2 < contentLength) {
        return 3;
    }
    if ((first & 0xF8u) == 0xF0u && offset + 3 < contentLength) {
        return 4;
    }

    return 1;
}

/** 将内部 UTF-16 列与客户端 UTF-8 字节列对应，补充平面字符需要两个 UTF-16 单元。 */
static TZrInt32 utf16_units_for_utf8_codepoint(const char *content,
                                               size_t contentLength,
                                               size_t offset,
                                               TZrSize byteLength) {
    unsigned char first;
    TZrUInt32 codepoint;

    if (content == NULL || offset >= contentLength || byteLength == 0) {
        return 1;
    }

    first = (unsigned char)content[offset];
    if (byteLength == 4 && (first & 0xF8u) == 0xF0u) {
        codepoint = ((TZrUInt32)(first & 0x07u) << 18) |
                    ((TZrUInt32)((unsigned char)content[offset + 1] & 0x3Fu) << 12) |
                    ((TZrUInt32)((unsigned char)content[offset + 2] & 0x3Fu) << 6) |
                    (TZrUInt32)((unsigned char)content[offset + 3] & 0x3Fu);
        return codepoint >= 0x10000u ? 2 : 1;
    }

    return 1;
}

/** 为请求和响应的宽松转换定位 LF 行；不可定位时调用方保留原坐标。
 * BUG: strict_find_line_bounds 接受独立 CR 为换行，本路径与核心 codec 只按 LF 增行；CR-only 第二行
 * 的客户端位置虽通过严格校验，进入语义查询或返回客户端时仍按错误行与列解释。 */
static TZrBool find_line_bounds(const char *content,
                                size_t contentLength,
                                TZrInt32 targetLine,
                                size_t *outLineStart,
                                size_t *outLineEnd) {
    TZrInt32 line = 0;
    size_t lineStart = 0;
    size_t index;

    if (content == NULL || targetLine < 0 || outLineStart == NULL || outLineEnd == NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < contentLength; index++) {
        if (line == targetLine && (content[index] == '\n' || content[index] == '\r')) {
            *outLineStart = lineStart;
            *outLineEnd = index;
            return ZR_TRUE;
        }

        if (content[index] == '\n') {
            if (line == targetLine) {
                *outLineStart = lineStart;
                *outLineEnd = index;
                return ZR_TRUE;
            }
            line++;
            lineStart = index + 1;
        }
    }

    if (line == targetLine) {
        *outLineStart = lineStart;
        *outLineEnd = contentLength;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

/** 对 didChange 和位置请求拒绝不存在的行，同时把 CRLF 视为单次换行。 */
static TZrBool strict_find_line_bounds(const char *content,
                                       size_t contentLength,
                                       TZrInt32 targetLine,
                                       size_t *outLineStart,
                                       size_t *outLineEnd) {
    TZrInt32 line = 0;
    size_t lineStart = 0;
    size_t index = 0;

    if (content == ZR_NULL || targetLine < 0 || outLineStart == ZR_NULL || outLineEnd == ZR_NULL) {
        return ZR_FALSE;
    }
    while (index < contentLength) {
        if (content[index] == '\r' || content[index] == '\n') {
            if (line == targetLine) {
                *outLineStart = lineStart;
                *outLineEnd = index;
                return ZR_TRUE;
            }
            if (content[index] == '\r' && index + 1U < contentLength && content[index + 1U] == '\n') {
                index++;
            }
            line++;
            lineStart = index + 1U;
        }
        index++;
    }
    if (line != targetLine) {
        return ZR_FALSE;
    }
    *outLineStart = lineStart;
    *outLineEnd = contentLength;
    return ZR_TRUE;
}

/** 客户端位置进入内容修改或语义查询前校验 UTF-8 文本及字符边界；拒绝代理对或字节序列中间的位置。 */
static TZrBool strict_client_position_to_byte_offset(
        SZrStdioServer *server,
        const char *content,
        size_t contentLength,
        SZrLspPosition position,
        TZrSize *outOffset) {
    size_t lineStart;
    size_t lineEnd;
    size_t offset;
    TZrSize clientCharacter = 0U;

    if (position.character < 0 || outOffset == ZR_NULL ||
        !strict_find_line_bounds(content, contentLength, position.line, &lineStart, &lineEnd) ||
        !ZrCore_Utf8_IsValid((TZrNativeString)content, (TZrSize)contentLength)) {
        return ZR_FALSE;
    }
    if (position_encoding_is_utf8(server)) {
        TZrSize target = (TZrSize)position.character;

        if (target > lineEnd - lineStart) {
            return ZR_FALSE;
        }
        for (offset = lineStart; offset < lineEnd;) {
            TZrUInt32 codePoint;
            TZrSize consumedBytes;

            if (offset - lineStart == target) {
                *outOffset = (TZrSize)offset;
                return ZR_TRUE;
            }
            if (!ZrCore_Utf8_DecodeCodePoint((TZrNativeString)(content + offset),
                                              (TZrSize)(lineEnd - offset),
                                              &codePoint,
                                              &consumedBytes)) {
                return ZR_FALSE;
            }
            ZR_UNUSED_PARAMETER(codePoint);
            offset += (size_t)consumedBytes;
        }
        if (offset - lineStart != target) {
            return ZR_FALSE;
        }
        *outOffset = (TZrSize)offset;
        return ZR_TRUE;
    }

    for (offset = lineStart; offset < lineEnd;) {
        TZrUInt32 codePoint;
        TZrSize consumedBytes;
        TZrSize utf16Units;

        if (clientCharacter == (TZrSize)position.character) {
            *outOffset = (TZrSize)offset;
            return ZR_TRUE;
        }
        if (!ZrCore_Utf8_DecodeCodePoint((TZrNativeString)(content + offset),
                                          (TZrSize)(lineEnd - offset),
                                          &codePoint,
                                          &consumedBytes)) {
            return ZR_FALSE;
        }
        utf16Units = codePoint >= 0x10000U ? 2U : 1U;
        if (clientCharacter + utf16Units > (TZrSize)position.character) {
            return ZR_FALSE;
        }
        clientCharacter += utf16Units;
        offset += (size_t)consumedBytes;
    }
    if (clientCharacter != (TZrSize)position.character) {
        return ZR_FALSE;
    }
    *outOffset = (TZrSize)offset;
    return ZR_TRUE;
}

/** 计算被替换区间在已协商编码中的长度，供 didChange 的 rangeLength 交叉校验。 */
static TZrBool strict_content_client_length(SZrStdioServer *server,
                                            const char *content,
                                            TZrSize startOffset,
                                            TZrSize endOffset,
                                            TZrSize *outLength) {
    TZrSize offset;
    TZrSize length = 0U;

    if (content == ZR_NULL || outLength == ZR_NULL || endOffset < startOffset) {
        return ZR_FALSE;
    }
    if (position_encoding_is_utf8(server)) {
        *outLength = endOffset - startOffset;
        return ZR_TRUE;
    }
    for (offset = startOffset; offset < endOffset;) {
        TZrUInt32 codePoint;
        TZrSize consumedBytes;

        if (!ZrCore_Utf8_DecodeCodePoint((TZrNativeString)(content + offset),
                                          endOffset - offset,
                                          &codePoint,
                                          &consumedBytes)) {
            return ZR_FALSE;
        }
        length += codePoint >= 0x10000U ? 2U : 1U;
        offset += consumedBytes;
    }
    *outLength = length;
    return ZR_TRUE;
}

/** didChange 在修改原文前统一把客户端区间解析为字节边界，并返回原区间的客户端长度。
 * 只有返回真时三个输出才可供 apply_single_change 使用；失败时调用方放弃整批编辑。 */
TZrBool content_change_range_to_byte_offsets(SZrStdioServer *server,
                                             const char *content,
                                             size_t contentLength,
                                             const cJSON *json,
                                             TZrSize *outStartOffset,
                                             TZrSize *outEndOffset,
                                             TZrSize *outClientLength) {
    SZrLspRange range;

    if (!parse_range(json, &range) || outStartOffset == ZR_NULL ||
        outEndOffset == ZR_NULL || outClientLength == ZR_NULL ||
        !strict_client_position_to_byte_offset(
                server, content, contentLength, range.start, outStartOffset) ||
        !strict_client_position_to_byte_offset(
                server, content, contentLength, range.end, outEndOffset) ||
        *outEndOffset < *outStartOffset) {
        return ZR_FALSE;
    }
    return strict_content_client_length(
            server, content, *outStartOffset, *outEndOffset, outClientLength);
}

/** 将已解析的 UTF-8 客户端列投影成语义接口使用的 UTF-16 列；越过行尾则收敛至行尾。 */
static SZrLspPosition utf8_position_to_utf16_position(const char *content,
                                                      size_t contentLength,
                                                      SZrLspPosition position) {
    SZrLspPosition converted = position;
    size_t lineStart;
    size_t lineEnd;
    size_t targetOffset;
    size_t offset;
    TZrInt32 character = 0;

    if (position.character < 0 ||
        !find_line_bounds(content, contentLength, position.line, &lineStart, &lineEnd)) {
        return converted;
    }

    targetOffset = lineStart + (size_t)position.character;
    if (targetOffset > lineEnd) {
        targetOffset = lineEnd;
    }

    offset = lineStart;
    while (offset < targetOffset) {
        TZrSize codepointLength = utf8_codepoint_length(content, contentLength, offset);

        if (codepointLength == 0 || offset + codepointLength > targetOffset) {
            break;
        }

        character += utf16_units_for_utf8_codepoint(content, contentLength, offset, codepointLength);
        offset += codepointLength;
    }

    converted.character = character;
    return converted;
}

/** 将语义层 UTF-16 范围投影回协商的 UTF-8 字节列，供 JSON 响应序列化。 */
static SZrLspPosition utf16_position_to_utf8_position(const char *content,
                                                      size_t contentLength,
                                                      SZrLspPosition position) {
    SZrLspPosition converted = position;
    size_t lineStart;
    size_t lineEnd;
    size_t offset;
    TZrInt32 character = 0;

    if (position.character < 0 ||
        !find_line_bounds(content, contentLength, position.line, &lineStart, &lineEnd)) {
        return converted;
    }

    offset = lineStart;
    while (offset < lineEnd && character < position.character) {
        TZrSize codepointLength = utf8_codepoint_length(content, contentLength, offset);
        TZrInt32 utf16Units;

        if (codepointLength == 0 || offset + codepointLength > lineEnd) {
            break;
        }

        utf16Units = utf16_units_for_utf8_codepoint(content, contentLength, offset, codepointLength);
        if (character + utf16Units > position.character) {
            break;
        }

        character += utf16Units;
        offset += codepointLength;
    }

    converted.character = (TZrInt32)(offset - lineStart);
    return converted;
}

/** 从 parser 当前文件版本获取稳定文本；调用方必须释放成功取得的快照。 */
static TZrBool content_snapshot_for_uri(SZrStdioServer *server,
                                        SZrString *uri,
                                        SZrFileVersionContentSnapshot *outSnapshot) {
    SZrFileVersion *fileVersion;

    if (server == ZR_NULL || uri == ZR_NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    return ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, outSnapshot);
}

/** 响应 JSON 持有 URI 文本时先借用缓存 URI，再复用文件版本快照路径。 */
static TZrBool content_snapshot_for_uri_text(SZrStdioServer *server,
                                             const char *uriText,
                                             SZrFileVersionContentSnapshot *outSnapshot) {
    SZrString *uri;

    if (server == ZR_NULL || uriText == NULL || outSnapshot == ZR_NULL) {
        return ZR_FALSE;
    }

    uri = server_get_cached_uri(server, uriText);
    return content_snapshot_for_uri(server, uri, outSnapshot);
}

/** 仅 UTF-8 协商需要在进入语义接口前改列数；无快照时只能保留原值。 */
static SZrLspPosition client_position_to_internal(SZrStdioServer *server,
                                                  const char *content,
                                                  size_t contentLength,
                                                  SZrLspPosition position) {
    if (!position_encoding_is_utf8(server) || content == NULL) {
        return position;
    }

    return utf8_position_to_utf16_position(content, contentLength, position);
}

/** 与请求入口配对，把内部 UTF-16 列转换成客户端列；调用方提供对应 URI 的文本。 */
static SZrLspPosition internal_position_to_client(SZrStdioServer *server,
                                                  const char *content,
                                                  size_t contentLength,
                                                  SZrLspPosition position) {
    if (!position_encoding_is_utf8(server) || content == NULL) {
        return position;
    }

    return utf16_position_to_utf8_position(content, contentLength, position);
}

/** 让格式化、导航及编辑请求的范围两端使用相同的文档快照和编码约定。 */
static SZrLspRange client_range_to_internal(SZrStdioServer *server,
                                            const char *content,
                                            size_t contentLength,
                                            SZrLspRange range) {
    range.start = client_position_to_internal(server, content, contentLength, range.start);
    range.end = client_position_to_internal(server, content, contentLength, range.end);
    return range;
}

/** get_uri_and_position 等入口先解析协议位置；取得文档快照后严格检查边界再转成内部列。
 * TODO: 文件版本快照不可用时仍返回成功并保留客户端列，需核对未打开 URI 的语义查询是否会误用该值。 */
int parse_position_for_uri(SZrStdioServer *server,
                           SZrString *uri,
                           const cJSON *json,
                           SZrLspPosition *outPosition) {
    SZrFileVersionContentSnapshot snapshot = {0};
    TZrSize offset;

    if (!parse_position(json, outPosition)) {
        return 0;
    }

    if (content_snapshot_for_uri(server, uri, &snapshot)) {
        if (!strict_client_position_to_byte_offset(server,
                                                   snapshot.content,
                                                   snapshot.contentLength,
                                                   *outPosition,
                                                   &offset)) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
            return 0;
        }
        *outPosition = client_position_to_internal(server,
                                                   snapshot.content,
                                                   snapshot.contentLength,
                                                   *outPosition);
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    }
    return 1;
}

/** 为范围格式化等请求按 URI 快照转换编码；快照不可用时调用方获得原始范围。
 * TODO: 与 parse_position_for_uri 不同，这里未验证行及码点边界；需核对下游如何处理非法范围。 */
int parse_range_for_uri(SZrStdioServer *server,
                        SZrString *uri,
                        const cJSON *json,
                        SZrLspRange *outRange) {
    SZrFileVersionContentSnapshot snapshot = {0};

    if (!parse_range(json, outRange)) {
        return 0;
    }

    if (content_snapshot_for_uri(server, uri, &snapshot)) {
        *outRange = client_range_to_internal(server,
                                             snapshot.content,
                                             snapshot.contentLength,
                                             *outRange);
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    }
    return 1;
}

/** 已由调用方持有文本时避免再次查询文件版本，范围仍按协商编码进入内部接口。
 * TODO: 仓内仅见内部头声明和本定义，需核对是否仍有实际调用或可删除此闲置入口。 */
int parse_range_for_content(SZrStdioServer *server,
                            const char *content,
                            size_t contentLength,
                            const cJSON *json,
                            SZrLspRange *outRange) {
    if (!parse_range(json, outRange)) {
        return 0;
    }

    *outRange = client_range_to_internal(server, content, contentLength, *outRange);
    return 1;
}

/** initialize 选择本服务端支持的 UTF-8 或默认 UTF-16，并让后续请求与响应共享该状态。 */
void negotiate_position_encoding(SZrStdioServer *server, const cJSON *params) {
    const cJSON *capabilities;
    const cJSON *general;
    const cJSON *encodings;
    const cJSON *encoding;

    if (server == ZR_NULL) {
        return;
    }

    server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF16;
    capabilities = get_object_item(params, ZR_LSP_FIELD_CAPABILITIES);
    general = get_object_item(capabilities, ZR_LSP_FIELD_GENERAL);
    encodings = get_object_item(general, ZR_LSP_FIELD_POSITION_ENCODINGS);
    if (!cJSON_IsArray((cJSON *)encodings)) {
        return;
    }

    cJSON_ArrayForEach(encoding, encodings) {
        if (cJSON_IsString((cJSON *)encoding) &&
            encoding->valuestring != NULL &&
            strcmp(encoding->valuestring, ZR_LSP_POSITION_ENCODING_UTF8) == 0) {
            server->positionEncoding = ZR_STDIO_POSITION_ENCODING_UTF8;
            return;
        }
    }
}

/** 从文档请求或顶层 URI 获取响应默认文档；多文档结果仍需每项自带 URI。 */
static const char *uri_text_from_params(const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *uriJson;

    if (params == NULL) {
        return NULL;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    uriJson = get_object_item(textDocument, ZR_LSP_FIELD_URI);
    if (cJSON_IsString((cJSON *)uriJson)) {
        return cJSON_GetStringValue((cJSON *)uriJson);
    }

    uriJson = get_object_item(params, ZR_LSP_FIELD_URI);
    if (cJSON_IsString((cJSON *)uriJson)) {
        return cJSON_GetStringValue((cJSON *)uriJson);
    }

    return NULL;
}

/** 只改已有 character 数值，保留语义处理器构造的 JSON 其他字段。 */
static void set_position_character(cJSON *positionJson, SZrLspPosition position) {
    cJSON *characterJson;

    if (!cJSON_IsObject(positionJson)) {
        return;
    }

    characterJson = cJSON_GetObjectItemCaseSensitive(positionJson, ZR_LSP_FIELD_CHARACTER);
    if (cJSON_IsNumber(characterJson)) {
        cJSON_SetNumberValue(characterJson, position.character);
    }
}

/** 在递归响应树中识别位置对象；非完整位置对象交给其他 JSON 节点处理。 */
static TZrBool read_position_object(cJSON *positionJson, SZrLspPosition *outPosition) {
    cJSON *lineJson;
    cJSON *characterJson;

    if (!cJSON_IsObject(positionJson) || outPosition == ZR_NULL) {
        return ZR_FALSE;
    }

    lineJson = cJSON_GetObjectItemCaseSensitive(positionJson, ZR_LSP_FIELD_LINE);
    characterJson = cJSON_GetObjectItemCaseSensitive(positionJson, ZR_LSP_FIELD_CHARACTER);
    if (!cJSON_IsNumber(lineJson) || !cJSON_IsNumber(characterJson)) {
        return ZR_FALSE;
    }

    outPosition->line = (TZrInt32)lineJson->valuedouble;
    outPosition->character = (TZrInt32)characterJson->valuedouble;
    return ZR_TRUE;
}

/** 同一快照内转换单个位置，供位置与范围节点共用，避免范围两端取到不同版本。 */
static void encode_position_object_for_content(SZrStdioServer *server,
                                               cJSON *positionJson,
                                               const char *content,
                                               size_t contentLength) {
    SZrLspPosition position;

    if (!read_position_object(positionJson, &position)) {
        return;
    }

    position = internal_position_to_client(server, content, contentLength, position);
    set_position_character(positionJson, position);
}

/** 独立位置节点按其 URI 获取快照；未找到文本时保留原值，不虚构客户端列。 */
static void encode_position_object(SZrStdioServer *server, cJSON *positionJson, const char *uriText) {
    SZrFileVersionContentSnapshot snapshot = {0};

    if (content_snapshot_for_uri_text(server, uriText, &snapshot)) {
        encode_position_object_for_content(server,
                                           positionJson,
                                           snapshot.content,
                                           snapshot.contentLength);
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    }
}

/** 响应遍历只把同时含 start/end 位置对象的节点视作可编码 Range。 */
static TZrBool object_is_range(cJSON *json) {
    return cJSON_IsObject(json) &&
           cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_START)) &&
           cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_END));
}

/** 用同一 URI 快照重写 Range 两端，供 hover、TextEdit、Location 等结果共享。 */
static void encode_range_object(SZrStdioServer *server, cJSON *rangeJson, const char *uriText) {
    cJSON *startJson;
    cJSON *endJson;
    SZrFileVersionContentSnapshot snapshot = {0};

    if (!object_is_range(rangeJson)) {
        return;
    }
    if (!content_snapshot_for_uri_text(server, uriText, &snapshot)) {
        return;
    }

    startJson = cJSON_GetObjectItemCaseSensitive(rangeJson, ZR_LSP_FIELD_START);
    endJson = cJSON_GetObjectItemCaseSensitive(rangeJson, ZR_LSP_FIELD_END);
    encode_position_object_for_content(server, startJson, snapshot.content, snapshot.contentLength);
    encode_position_object_for_content(server, endJson, snapshot.content, snapshot.contentLength);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
}

/** foldingRange 的扁平字段不符合普通 Range 形状，因此单独映射其列号。 */
static void encode_folding_range_fields(SZrStdioServer *server, cJSON *json, const char *uriText) {
    cJSON *startLineJson;
    cJSON *startCharacterJson;
    cJSON *endLineJson;
    cJSON *endCharacterJson;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrLspPosition start;
    SZrLspPosition end;

    startLineJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_START_LINE);
    startCharacterJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_START_CHARACTER);
    endLineJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_END_LINE);
    endCharacterJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_END_CHARACTER);
    if (!cJSON_IsNumber(startLineJson) ||
        !cJSON_IsNumber(startCharacterJson) ||
        !cJSON_IsNumber(endLineJson) ||
        !cJSON_IsNumber(endCharacterJson)) {
        return;
    }

    if (!content_snapshot_for_uri_text(server, uriText, &snapshot)) {
        return;
    }

    start.line = (TZrInt32)startLineJson->valuedouble;
    start.character = (TZrInt32)startCharacterJson->valuedouble;
    end.line = (TZrInt32)endLineJson->valuedouble;
    end.character = (TZrInt32)endCharacterJson->valuedouble;
    start = internal_position_to_client(server, snapshot.content, snapshot.contentLength, start);
    end = internal_position_to_client(server, snapshot.content, snapshot.contentLength, end);
    cJSON_SetNumberValue(startCharacterJson, start.character);
    cJSON_SetNumberValue(endCharacterJson, end.character);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
}

/** 在最终响应 JSON 中按 URI 上下文遍历内部坐标；data 视为不透明协议负载而跳过。
 * BUG: rename 的 documentChanges 项把 URI 放在 textDocument 子对象，edits 是兄弟节点；
 * UTF-8 协商时这些跨文件 edits 继承请求 URI，willRenameFiles 没有默认 URI 时甚至不转换，客户端得到错误列。 */
static void apply_encoding_to_json_node(SZrStdioServer *server, cJSON *json, const char *currentUriText) {
    cJSON *child;
    const cJSON *uriJson;
    const char *localUriText = currentUriText;

    if (!position_encoding_is_utf8(server) || json == NULL) {
        return;
    }

    if (cJSON_IsArray(json)) {
        cJSON_ArrayForEach(child, json) {
            apply_encoding_to_json_node(server, child, currentUriText);
        }
        return;
    }

    if (!cJSON_IsObject(json)) {
        return;
    }

    uriJson = cJSON_GetObjectItemCaseSensitive(json, ZR_LSP_FIELD_URI);
    if (cJSON_IsString((cJSON *)uriJson) && uriJson->valuestring != NULL) {
        localUriText = uriJson->valuestring;
    }

    if (object_is_range(json)) {
        encode_range_object(server, json, localUriText);
        return;
    }

    {
        SZrLspPosition ignoredPosition;
        if (read_position_object(json, &ignoredPosition)) {
            encode_position_object(server, json, localUriText);
            return;
        }
    }

    encode_folding_range_fields(server, json, localUriText);
    cJSON_ArrayForEach(child, json) {
        /* TODO: data 同时承载服务端内部续查位置与客户端可见载荷；
         * 需逐方法核对跳过坐标重编码的契约。 */
        if (child->string != NULL &&
            (strcmp(child->string, ZR_LSP_FIELD_DATA) == 0 ||
             strcmp(child->string, ZR_LSP_FIELD_RANGE) == 0 ||
             strcmp(child->string, ZR_LSP_FIELD_POSITION) == 0)) {
            if (strcmp(child->string, ZR_LSP_FIELD_RANGE) == 0) {
                encode_range_object(server, child, localUriText);
            } else if (strcmp(child->string, ZR_LSP_FIELD_POSITION) == 0) {
                encode_position_object(server, child, localUriText);
            }
            continue;
        }

        apply_encoding_to_json_node(server, child, localUriText);
    }
}

/** codeAction/resolve 仅深拷贝客户端已编码的操作，避免最终响应再转换一次范围。 */
static TZrBool result_method_uses_client_owned_positions(const char *method) {
    return method != NULL &&
           strcmp(method, ZR_LSP_METHOD_CODE_ACTION_RESOLVE) == 0;
}

/** 所有同步请求在 JSON-RPC 发送前统一转换结果位置；入口负责传入请求方法和参数。 */
void apply_position_encoding_to_response(SZrStdioServer *server,
                                         const char *method,
                                         const cJSON *requestParams,
                                         cJSON *response) {
    if (result_method_uses_client_owned_positions(method)) {
        return;
    }

    apply_encoding_to_json_node(server, response, uri_text_from_params(requestParams));
}

/** diagnostics 等服务端通知没有请求参数，发送者直接提供其文档 URI 做响应坐标映射。 */
void apply_position_encoding_to_json_for_uri(SZrStdioServer *server,
                                             const char *uriText,
                                             cJSON *json) {
    apply_encoding_to_json_node(server, json, uriText);
}
