#include "interface/lsp_interface_internal.h"

#include <ctype.h>
#include <string.h>

/* 补全、meta method hover 和语义 token 共同消费的展示词条；所有文本都借用静态字面量。 */
typedef struct SZrLspTokenMetadataDescriptor {
    const TZrChar *label;
    const TZrChar *kind;
    const TZrChar *detail;
    const TZrChar *category;
    const TZrChar *applicableTo;
} SZrLspTokenMetadataDescriptor;

/* 这是补全提示子集；其词表不等同于 lexer 接受的全部关键字。 */
static const SZrLspTokenMetadataDescriptor g_lspKeywordTokens[] = {
    {"let", "keyword", "immutable binding", "declaration", "local and module bindings"},
    {"var", "keyword", "mutable binding", "declaration", "local and module bindings"},
    {"fn", "keyword", "function declaration", "declaration", "functions and methods"},
    {"ref", "keyword", "reference parameter qualifier", "parameter", "function parameters"},
    {"in", "keyword", "input parameter qualifier", "parameter", "function parameters"},
    {"out", "keyword", "output parameter qualifier", "parameter", "function parameters"},
    {"scoped", "keyword", "scoped lifetime qualifier", "ownership", "references and parameters"},
    {"readonly", "keyword", "readonly qualifier", "ownership", "references and parameters"},
    {"init", "keyword", "initializer declaration", "lifecycle", "class and struct members"},
    {"own", "keyword", "ownership operation", "ownership", "ownership expressions"},
    {"move", "keyword", "move operation", "ownership", "ownership expressions"},
    {"drop", "keyword", "release operation", "ownership", "ownership expressions"},
    {"native", "keyword", "native declaration modifier", "ffi", "native declarations"},
    {"extern", "keyword", "extern declaration modifier", "ffi", "native declarations"},
    {"import", "keyword", "module import expression", "module", "module bindings"},
    {"typeid", "keyword", "runtime type query", "reflection", "expressions"},
    {"typeof", "keyword", "static type query", "reflection", "expressions"},
    {"loadModule", "keyword", "dynamic module loader", "module", "expressions"},
    {"loadPlugin", "keyword", "dynamic plugin loader", "module", "expressions"},
    {"async", "keyword", "asynchronous function modifier", "async", "function declarations"},
    {"await", "keyword", "asynchronous wait expression", "async", "expressions"},
    {"yield", "keyword", "generator yield expression", "async", "function bodies"}
};

