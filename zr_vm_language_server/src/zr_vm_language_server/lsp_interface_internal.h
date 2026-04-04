#ifndef ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_library/project.h"

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
                                                       SZrString *hoveredSymbolName,
                                                       SZrString *resolvedTypeText,
                                                       const TZrChar *content,
                                                       TZrSize contentLength);
SZrString *ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(SZrState *state,
                                                              SZrSymbol *symbol,
                                                              const TZrChar *content,
                                                              TZrSize contentLength);
SZrString *ZrLanguageServer_Lsp_ParseResolvedTypeFromHoverMarkdown(SZrState *state,
                                                                   SZrString *hoverMarkdown);
SZrString *ZrLanguageServer_Lsp_TryBuildReceiverNativeHoverMarkdown(SZrState *state,
                                                                    SZrSemanticAnalyzer *analyzer,
                                                                    SZrAstNode *ast,
                                                                    const TZrChar *content,
                                                                    TZrSize contentLength,
                                                                    TZrSize cursorOffset);
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
TZrBool ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(SZrState *state,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize cursorOffset,
                                                              SZrArray *result);
SZrSymbol *ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(SZrSemanticAnalyzer *analyzer,
                                                              SZrFileRange position);
SZrFileVersion *ZrLanguageServer_Lsp_GetDocumentFileVersion(SZrLspContext *context, SZrString *uri);
SZrFilePosition ZrLanguageServer_Lsp_GetDocumentFilePosition(SZrLspContext *context,
                                                             SZrString *uri,
                                                             SZrLspPosition position);

typedef struct SZrLspProjectFileRecord {
    SZrString *uri;
    SZrString *path;
    SZrString *moduleName;
} SZrLspProjectFileRecord;

typedef struct SZrLspProjectIndex {
    SZrLibrary_Project *project;
    SZrString *projectFileUri;
    SZrString *projectFilePath;
    SZrString *projectRootPath;
    SZrString *sourceRootPath;
    SZrArray files; // SZrLspProjectFileRecord*
} SZrLspProjectIndex;

SZrSemanticAnalyzer *ZrLanguageServer_Lsp_GetOrCreateAnalyzer(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri);
SZrSemanticAnalyzer *ZrLanguageServer_Lsp_FindAnalyzer(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri);
void ZrLanguageServer_Lsp_RemoveAnalyzer(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri);
TZrBool ZrLanguageServer_Lsp_UpdateDocumentCore(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize version,
                                                TZrBool allowProjectRefresh);
void ZrLanguageServer_Lsp_ProjectIndexes_Free(SZrState *state, SZrLspContext *context);
TZrBool ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength);
SZrLspProjectIndex *ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(SZrState *state,
                                                                    SZrLspContext *context,
                                                                    SZrString *uri);
TZrBool ZrLanguageServer_Lsp_ProjectTryGetDefinition(SZrState *state,
                                                     SZrLspContext *context,
                                                     SZrString *uri,
                                                     SZrLspPosition position,
                                                     SZrArray *result);
TZrBool ZrLanguageServer_Lsp_ProjectTryFindReferences(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrLspPosition position,
                                                      TZrBool includeDeclaration,
                                                      SZrArray *result);
TZrBool ZrLanguageServer_Lsp_ProjectContainsUri(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri);
TZrBool ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *query,
                                                           SZrArray *result);
TZrBool ZrLanguageServer_Lsp_ProjectTryGetHover(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrLspPosition position,
                                                SZrLspHover **result);
TZrBool ZrLanguageServer_Lsp_ProjectTryCollectImportCompletions(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                const TZrChar *content,
                                                                TZrSize contentLength,
                                                                TZrSize cursorOffset,
                                                                SZrFileRange cursorRange,
                                                                SZrArray *result);

#endif
