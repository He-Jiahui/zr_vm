#include "zr_vm_language_server_stdio_internal.h"
#include "stdio_json_builder.h"

/* LSP semantic token 的固定五元组：行差、列差、长度、类型、modifier 位集。 */
#define ZR_LSP_SEMANTIC_TOKEN_TUPLE_SIZE 5

/** @brief initialize 期间声明 token 类型与 modifier 序号，供客户端解释数据五元组。
 *  名称顺序必须与语义层 typeIndex 一致；declaration 固定占 modifier 位 0。
 */
cJSON *create_semantic_token_legend_json(void) {
    cJSON *legend = cJSON_CreateObject();
    cJSON *types;
    const char *modifiers[] = {"declaration"};

    if (legend == NULL ||
        (types = cJSON_AddArrayToObject(legend, ZR_LSP_FIELD_TOKEN_TYPES)) == NULL ||
        !stdio_json_add_owned_item(legend, ZR_LSP_FIELD_TOKEN_MODIFIERS,
                                   cJSON_CreateStringArray(modifiers, 1))) {
        cJSON_Delete(legend);
        return NULL;
    }

    for (TZrSize index = 0; index < ZrLanguageServer_Lsp_SemanticTokenTypeCount(); index++) {
        const TZrChar *typeName = ZrLanguageServer_Lsp_SemanticTokenTypeName(index);
        if (typeName == ZR_NULL ||
            !stdio_json_add_owned_array_item(types, cJSON_CreateString(typeName))) {
            cJSON_Delete(legend);
            return NULL;
        }
    }

    return legend;
}

/** @brief 将语义层的完整数值流封装为 full 响应，并交出 JSON 所有权。 */
/* BUG: UTF-8 协商成功后，语义层仍按 UTF-16 生成 deltaStart/length；本函数及下方
 * delta/range 编码原样发送数值，而通用响应位置转换会跳过 data 数组。非 ASCII
 * 前缀或 token 因此被客户端定位错误；见 LSP 3.17 Semantic Tokens 整数编码约束。
 */
cJSON *serialize_semantic_tokens_result(SZrArray *tokens, const char *resultId) {
    cJSON *result = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();

    if (result == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(data);
        return NULL;
    }

    for (TZrSize index = 0; tokens != ZR_NULL && index < tokens->length; index++) {
        cJSON_AddItemToArray(data, cJSON_CreateNumber((double)semantic_tokens_value_at(tokens, index)));
    }

    cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}

/** @brief 从本服务端 resultId 的末段读取旧数组长度，供缓存未命中时构造替换编辑。
 *  TODO: 当前只检查前缀并用 strtoull 宽松解析后缀；旧 ID 不在最近缓存时会把
 *  客户端传入的长度当作 deleteCount。需核对无历史基线时是否应返回 full 响应，
 *  并用非数字、溢出及跨 URI 的 previousResultId 验证协议约束。
 */
TZrSize semantic_tokens_previous_result_length(const cJSON *params) {
    const cJSON *previousResultId = get_object_item(params, ZR_LSP_FIELD_PREVIOUS_RESULT_ID);
    const char *text;
    const char *prefix = "zr-snapshot:";
    size_t prefixLength = strlen(prefix);
    const char *lengthText;

    if (!cJSON_IsString((cJSON *)previousResultId)) {
        return 0;
    }

    text = cJSON_GetStringValue((cJSON *)previousResultId);
    if (text == NULL || strncmp(text, prefix, prefixLength) != 0) {
        return 0;
    }

    lengthText = strrchr(text + prefixLength, ':');
    return lengthText != NULL ? (TZrSize)strtoull(lengthText + 1, NULL, 10) : 0;
}

/** @brief 将当前完整流和客户端上次结果的差异编码成 LSP SemanticTokensDelta。
 *  相同 resultId 直接返回空编辑；命中同 URI 上次缓存时修剪公共前后缀；其余情况
 *  用 resultId 携带的旧长度执行全量替换。返回的 JSON 由请求层接管。
 */
