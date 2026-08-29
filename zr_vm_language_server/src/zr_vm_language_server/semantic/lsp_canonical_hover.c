#include "semantic/lsp_canonical_hover.h"
#include "interface/lsp_interface_internal.h"

#include "zr_vm_parser/canonical_type.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *canonical_hover_kind_text(
        EZrSemanticSymbolKind kind,
        const SZrAstNode *declarationNode) {
    if (kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE && declarationNode != ZR_NULL) {
        switch (declarationNode->type) {
            case ZR_AST_STRUCT_DECLARATION:
                return "struct";
            case ZR_AST_ENUM_DECLARATION:
                return "enum";
            case ZR_AST_INTERFACE_DECLARATION:
                return "interface";
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                return "delegate";
            default:
                break;
        }
    }

    switch (kind) {
        case ZR_SEMANTIC_SYMBOL_KIND_FUNCTION:
            return "function";
        case ZR_SEMANTIC_SYMBOL_KIND_TYPE:
            return "class";
        case ZR_SEMANTIC_SYMBOL_KIND_FIELD:
            return "field";
        case ZR_SEMANTIC_SYMBOL_KIND_PROPERTY:
            return "property";
        case ZR_SEMANTIC_SYMBOL_KIND_PARAMETER:
        case ZR_SEMANTIC_SYMBOL_KIND_VARIABLE:
        case ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN:
        default:
            return "variable";
    }
}

static const TZrChar *canonical_hover_string_text(const SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort((SZrString *)value)
               : ZrCore_String_GetNativeString((SZrString *)value);
}

static SZrString *canonical_hover_append_section(
        SZrState *state,
        SZrString *base,
        SZrString *appendix) {
    const TZrChar *baseText;
    const TZrChar *appendixText;
    TZrSize baseLength;
    TZrSize appendixLength;
    TZrChar buffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];

    if (state == ZR_NULL || base == ZR_NULL || appendix == ZR_NULL) {
        return base;
    }

    baseText = canonical_hover_string_text(base);
    appendixText = canonical_hover_string_text(appendix);
    baseLength = base->shortStringLength < ZR_VM_LONG_STRING_FLAG
                     ? base->shortStringLength
                     : base->longStringLength;
    appendixLength = appendix->shortStringLength < ZR_VM_LONG_STRING_FLAG
                         ? appendix->shortStringLength
                         : appendix->longStringLength;
    if (baseText == ZR_NULL || appendixText == ZR_NULL || appendixLength == 0U ||
        baseLength + appendixLength + 3U >= sizeof(buffer) ||
        strstr(baseText, appendixText) != ZR_NULL) {
        return base;
    }

    memcpy(buffer, baseText, baseLength);
    memcpy(buffer + baseLength, "\n\n", 2U);
    memcpy(buffer + baseLength + 2U, appendixText, appendixLength);
    buffer[baseLength + appendixLength + 2U] = '\0';
    return ZrCore_String_Create(
            state, buffer, baseLength + appendixLength + 2U);
}

