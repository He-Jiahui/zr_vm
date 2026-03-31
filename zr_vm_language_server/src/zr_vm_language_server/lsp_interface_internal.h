#ifndef ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H

#include "zr_vm_language_server/lsp_interface.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

TZrSize ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrInt32 line,
                                                           TZrInt32 column);

TZrBool ZrLanguageServer_Lsp_StringsEqual(SZrString *left, SZrString *right);
TZrBool ZrLanguageServer_Lsp_StringContainsCaseInsensitive(SZrString *haystack, SZrString *needle);
SZrFileRange ZrLanguageServer_Lsp_GetSymbolLookupRange(SZrSymbol *symbol);
SZrString *ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(SZrState *state,
                                                                 SZrSymbol *symbol,
                                                                 const TZrChar *content,
                                                                 TZrSize contentLength);
void ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrCompletionItem *item,
                                                       const TZrChar *content,
                                                       TZrSize contentLength);
void ZrLanguageServer_Lsp_AppendDiagnostic(SZrState *state, SZrArray *result, SZrDiagnostic *diag);
SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformation(SZrState *state,
                                                                      SZrSymbol *symbol);
TZrBool ZrLanguageServer_Lsp_TryCollectReceiverCompletions(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNode *ast,
                                                           const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrSize cursorOffset,
                                                           SZrArray *result);
SZrSymbol *ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(SZrSemanticAnalyzer *analyzer,
                                                              SZrFileRange position);

#endif
