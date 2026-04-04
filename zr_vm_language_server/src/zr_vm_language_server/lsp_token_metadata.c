#include "lsp_interface_internal.h"

#include <ctype.h>
#include <string.h>

typedef struct SZrLspTokenMetadataDescriptor {
    const TZrChar *label;
    const TZrChar *kind;
    const TZrChar *detail;
    const TZrChar *category;
    const TZrChar *applicableTo;
} SZrLspTokenMetadataDescriptor;

static const SZrLspTokenMetadataDescriptor g_lspDirectiveTokens[] = {
    {"%import", "keyword", "module import directive", "directive", "statement / expression prefix"},
    {"%type", "keyword", "type query / reflection directive", "directive", "expression prefix"},
    {"%module", "keyword", "module declaration directive", "directive", "module declaration"},
    {"%test", "keyword", "test declaration directive", "directive", "test declaration"},
    {"%compileTime", "keyword", "compile-time declaration directive", "directive", "compile-time declaration"},
    {"%extern", "keyword", "FFI extern declaration directive", "directive", "extern declaration"},
    {"%unique", "keyword", "ownership qualifier", "ownership", "ownership-qualified declarations"},
    {"%shared", "keyword", "ownership qualifier", "ownership", "ownership-qualified declarations"},
    {"%weak", "keyword", "ownership qualifier", "ownership", "ownership-qualified declarations"},
    {"%borrowed", "keyword", "ownership qualifier", "ownership", "ownership-qualified declarations"}
};

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

static TZrBool token_metadata_is_identifier_start(TZrChar value) {
    return isalpha((unsigned char)value) || value == '_';
}

static TZrBool token_metadata_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

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

static const SZrLspTokenMetadataDescriptor *token_metadata_find_directive_descriptor(const TZrChar *text,
                                                                                      TZrSize length) {
    return token_metadata_find_descriptor(g_lspDirectiveTokens,
                                          sizeof(g_lspDirectiveTokens) / sizeof(g_lspDirectiveTokens[0]),
                                          text,
                                          length);
}

static const SZrLspTokenMetadataDescriptor *token_metadata_find_meta_method_descriptor(const TZrChar *text,
                                                                                        TZrSize length) {
    return token_metadata_find_descriptor(g_lspMetaMethodTokens,
                                          sizeof(g_lspMetaMethodTokens) / sizeof(g_lspMetaMethodTokens[0]),
                                          text,
                                          length);
}

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

TZrBool ZrLanguageServer_Lsp_IsKnownDirectiveToken(const TZrChar *text, TZrSize length) {
    return token_metadata_find_directive_descriptor(text, length) != ZR_NULL;
}

TZrBool ZrLanguageServer_Lsp_IsKnownMetaMethodToken(const TZrChar *text, TZrSize length) {
    return token_metadata_find_meta_method_descriptor(text, length) != ZR_NULL;
}

TZrBool ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(SZrState *state,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize cursorOffset,
                                                              SZrArray *result) {
    TZrChar prefixChar;

    if (state == ZR_NULL || content == ZR_NULL || result == ZR_NULL || cursorOffset == 0 ||
        cursorOffset > contentLength) {
        return ZR_FALSE;
    }

    prefixChar = content[cursorOffset - 1];
    if (prefixChar == '%') {
        token_metadata_append_completion_descriptors(state,
                                                     result,
                                                     g_lspDirectiveTokens,
                                                     sizeof(g_lspDirectiveTokens) / sizeof(g_lspDirectiveTokens[0]));
        return result->length > 0;
    }

    if (prefixChar == '@') {
        token_metadata_append_completion_descriptors(state,
                                                     result,
                                                     g_lspMetaMethodTokens,
                                                     sizeof(g_lspMetaMethodTokens) / sizeof(g_lspMetaMethodTokens[0]));
        return result->length > 0;
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_Lsp_TryGetMetaMethodHover(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri,
                                                   SZrLspPosition position,
                                                   SZrLspHover **result) {
    SZrFileVersion *fileVersion;
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
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || fileVersion->contentLength == 0) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    if (!token_metadata_find_prefixed_identifier_at_offset(fileVersion->content,
                                                           fileVersion->contentLength,
                                                           filePosition.offset,
                                                           '@',
                                                           &tokenStart,
                                                           &tokenEnd)) {
        return ZR_FALSE;
    }

    tokenLength = tokenEnd - tokenStart;
    descriptor = token_metadata_find_meta_method_descriptor(fileVersion->content + tokenStart, tokenLength);
    if (descriptor == ZR_NULL || descriptor->label == ZR_NULL) {
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
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        return ZR_FALSE;
    }

    range = ZrParser_FileRange_Create(token_metadata_file_position_from_offset(fileVersion->content,
                                                                               fileVersion->contentLength,
                                                                               tokenStart),
                                      token_metadata_file_position_from_offset(fileVersion->content,
                                                                               fileVersion->contentLength,
                                                                               tokenEnd),
                                      uri);

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &markdown);
    hover->range = ZrLanguageServer_LspRange_FromFileRange(range);

    *result = hover;
    return ZR_TRUE;
}
