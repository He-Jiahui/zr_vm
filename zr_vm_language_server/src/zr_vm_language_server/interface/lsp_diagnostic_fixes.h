#ifndef ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_FIXES_H
#define ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_FIXES_H

#include "interface/lsp_interface_internal.h"

void ZrLanguageServer_Lsp_CopyDiagnosticFixes(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              const SZrDiagnostic *diagnostic,
                                              SZrLspDiagnostic *lspDiagnostic);

#endif
