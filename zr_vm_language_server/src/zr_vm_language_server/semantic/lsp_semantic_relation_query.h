#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_RELATION_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_RELATION_QUERY_H

#include "metadata/lsp_metadata_provider.h"

typedef struct SZrLspSemanticImportOriginTarget {
    SZrString *originIdentity;
    SZrString *virtualDeclarationUri;
    SZrFileRange referenceRange;
    SZrLspResolvedImportedModule module;
    SZrLspResolvedImportedModuleEntry declaration;
} SZrLspSemanticImportOriginTarget;

typedef enum EZrLspSemanticImportOriginResolution {
    ZR_LSP_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE = 0,
    ZR_LSP_SEMANTIC_IMPORT_ORIGIN_RESOLVED,
    ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID
} EZrLspSemanticImportOriginResolution;

EZrLspSemanticImportOriginResolution
ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOrigin(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *symbol,
        SZrLspSemanticImportOriginTarget *outTarget);
EZrLspSemanticImportOriginResolution
ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOriginAt(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrLspSemanticImportOriginTarget *outTarget);

#endif
