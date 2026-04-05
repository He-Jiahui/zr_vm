#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPORT_CHAIN_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPORT_CHAIN_H

#include "lsp_metadata_provider.h"
#include "lsp_project_internal.h"

typedef struct SZrLspSemanticImportChainHit {
    SZrString *moduleName;
    SZrString *memberName;
    SZrFileRange location;
    SZrLspResolvedMetadataMember resolvedMember;
} SZrLspSemanticImportChainHit;

TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveLinkedMember(
    SZrState *state,
    SZrLspContext *context,
    SZrSemanticAnalyzer *analyzer,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolvedMember,
    SZrString **outNextModuleName);
TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveCompletionModuleAtOffset(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrSemanticAnalyzer *analyzer,
    SZrArray *bindings,
    const TZrChar *content,
    TZrSize contentLength,
    TZrSize cursorOffset,
    SZrLspResolvedImportedModule *outResolvedModule);
TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveAtRange(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrLspProjectIndex *projectIndex,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrArray *bindings,
                                                               SZrFileRange queryRange,
                                                               SZrLspSemanticImportChainHit *outHit);
TZrBool ZrLanguageServer_LspSemanticImportChain_AppendMatchingLocationsForUri(SZrState *state,
                                                                              SZrLspContext *context,
                                                                              SZrLspProjectIndex *projectIndex,
                                                                              SZrString *uri,
                                                                              SZrString *moduleName,
                                                                              SZrString *memberName,
                                                                              SZrArray *result);

#endif