cJSON *serialize_semantic_tokens_delta_result(SZrArray *tokens,
                                              TZrSize previousLength,
                                              const char *previousResultId,
                                              const SZrSemanticTokenSnapshot *previousSnapshot,
                                              const char *resultId) {
    cJSON *result = cJSON_CreateObject();
    cJSON *edits = cJSON_CreateArray();
    cJSON *edit = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();
    TZrSize newLength = tokens != ZR_NULL ? tokens->length : 0;
    TZrSize start = 0;
    TZrSize deleteCount;
    TZrSize insertEnd;
    const TZrUInt32 *oldData = NULL;
    TZrSize oldLength = previousLength;

    if (result == NULL || edits == NULL || edit == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(edits);
        cJSON_Delete(edit);
        cJSON_Delete(data);
        return NULL;
    }

    cJSON_AddStringToObject(result, ZR_LSP_FIELD_RESULT_ID, resultId);
    /* BUG: 无 AST 时快照 ID 退化为 0:<数组长度>；同长度而值变化的两版流会
     * 被当成 unchanged，客户端无法收到更新。须先保证 ID 真正标识 token 内容。
     */
    if (previousResultId != NULL && strcmp(previousResultId, resultId) == 0) {
        cJSON_Delete(edit);
        cJSON_Delete(data);
        cJSON_AddItemToObject(result, ZR_LSP_FIELD_EDITS, edits);
        return result;
    }

    /* 只有缓存的 ID 与客户端 ID 完全相等，缓存的旧数组才是可比较基线。 */
    if (previousResultId != NULL && previousSnapshot != NULL &&
        strcmp(previousSnapshot->resultId, previousResultId) == 0) {
        TZrSize suffix = 0;

        oldData = previousSnapshot->data;
        oldLength = previousSnapshot->length;
        while (start < oldLength && start < newLength && oldData[start] == semantic_tokens_value_at(tokens, start)) {
            start++;
        }
        while (suffix + start < oldLength && suffix + start < newLength &&
               oldData[oldLength - suffix - 1] == semantic_tokens_value_at(tokens, newLength - suffix - 1)) {
            suffix++;
        }
        deleteCount = oldLength - start - suffix;
        insertEnd = newLength - suffix;
    } else {
        /* TODO: 缓存未命中时只能从未经认证的 previousResultId 猜旧长度；
         * 需核查返回 full 结果的协议路径，避免构造无法应用到客户端旧数组的编辑。
         */
        deleteCount = previousLength;
        insertEnd = newLength;
    }

    for (TZrSize index = start; index < insertEnd; index++) {
        cJSON_AddItemToArray(data, cJSON_CreateNumber((double)semantic_tokens_value_at(tokens, index)));
    }

    if (oldData != NULL && deleteCount == 0 && start == insertEnd) {
        cJSON_Delete(edit);
        cJSON_Delete(data);
        edit = NULL;
        data = NULL;
    } else {
        cJSON_AddNumberToObject(edit, ZR_LSP_FIELD_START, (double)start);
        cJSON_AddNumberToObject(edit, ZR_LSP_FIELD_DELETE_COUNT, (double)deleteCount);
        if (start < insertEnd) {
            cJSON_AddItemToObject(edit, ZR_LSP_FIELD_DATA, data);
            data = NULL;
        }
        cJSON_AddItemToArray(edits, edit);
        edit = NULL;
    }

    cJSON_Delete(edit);
    cJSON_Delete(data);
    cJSON_AddItemToObject(result, ZR_LSP_FIELD_EDITS, edits);
    return result;
}

/** @brief 判断 token 起点是否落在客户端请求的半开范围内。 */
/* BUG: token 从 range.start 前开始、但长度跨入请求范围时仍被丢弃；调用方只传入
 * 起点，未传入长度。LSP 3.17 semanticTokens/range 指明应包含边界处部分相交的 token。
 */
