#ifndef ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_library/project.h"
#include "interface/lsp_position_codec.h"
#include "interface/lsp_semantic_cache_lru.h"
#include "interface/lsp_semantic_snapshot_cache.h"
#include "interface/lsp_workspace_edit_snapshot.h"

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
TZrBool ZrLanguageServer_Lsp_UrisResolveToSameNativePath(SZrString *left, SZrString *right);
SZrHashKeyValuePair *ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(SZrState *state,
                                                                   SZrHashSet *set,
                                                                   SZrString *uri);
SZrFileRange ZrLanguageServer_Lsp_GetSymbolLookupRange(SZrSymbol *symbol);
TZrBool ZrLanguageServer_Lsp_IsOffsetInCodeSpan(const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize offset);
TZrBool ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan(const TZrChar *content,
                                                      TZrSize contentLength,
                                                      TZrSize offset);
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrSymbol *symbol,
    const TZrChar *content,
    TZrSize contentLength);
SZrString *ZrLanguageServer_Lsp_AppendSymbolFfiMetadataMarkdown(SZrState *state,
                                                                SZrString *base,
                                                                SZrSymbol *symbol);
void ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrCompletionItem *item,
                                                       SZrString *hoveredSymbolName,
                                                       SZrString *resolvedTypeText,
                                                       const TZrChar *content,
                                                       TZrSize contentLength);
void ZrLanguageServer_Lsp_EnrichCompletionItemSemanticFacts(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrSymbol *symbol,
                                                            SZrCompletionItem *item);
SZrString *ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *argumentNode);
SZrString *ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(SZrState *state,
                                                              SZrSymbol *symbol,
                                                              const TZrChar *content,
                                                              TZrSize contentLength);
TZrBool ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(SZrState *state,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrString *uri,
                                                            SZrAstNode *ast,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            TZrSize cursorOffset,
                                                            SZrLspResolvedMetadataMember *outResolved);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_TryResolveReceiverProjectMember(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrSemanticAnalyzer *analyzer,
    SZrString *uri,
    SZrAstNode *ast,
    const TZrChar *content,
    TZrSize contentLength,
    TZrSize cursorOffset,
    SZrLspResolvedMetadataMember *outResolved);
void ZrLanguageServer_Lsp_AppendDiagnostic(SZrState *state, SZrArray *result, SZrDiagnostic *diag);
void ZrLanguageServer_Lsp_AppendDiagnosticForDocument(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrArray *result,
                                                      SZrDiagnostic *diag);
SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformation(SZrState *state,
                                                                      SZrSymbol *symbol);
SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformationForDocument(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
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
                                                           SZrArray *result,
                                                           TZrBool *outFailClosed);
TZrBool ZrLanguageServer_Lsp_ShouldFailClosedReceiverCompletion(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ast,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize cursorOffset);
TZrBool ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(SZrState *state,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize cursorOffset,
                                                              SZrArray *result);
TZrBool ZrLanguageServer_Lsp_IsKnownMetaMethodToken(const TZrChar *text, TZrSize length);
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange position);
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrLanguageServer_Lsp_GetDocumentFileVersion(SZrLspContext *context,
                                                                                   SZrString *uri);
ZR_LANGUAGE_SERVER_API SZrFilePosition ZrLanguageServer_Lsp_GetDocumentFilePosition(SZrLspContext *context,
                                                                                      SZrString *uri,
                                                                                      SZrLspPosition position);
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
    SZrLspContext *context,
    SZrString *uri,
    SZrFilePosition position);
ZR_LANGUAGE_SERVER_API SZrLspRange ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(SZrLspContext *context,
                                                                                         SZrString *uri,
                                                                                         SZrFileRange range);
TZrBool ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates(SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrFileRange range,
                                                                   SZrLspRange *outRange);
TZrBool ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates(SZrLspContext *context,
                                                                          SZrString *uri,
                                                                          SZrLspPosition position,
                                                                          SZrFilePosition *outPosition);

typedef struct SZrLspProjectFileRecord {
    SZrString *uri;
    SZrString *path;
    SZrString *moduleName;
    TZrBool isFfiWrapperSource;
    TZrUInt64 publicContractHash;
    TZrSize publicContractExportCount;
    TZrBool hasPublicContractHash;
} SZrLspProjectFileRecord;

typedef struct SZrLspProjectIndex {
    SZrLibrary_Project *project;
    SZrString *projectFileUri;
    SZrString *projectFilePath;
    SZrString *projectRootPath;
    SZrString *sourceRootPath;
    TZrBool hasSemanticProjectLoad;
    TZrBool hasLightweightSourceGraph;
    TZrSize reverseDependencyPreservationCount;
    TZrSize reverseDependencyReanalysisCount;
    TZrSize lastReverseDependencyReanalysisCount;
    TZrSize publicContractHashMatchCount;
    TZrSize publicContractHashChangeCount;
    TZrSize publicContractHashUnavailableCount;
    SZrArray files; // SZrLspProjectFileRecord*
} SZrLspProjectIndex;

typedef enum EZrLspImportedModuleSourceKind {
    ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED = 0,
    ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE = 1,
    ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER = 2,
    ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA = 3,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN = 4,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN = 5,
    ZR_LSP_IMPORTED_MODULE_SOURCE_COMPILE_TOOL = 6
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

ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *ZrLanguageServer_Lsp_FindAnalyzer(SZrState *state,
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
                                                              TZrSize contentLength,
                                                              TZrBool rescanAllLoadedSources);
TZrBool ZrLanguageServer_Lsp_ProjectAnalyzeDocument(SZrState *state,
                                                    SZrLspContext *context,
                                                    SZrString *uri,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *ast);
SZrLspProjectIndex *ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(SZrState *state,
                                                                    SZrLspContext *context,
                                                                    SZrString *uri);
SZrLspProjectIndex *ZrLanguageServer_Lsp_ProjectEnsureProjectByProjectUri(SZrState *state,
                                                                          SZrLspContext *context,
                                                                          SZrString *projectUri);
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
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectForUri(SZrLspContext *context, SZrString *uri);
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByModuleName(SZrLspProjectIndex *projectIndex,
                                                                            SZrString *moduleName);
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
