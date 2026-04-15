#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_QUERY_H

#include "metadata/lsp_metadata_provider.h"

typedef enum EZrLspResolvedValueKind {
    ZR_LSP_RESOLVED_VALUE_KIND_UNKNOWN = 0,
    ZR_LSP_RESOLVED_VALUE_KIND_SYMBOL = 1,
    ZR_LSP_RESOLVED_VALUE_KIND_MODULE = 2,
    ZR_LSP_RESOLVED_VALUE_KIND_CALLABLE = 3,
    ZR_LSP_RESOLVED_VALUE_KIND_TYPE = 4
} EZrLspResolvedValueKind;

typedef struct SZrLspResolvedTypeInfo {
    SZrString *resolvedTypeText;
    EZrLspResolvedValueKind valueKind;
    EZrLspImportedModuleSourceKind origin;
} SZrLspResolvedTypeInfo;

typedef enum EZrLspSemanticQueryTargetKind {
    ZR_LSP_SEMANTIC_QUERY_TARGET_NONE = 0,
    ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL = 1,
    ZR_LSP_SEMANTIC_QUERY_TARGET_IMPORTED_MEMBER = 2,
    ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_DECLARATION = 3,
    ZR_LSP_SEMANTIC_QUERY_TARGET_EXTERNAL_METADATA_TYPE_MEMBER = 4
} EZrLspSemanticQueryTargetKind;

typedef struct SZrLspSemanticQuery {
    EZrLspSemanticQueryTargetKind kind;
    SZrString *uri;
    SZrLspPosition position;
    SZrFileRange queryRange;
    SZrLspProjectIndex *projectIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbol *symbol;
    SZrString *moduleName;
    SZrString *memberName;
    EZrLspImportedModuleSourceKind sourceKind;
    SZrLspResolvedTypeInfo resolvedTypeInfo;
    SZrLspResolvedImportedModule resolvedModule;
    SZrLspResolvedMetadataMember resolvedMember;
} SZrLspSemanticQuery;

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticQuery_Init(SZrLspSemanticQuery *query);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSemanticQuery_Free(SZrState *state,
                                                                   SZrLspSemanticQuery *query);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspSemanticQuery *query);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_BuildHover(SZrState *state,
                                                                            SZrLspContext *context,
                                                                            SZrLspSemanticQuery *query,
                                                                            SZrLspHover **result);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_CollectCompletionItems(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrArray *result);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDefinitions(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendReferences(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    TZrBool includeDeclaration,
    SZrArray *result);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSemanticQuery_AppendDocumentHighlights(
    SZrState *state,
    SZrLspContext *context,
    SZrLspSemanticQuery *query,
    SZrArray *result);

#endif