static TZrBool canonical_hover_range_is_in_extern_block(
        SZrAstNode *node,
        SZrFileRange range) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL &&
                node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (canonical_hover_range_is_in_extern_block(
                                node->data.script.statements->nodes[index], range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return node->data.compileTimeDeclaration.declaration != ZR_NULL &&
                   canonical_hover_range_is_in_extern_block(
                           node->data.compileTimeDeclaration.declaration, range);

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL &&
                node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (canonical_hover_range_is_in_extern_block(
                                node->data.block.body->nodes[index], range)) {
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
                        declaration->location.start.offset <= range.start.offset &&
                        declaration->location.end.offset >= range.end.offset) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool canonical_hover_symbol_is_ffi_extern(
        SZrAstNode *documentAst,
        const SZrSymbol *symbol) {
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (symbol->astNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION ||
        symbol->astNode->type == ZR_AST_EXTERN_DELEGATE_DECLARATION) {
        return ZR_TRUE;
    }
    return canonical_hover_range_is_in_extern_block(
            documentAst, symbol->astNode->location);
}

static void canonical_hover_enrich_source_symbol(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrAstNode *documentAst,
        SZrSymbol *sourceSymbol,
        SZrLspHover *hover) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrString **content;
    SZrString *comment;
    SZrString *merged;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        sourceSymbol == ZR_NULL || hover == ZR_NULL || hover->contents.length == 0U) {
        return;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(
                state, fileVersion, &snapshot)) {
        return;
    }

    content = (SZrString **)ZrCore_Array_Get(&hover->contents, 0U);
    comment = ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(
            state, sourceSymbol, snapshot.content, snapshot.contentLength);
    if (content != ZR_NULL && *content != ZR_NULL && comment != ZR_NULL) {
        merged = canonical_hover_append_section(state, *content, comment);
        if (merged != *content) {
            *content = merged;
        }
    }
    if (content != ZR_NULL && *content != ZR_NULL) {
        merged = ZrLanguageServer_Lsp_AppendSymbolFfiMetadataMarkdown(
                state, *content, sourceSymbol);
        if (merged != *content) {
            *content = merged;
        }
    }
    if (content != ZR_NULL && *content != ZR_NULL &&
        canonical_hover_symbol_is_ffi_extern(documentAst, sourceSymbol)) {
        SZrString *ffiSource = ZrCore_String_Create(
                state, "Source: ffi extern", sizeof("Source: ffi extern") - 1U);
        if (ffiSource != ZR_NULL) {
            merged = canonical_hover_append_section(state, *content, ffiSource);
            if (merged != *content) {
                *content = merged;
            }
        }
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
}

static TZrBool canonical_hover_create(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const TZrChar *content,
        SZrFileRange referenceRange,
        SZrLspHover **result) {
    SZrLspHover *hover;
    SZrString *text;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        content == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_Create(
            state, (TZrNativeString)content, strlen(content));
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1U);
    ZrCore_Array_Push(state, &hover->contents, &text);
    hover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, referenceRange);
    *result = hover;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspCanonicalHover_BuildSymbol(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol,
        SZrFileRange referenceRange,
        SZrAstNode *documentAst,
        SZrSymbol *sourceSymbol,
        SZrLspHover **result) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar content[ZR_LSP_HOVER_BUFFER_LENGTH];
    const TZrChar *name;
    const TZrChar *signature;
    const TZrChar *typeText;
    const TZrChar *kindText;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        semanticContext == ZR_NULL ||
        symbol == ZR_NULL || result == ZR_NULL ||
        symbol->symbolId == ZR_SEMANTIC_ID_INVALID ||
        symbol->displayName == ZR_NULL) {
        return ZR_FALSE;
    }

    name = canonical_hover_string_text(symbol->displayName);
    if (name == ZR_NULL || name[0] == '\0') {
        return ZR_FALSE;
    }
    kindText = canonical_hover_kind_text(symbol->kind, symbol->declarationNode);
    signature = canonical_hover_string_text(symbol->signatureDisplay);
    if (signature != ZR_NULL && signature[0] != '\0') {
        snprintf(content,
                 sizeof(content),
                 "**%s**: %s\n\nSignature: %s",
                 kindText,
                 name,
                 signature);
    } else if (ZrParser_CanonicalType_Format(
                       semanticContext,
                       symbol->typeId,
                       typeBuffer,
                       sizeof(typeBuffer))) {
        typeText = typeBuffer;
        snprintf(content,
                 sizeof(content),
                 "**%s**: %s\n\nResolved Type: %s",
                 kindText,
                 name,
                 typeText);
    } else {
        snprintf(content,
                 sizeof(content),
                 "**%s**: %s\n\nType: cannot infer exact type",
                 kindText,
                 name);
    }

    if (!canonical_hover_create(
                state, context, uri, content, referenceRange, result)) {
        return ZR_FALSE;
    }
    if (sourceSymbol != ZR_NULL &&
        sourceSymbol->semanticId == symbol->symbolId) {
        canonical_hover_enrich_source_symbol(
                state, context, uri, documentAst, sourceSymbol, *result);
    }
    return ZR_TRUE;
}
