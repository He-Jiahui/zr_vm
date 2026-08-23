#ifndef ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_STORE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_STORE_H

#include "zr_vm_language_server/lsp_interface.h"

#define ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX 128U

/*
 * Produces the pull-diagnostic identity from the immutable semantic snapshot
 * and the complete structured diagnostic payload. Callers retain ownership of
 * diagnostics and may use the result for both push/pull transports.
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspDiagnosticStore_BuildResultId(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const SZrArray *diagnostics,
        TZrChar *buffer,
        TZrSize bufferLength);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_STORE_H