static int is_semantic_token_in_range(TZrUInt32 line, TZrUInt32 character, SZrLspRange range) {
    TZrUInt32 startLine;
    TZrUInt32 startCharacter;
    TZrUInt32 endLine;
    TZrUInt32 endCharacter;

    if (range.start.line < 0 || range.start.character < 0 || range.end.line < 0 || range.end.character < 0) {
        return 0;
    }

    startLine = (TZrUInt32)range.start.line;
    startCharacter = (TZrUInt32)range.start.character;
    endLine = (TZrUInt32)range.end.line;
    endCharacter = (TZrUInt32)range.end.character;

    return (line > startLine || (line == startLine && character >= startCharacter)) &&
           (line < endLine || (line == endLine && character < endCharacter));
}

/** @brief 向 range 响应追加按 legend 序号解释的单个协议五元组。 */
static void add_semantic_token_tuple(cJSON *data,
                                     TZrUInt32 deltaLine,
                                     TZrUInt32 deltaStart,
                                     TZrUInt32 length,
                                     TZrUInt32 tokenType,
                                     TZrUInt32 tokenModifiers) {
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)deltaLine));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)deltaStart));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)length));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)tokenType));
    cJSON_AddItemToArray(data, cJSON_CreateNumber((double)tokenModifiers));
}

/** @brief 从完整 delta 编码流筛出范围内的 token，并相对上一个保留项重新编码。
 *  客户端请求范围已由调用方转换为内部坐标；结果不带 full/delta 的 resultId。
 */
cJSON *serialize_semantic_tokens_range_result(SZrArray *tokens, SZrLspRange range) {
    cJSON *result = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();
    TZrUInt32 absoluteLine = 0;
    TZrUInt32 absoluteCharacter = 0;
    TZrUInt32 previousIncludedLine = 0;
    TZrUInt32 previousIncludedCharacter = 0;
    TZrBool hasPreviousIncluded = ZR_FALSE;

    if (result == NULL || data == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(data);
        return NULL;
    }

    /* 过滤前须先恢复绝对位置，否则跳过的 token 会破坏后续保留项的列差。 */
    for (TZrSize index = 0;
         tokens != ZR_NULL && index + ZR_LSP_SEMANTIC_TOKEN_TUPLE_SIZE - 1 < tokens->length;
         index += ZR_LSP_SEMANTIC_TOKEN_TUPLE_SIZE) {
        TZrUInt32 deltaLine = semantic_tokens_value_at(tokens, index);
        TZrUInt32 deltaStart = semantic_tokens_value_at(tokens, index + 1);
        TZrUInt32 length = semantic_tokens_value_at(tokens, index + 2);
        TZrUInt32 tokenType = semantic_tokens_value_at(tokens, index + 3);
        TZrUInt32 tokenModifiers = semantic_tokens_value_at(tokens, index + 4);

        if (deltaLine == 0) {
            absoluteCharacter += deltaStart;
        } else {
            absoluteLine += deltaLine;
            absoluteCharacter = deltaStart;
        }

        if (is_semantic_token_in_range(absoluteLine, absoluteCharacter, range)) {
            TZrUInt32 encodedDeltaLine = hasPreviousIncluded ? absoluteLine - previousIncludedLine : absoluteLine;
            TZrUInt32 encodedDeltaStart =
                (hasPreviousIncluded && encodedDeltaLine == 0)
                    ? absoluteCharacter - previousIncludedCharacter
                    : absoluteCharacter;

            add_semantic_token_tuple(data,
                                     encodedDeltaLine,
                                     encodedDeltaStart,
                                     length,
                                     tokenType,
                                     tokenModifiers);
            previousIncludedLine = absoluteLine;
            previousIncludedCharacter = absoluteCharacter;
            hasPreviousIncluded = ZR_TRUE;
        }
    }

    cJSON_AddItemToObject(result, ZR_LSP_FIELD_DATA, data);
    return result;
}
