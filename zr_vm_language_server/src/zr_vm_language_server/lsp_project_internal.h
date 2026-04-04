#ifndef ZR_VM_LANGUAGE_SERVER_LSP_PROJECT_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_PROJECT_INTERNAL_H

#include "lsp_interface_internal.h"

typedef struct SZrLspImportBinding {
    SZrString *aliasName;
    SZrString *moduleName;
    SZrFileRange modulePathLocation;
} SZrLspImportBinding;

typedef struct SZrLspImportedMemberHit {
    SZrString *moduleName;
    SZrString *memberName;
    SZrFileRange location;
} SZrLspImportedMemberHit;

typedef struct SZrLspProjectResolvedSymbol {
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *record;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbol *symbol;
} SZrLspProjectResolvedSymbol;

SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByUri(SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri);
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByModuleName(SZrLspProjectIndex *projectIndex,
                                                                            SZrString *moduleName);
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectByProjectUri(SZrLspContext *context,
                                                                        SZrString *uri,
                                                                        TZrSize *outIndex);
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectForUri(SZrLspContext *context, SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                                                     SZrLspContext *context,
                                                                                     SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                                                 SZrLspContext *context,
                                                                                 SZrString *uri);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                                            SZrLspContext *context,
                                                                                            SZrString *uri);

void ZrLanguageServer_LspProject_FreeImportBindings(SZrState *state, SZrArray *bindings);
void ZrLanguageServer_LspProject_CollectImportBindings(SZrState *state, SZrAstNode *node, SZrArray *bindings);
SZrLspImportBinding *ZrLanguageServer_LspProject_FindImportBindingByAlias(SZrArray *bindings, SZrString *aliasName);
TZrBool ZrLanguageServer_LspProject_FindImportBindingHit(SZrAstNode *node,
                                                         SZrArray *bindings,
                                                         SZrFileRange position,
                                                         SZrLspImportBinding **outBinding,
                                                         SZrFileRange *outLocation);
TZrBool ZrLanguageServer_LspProject_FindImportedMemberHit(SZrAstNode *node,
                                                          SZrArray *bindings,
                                                          SZrFileRange position,
                                                          SZrLspImportedMemberHit *outHit);
TZrBool ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(SZrState *state,
                                                                          SZrAstNode *node,
                                                                          SZrArray *bindings,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrArray *result);
TZrBool ZrLanguageServer_LspProject_AppendMatchingImportedModuleLocations(SZrState *state,
                                                                          SZrAstNode *node,
                                                                          SZrArray *bindings,
                                                                          SZrString *moduleName,
                                                                          SZrArray *result);
TZrBool ZrLanguageServer_LspProject_DeriveBinaryModuleNameFromPath(SZrLspProjectIndex *projectIndex,
                                                                   const TZrChar *path,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize);

#endif
