#ifndef ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_FIXES_H
#define ZR_VM_LANGUAGE_SERVER_LSP_DIAGNOSTIC_FIXES_H

#include "interface/lsp_interface_internal.h"

/** 将解析器诊断的结构化修复借用到 LSP 诊断，并按当前文档投影编辑范围。 */
void ZrLanguageServer_Lsp_CopyDiagnosticFixes(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              const SZrDiagnostic *diagnostic,
                                              SZrLspDiagnostic *lspDiagnostic);

#endif
