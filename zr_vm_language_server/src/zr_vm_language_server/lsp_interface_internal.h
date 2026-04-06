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

typedef struct SZrLspResolvedMetadataMember SZrLspResolvedMetadataMember;
typedef struct SZrLspProjectIndex SZrLspProjectIndex;

TZrSize ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrInt32 line,
                                                           TZrInt32 column);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_FileUriToNativePath(SZrString *uri,
                                                                        TZrChar *buffer,
                                                                        TZrSize bufferSize);

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
TZrBool ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(SZrState *state,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNode *ast,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            TZrSize cursorOffset,
                                                            SZrLspResolvedMetadataMember *outResolved);
void ZrLanguageServer_Lsp_AppendDiagnostic(SZrState *state, SZrArray *result, SZrDiagnostic *diag);
SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformation(SZrState *state,
                                                                      SZrSymbol *symbol);
TZrBool ZrLanguageServer_Lsp_TryCollectReceiverCompletions(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrString *uri,
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
TZrBool ZrLanguageServer_Lsp_IsKnownDirectiveToken(const TZrChar *text, TZrSize length);
TZrBool ZrLanguageServer_Lsp_IsKnownMetaMethodToken(const TZrChar *text, TZrSize length);
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
    TZrBool isFfiWrapperSource;
} SZrLspProjectFileRecord;

typedef struct SZrLspProjectIndex {
    SZrLibrary_Project *project;
    SZrString *projectFileUri;
    SZrString *projectFilePath;
    SZrString *projectRootPath;
    SZrString *sourceRootPath;
    SZrArray files; // SZrLspProjectFileRecord*
} SZrLspProjectIndex;

typedef enum EZrLspImportedModuleSourceKind {
    ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED = 0,
    ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE = 1,
    ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER = 2,
    ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA = 3,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN = 4,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN = 5
} EZrLspImportedModuleSourceKind;

typedef struct SZrLspExternalMetadataDeclaration {
    SZrLspProjectIndex *projectIndex;
    SZrString *moduleName;
    SZrString *memberName;
    TZrInt32 sourceKind;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
} SZrLspExternalMetadataDeclaration;

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
TZrBool ZrLanguageServer_Lsp_ProjectTryGetDocumentHighlights(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrString *uri,
                                                             SZrLspPosition position,
                                                             SZrArray *result);
TZrBool ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri,
                                                                       SZrLspPosition position,
                                                                       SZrLspExternalMetadataDeclaration *outResolved);
TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationReferences(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    TZrBool includeDeclaration,
    SZrArray *result);
TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationHighlights(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    SZrArray *result);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_ProjectContainsUri(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri);
TZrBool ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *query,
                                                           SZrArray *result);
TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDefinition(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrLspPosition position,
                                                              SZrArray *result);
TZrBool ZrLanguageServer_Lsp_TryFindSuperConstructorReferences(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrLspPosition position,
                                                               TZrBool includeDeclaration,
                                                               SZrArray *result);
TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDocumentHighlights(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result);
TZrBool ZrLanguageServer_Lsp_TryGetDecoratorDefinition(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrLspPosition position,
                                                       SZrArray *result);
TZrBool ZrLanguageServer_Lsp_TryGetDecoratorHover(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  SZrLspHover **result);
TZrBool ZrLanguageServer_Lsp_TryGetMetaMethodHover(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri,
                                                   SZrLspPosition position,
                                                   SZrLspHover **result);

#endif
