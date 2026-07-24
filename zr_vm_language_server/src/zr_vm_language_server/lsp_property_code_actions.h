#ifndef ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CODE_ACTIONS_H
#define ZR_VM_LANGUAGE_SERVER_LSP_PROPERTY_CODE_ACTIONS_H

#include "interface/lsp_interface_internal.h"

TZrBool ZrLanguageServer_LspPropertyCodeActions_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspRange requestedRange,
        SZrArray *result);

#endif
