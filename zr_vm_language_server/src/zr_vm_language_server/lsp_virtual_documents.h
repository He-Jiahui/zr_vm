#ifndef ZR_VM_LANGUAGE_SERVER_LSP_VIRTUAL_DOCUMENTS_H
#define ZR_VM_LANGUAGE_SERVER_LSP_VIRTUAL_DOCUMENTS_H

#include "lsp_module_metadata.h"

typedef enum EZrLspVirtualDeclarationKind {
    ZR_LSP_VIRTUAL_DECLARATION_MODULE = 0,
    ZR_LSP_VIRTUAL_DECLARATION_MODULE_LINK = 1,
    ZR_LSP_VIRTUAL_DECLARATION_CONSTANT = 2,
    ZR_LSP_VIRTUAL_DECLARATION_FUNCTION = 3,
    ZR_LSP_VIRTUAL_DECLARATION_TYPE = 4,
    ZR_LSP_VIRTUAL_DECLARATION_FIELD = 5,
    ZR_LSP_VIRTUAL_DECLARATION_METHOD = 6,
    ZR_LSP_VIRTUAL_DECLARATION_META_METHOD = 7
} EZrLspVirtualDeclarationKind;

typedef struct SZrLspVirtualDeclarationMatch {
    EZrLspVirtualDeclarationKind kind;
    const ZrLibModuleDescriptor *descriptor;
    const TZrChar *moduleName;
    const TZrChar *ownerName;
    const TZrChar *name;
    const TZrChar *targetModuleName;
    SZrFileRange range;
} SZrLspVirtualDeclarationMatch;

TZrBool ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(SZrString *uri);
SZrString *ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(SZrState *state, const TZrChar *moduleName);
TZrBool ZrLanguageServer_LspVirtualDocuments_ParseDeclarationUri(SZrString *uri,
                                                                 TZrChar *moduleNameBuffer,
                                                                 TZrSize bufferSize);
TZrBool ZrLanguageServer_LspVirtualDocuments_ResolveDescriptorForUri(SZrState *state,
                                                                     SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri,
                                                                     const ZrLibModuleDescriptor **outDescriptor,
                                                                     EZrLspImportedModuleSourceKind *outSourceKind,
                                                                     TZrChar *moduleNameBuffer,
                                                                     TZrSize bufferSize);
TZrBool ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(SZrState *state,
                                                                   const ZrLibModuleDescriptor *descriptor,
                                                                   SZrString *uri,
                                                                   SZrString **outText);
SZrFileRange ZrLanguageServer_LspVirtualDocuments_ModuleEntryRange(SZrString *uri);
TZrBool ZrLanguageServer_LspVirtualDocuments_FindModuleLinkDeclaration(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       const TZrChar *linkName,
                                                                       SZrFileRange *outRange,
                                                                       const ZrLibModuleLinkDescriptor **outLink);
TZrBool ZrLanguageServer_LspVirtualDocuments_FindTypeDeclaration(SZrState *state,
                                                                 const ZrLibModuleDescriptor *descriptor,
                                                                 SZrString *uri,
                                                                 const TZrChar *typeName,
                                                                 SZrFileRange *outRange,
                                                                 const ZrLibTypeDescriptor **outType);
TZrBool ZrLanguageServer_LspVirtualDocuments_FindTypeMemberDeclaration(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       const TZrChar *typeName,
                                                                       const TZrChar *memberName,
                                                                       TZrInt32 memberKind,
                                                                       SZrFileRange *outRange);
TZrBool ZrLanguageServer_LspVirtualDocuments_FindDeclarationAtPosition(SZrState *state,
                                                                       const ZrLibModuleDescriptor *descriptor,
                                                                       SZrString *uri,
                                                                       SZrLspPosition position,
                                                                       SZrLspVirtualDeclarationMatch *outMatch);

#endif