/* meta method 的补全、hover 和语义高亮共享这一表，避免同一名称在三处解释不一。 */
static const SZrLspTokenMetadataDescriptor g_lspMetaMethodTokens[] = {
    {"@constructor", "method", "lifecycle meta method", "lifecycle", "class/struct meta function"},
    {"@destructor", "method", "lifecycle meta method", "lifecycle", "class/struct meta function"},
    {"@close", "method", "lifecycle meta method", "lifecycle", "class/struct meta function"},
    {"@add", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@sub", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@mul", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@div", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@mod", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@pow", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@neg", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@compare", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@shiftLeft", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@shiftRight", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@bitAnd", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@bitOr", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@bitXor", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@bitNot", "method", "arithmetic/operator meta method", "arithmetic/operator", "class/struct meta function"},
    {"@toBool", "method", "conversion meta method", "conversion", "class/struct meta function"},
    {"@toString", "method", "conversion meta method", "conversion", "class/struct meta function"},
    {"@toInt", "method", "conversion meta method", "conversion", "class/struct meta function"},
    {"@toUInt", "method", "conversion meta method", "conversion", "class/struct meta function"},
    {"@toFloat", "method", "conversion meta method", "conversion", "class/struct meta function"},
    {"@call", "method", "call/access meta method", "call/access", "class/struct meta function"},
    {"@getter", "method", "call/access meta method", "call/access", "class/struct meta function"},
    {"@setter", "method", "call/access meta method", "call/access", "class/struct meta function"},
    {"@getItem", "method", "call/access meta method", "call/access", "class/struct meta function"},
    {"@setItem", "method", "call/access meta method", "call/access", "class/struct meta function"}
};

/* @ 前缀扫描只接受 ASCII 标识符起始字符；真实词法合法性仍归 parser 判定。 */
static TZrBool token_metadata_is_identifier_start(TZrChar value) {
    return isalpha((unsigned char)value) || value == '_';
}

/* 光标附近的轻量扫描需要与上面的起始规则成对使用，不能代替 lexer。 */
static TZrBool token_metadata_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

/* 返回静态表中的借用项；hover 必须先匹配完整 token，再生成客户端展示内容。 */
static const SZrLspTokenMetadataDescriptor *token_metadata_find_descriptor(
    const SZrLspTokenMetadataDescriptor *descriptors,
    TZrSize descriptorCount,
    const TZrChar *text,
    TZrSize length) {
    if (descriptors == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < descriptorCount; index++) {
        const SZrLspTokenMetadataDescriptor *descriptor = &descriptors[index];
        TZrSize labelLength;

        if (descriptor->label == ZR_NULL) {
            continue;
        }

        labelLength = strlen(descriptor->label);
        if (labelLength == length && memcmp(descriptor->label, text, length) == 0) {
            return descriptor;
        }
    }

    return ZR_NULL;
}

/* 供语义 token 判别和 hover 共用完全一致的 meta method 集合。 */
static const SZrLspTokenMetadataDescriptor *token_metadata_find_meta_method_descriptor(const TZrChar *text,
                                                                                        TZrSize length) {
    return token_metadata_find_descriptor(g_lspMetaMethodTokens,
                                          sizeof(g_lspMetaMethodTokens) / sizeof(g_lspMetaMethodTokens[0]),
                                          text,
                                          length);
}

/* 将静态词条物化为补全项；所有权转给调用方结果数组，后续由补全结果释放路径处理。 */
static void token_metadata_append_completion_descriptors(SZrState *state,
                                                         SZrArray *result,
                                                         const SZrLspTokenMetadataDescriptor *descriptors,
                                                         TZrSize descriptorCount) {
    if (state == ZR_NULL || result == ZR_NULL || descriptors == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < descriptorCount; index++) {
        const SZrLspTokenMetadataDescriptor *descriptor = &descriptors[index];
        SZrCompletionItem *item;

        if (descriptor->label == ZR_NULL || descriptor->kind == ZR_NULL) {
            continue;
        }

        item = ZrLanguageServer_CompletionItem_New(state,
                                                   descriptor->label,
                                                   descriptor->kind,
                                                   descriptor->detail,
                                                   ZR_NULL,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

/* 关键字补全仅用于普通词首；上层若得到候选便优先于泛用可见符号补全。 */
static void token_metadata_append_keyword_prefix_completions(SZrState *state,
                                                             SZrArray *result,
                                                             const TZrChar *prefix,
                                                             TZrSize prefixLength) {
    for (TZrSize index = 0; index < sizeof(g_lspKeywordTokens) / sizeof(g_lspKeywordTokens[0]); index++) {
        const SZrLspTokenMetadataDescriptor *descriptor = &g_lspKeywordTokens[index];
        TZrSize labelLength = strlen(descriptor->label);
        SZrCompletionItem *item;

        if (prefixLength > labelLength || memcmp(descriptor->label, prefix, prefixLength) != 0) {
            continue;
        }
        item = ZrLanguageServer_CompletionItem_New(state,
                                                   descriptor->label,
                                                   descriptor->kind,
                                                   descriptor->detail,
                                                   ZR_NULL,
                                                   ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
}

/* hover 先按当前快照确定字节偏移，范围再由文档位置编解码器转成客户端 UTF-16 坐标。 */
static SZrFilePosition token_metadata_file_position_from_offset(const TZrChar *content,
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

/* 在光标附近提取单个 @ 名称；调用方还需做代码区域过滤和静态表精确匹配。 */
static TZrBool token_metadata_find_prefixed_identifier_at_offset(const TZrChar *content,
                                                                 TZrSize contentLength,
                                                                 TZrSize cursorOffset,
                                                                 TZrChar prefix,
                                                                 TZrSize *outStart,
                                                                 TZrSize *outEnd) {
    TZrSize probe;

    if (content == ZR_NULL || contentLength == 0 || cursorOffset > contentLength ||
        outStart == ZR_NULL || outEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    probe = cursorOffset < contentLength ? cursorOffset : contentLength - 1;
    if (!token_metadata_is_identifier_char(content[probe]) && content[probe] != prefix) {
        if (probe == 0) {
            return ZR_FALSE;
        }
        probe--;
    }

    while (probe > 0 && token_metadata_is_identifier_char(content[probe])) {
        probe--;
    }

    if (content[probe] != prefix) {
        if (token_metadata_is_identifier_char(content[probe]) && probe > 0 && content[probe - 1] == prefix) {
            probe--;
        } else {
            return ZR_FALSE;
        }
    }

    if (probe + 1 >= contentLength || !token_metadata_is_identifier_start(content[probe + 1])) {
        return ZR_FALSE;
    }

    *outStart = probe;
    *outEnd = probe + 1;
    while (*outEnd < contentLength && token_metadata_is_identifier_char(content[*outEnd])) {
        (*outEnd)++;
    }

    return *outEnd > *outStart + 1;
}

/* 语义 token 的名称识别复用 hover 词表；这里只认名称，不保证该位置是合法声明。 */
TZrBool ZrLanguageServer_Lsp_IsKnownMetaMethodToken(const TZrChar *text, TZrSize length) {
    return token_metadata_find_meta_method_descriptor(text, length) != ZR_NULL;
}

/* 作为补全管线的结构化优先分支，结果写入调用方已经初始化的数组。 */
TZrBool ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(SZrState *state,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize cursorOffset,
                                                              SZrArray *result) {
    TZrChar prefixChar;
    TZrSize prefixStart;

    if (state == ZR_NULL || content == ZR_NULL || result == ZR_NULL || cursorOffset == 0 ||
        cursorOffset > contentLength) {
        return ZR_FALSE;
    }

    /* 协议补全入口先过滤注释与字符串；这个低层词首收集器只负责词表。
     * TODO: 公开的语义收集接口也可被直接调用，未做相同预筛选；需核对其非代码区域契约。 */
    prefixChar = content[cursorOffset - 1];
    if (prefixChar == '%') {
        return ZR_FALSE;
    }

    if (prefixChar == '@') {
        token_metadata_append_completion_descriptors(state,
                                                     result,
                                                     g_lspMetaMethodTokens,
                                                     sizeof(g_lspMetaMethodTokens) / sizeof(g_lspMetaMethodTokens[0]));
        return result->length > 0;
    }

    if (!token_metadata_is_identifier_char(prefixChar)) {
        return ZR_FALSE;
    }
    prefixStart = cursorOffset;
    while (prefixStart > 0 && token_metadata_is_identifier_char(content[prefixStart - 1])) {
        prefixStart--;
    }
    token_metadata_append_keyword_prefix_completions(state,
                                                     result,
                                                     content + prefixStart,
                                                     cursorOffset - prefixStart);
    return result->length > 0;
}

/* 在通用符号 hover 前处理特殊 @ 名称；同一文档快照用于词提取、过滤和范围转换。 */
TZrBool ZrLanguageServer_Lsp_TryGetMetaMethodHover(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri,
                                                   SZrLspPosition position,
                                                   SZrLspHover **result) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrFilePosition filePosition;
    TZrSize tokenStart;
    TZrSize tokenEnd;
    const SZrLspTokenMetadataDescriptor *descriptor;
    TZrChar markdownBuffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize tokenLength;
    SZrString *markdown;
    SZrLspHover *hover;
    SZrFileRange range;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    if (snapshot.contentLength == 0) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    if (!token_metadata_find_prefixed_identifier_at_offset(snapshot.content,
                                                           snapshot.contentLength,
                                                           filePosition.offset,
                                                           '@',
                                                           &tokenStart,
                                                           &tokenEnd)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    if (!ZrLanguageServer_Lsp_IsOffsetInCodeSpan(snapshot.content,
                                                 snapshot.contentLength,
                                                 tokenStart)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    tokenLength = tokenEnd - tokenStart;
    descriptor = token_metadata_find_meta_method_descriptor(snapshot.content + tokenStart, tokenLength);
    if (descriptor == ZR_NULL || descriptor->label == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    snprintf(markdownBuffer,
             sizeof(markdownBuffer),
             "Meta Method: %s\n\nCategory: %s\n\nApplicable To: %s\n\nDetail: %s",
             descriptor->label,
             descriptor->category != ZR_NULL ? descriptor->category : "meta method",
             descriptor->applicableTo != ZR_NULL ? descriptor->applicableTo : "class/struct meta function",
             descriptor->detail != ZR_NULL ? descriptor->detail : "meta method");
    markdown = ZrCore_String_Create(state, markdownBuffer, strlen(markdownBuffer));
    if (markdown == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    range = ZrParser_FileRange_Create(token_metadata_file_position_from_offset(snapshot.content,
                                                                               snapshot.contentLength,
                                                                               tokenStart),
                                      token_metadata_file_position_from_offset(snapshot.content,
                                                                               snapshot.contentLength,
                                                                               tokenEnd),
                                      uri);

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &markdown);
    hover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);

    *result = hover;
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}
