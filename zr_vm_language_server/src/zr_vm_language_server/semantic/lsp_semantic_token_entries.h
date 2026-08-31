#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_ENTRIES_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_ENTRIES_H

#include "semantic/lsp_semantic_token_canonical.h"
#include "interface/lsp_interface_internal.h"

typedef struct SZrLspSemanticTokenEntry {
    TZrUInt32 line;
    TZrUInt32 character;
    TZrUInt32 length;
    TZrUInt32 typeIndex;
    TZrUInt32 modifiers;
} SZrLspSemanticTokenEntry;

void ZrLanguageServer_LspSemanticTokenEntries_Add(
        SZrState *state,
        SZrArray *entries,
        TZrUInt32 line,
        TZrUInt32 character,
        TZrUInt32 length,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers);

TZrBool ZrLanguageServer_LspSemanticTokenEntries_AddUtf16Span(
        SZrState *state,
        SZrArray *entries,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize startOffset,
        TZrSize endOffset,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers);

void ZrLanguageServer_LspSemanticTokenEntries_AddFileRange(
        SZrState *state,
        SZrLspContext *context,
        SZrArray *entries,
        SZrString *uri,
        SZrFileRange range,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers);

void ZrLanguageServer_LspSemanticTokenEntries_AppendEncoded(
        SZrState *state,
        SZrArray *entries,
        SZrArray *result);

#endif
