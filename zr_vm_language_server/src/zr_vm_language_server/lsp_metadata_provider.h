#ifndef ZR_VM_LANGUAGE_SERVER_LSP_METADATA_PROVIDER_H
#define ZR_VM_LANGUAGE_SERVER_LSP_METADATA_PROVIDER_H

#include "lsp_module_metadata.h"

typedef enum EZrLspMetadataMemberKind {
    ZR_LSP_METADATA_MEMBER_NONE = 0,
    ZR_LSP_METADATA_MEMBER_MODULE = 1,
    ZR_LSP_METADATA_MEMBER_CONSTANT = 2,
    ZR_LSP_METADATA_MEMBER_FUNCTION = 3,
    ZR_LSP_METADATA_MEMBER_TYPE = 4,
    ZR_LSP_METADATA_MEMBER_FIELD = 5,
    ZR_LSP_METADATA_MEMBER_METHOD = 6
} EZrLspMetadataMemberKind;

typedef struct SZrLspMetadataProvider {
    SZrState *state;
    SZrLspContext *context;
} SZrLspMetadataProvider;

typedef struct SZrLspResolvedMetadataMember {
    SZrLspResolvedImportedModule module;
    SZrString *memberName;
    EZrLspMetadataMemberKind memberKind;
    const ZrLibModuleLinkDescriptor *moduleLinkDescriptor;
    const ZrLibConstantDescriptor *constantDescriptor;
    const ZrLibFunctionDescriptor *functionDescriptor;
    const ZrLibTypeDescriptor *typeDescriptor;
    const ZrLibFieldDescriptor *fieldDescriptor;
    const ZrLibMethodDescriptor *methodDescriptor;
    const ZrLibTypeDescriptor *ownerTypeDescriptor;
    const ZrLibTypeHintDescriptor *typeHintDescriptor;
    SZrString *ownerTypeName;
    SZrSemanticAnalyzer *declarationAnalyzer;
    SZrSymbol *declarationSymbol;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    SZrString *resolvedTypeText;
    TZrBool hasDeclaration;
} SZrLspResolvedMetadataMember;

typedef struct SZrLspResolvedImportedModuleEntry {
    SZrLspResolvedImportedModule module;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
} SZrLspResolvedImportedModuleEntry;

void ZrLanguageServer_LspMetadataProvider_Init(SZrLspMetadataProvider *provider,
                                               SZrState *state,
                                               SZrLspContext *context);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(SZrLspMetadataProvider *provider,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrLspProjectIndex *projectIndex,
                                                                   SZrString *moduleName,
                                                                   SZrLspResolvedImportedModule *outResolved);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(SZrLspMetadataProvider *provider,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrLspProjectIndex *projectIndex,
                                                                   SZrString *moduleName,
                                                                   SZrString *memberName,
                                                                   SZrLspResolvedMetadataMember *outResolved);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveImportedModuleEntry(SZrLspMetadataProvider *provider,
                                                                        SZrSemanticAnalyzer *analyzer,
                                                                        SZrLspProjectIndex *projectIndex,
                                                                        SZrString *moduleName,
                                                                        SZrLspResolvedImportedModuleEntry *outResolved);
TZrBool ZrLanguageServer_LspMetadataProvider_CreateImportedModuleHover(SZrLspMetadataProvider *provider,
                                                                       const SZrLspResolvedImportedModule *resolvedModule,
                                                                       SZrFileRange range,
                                                                       SZrLspHover **result);
TZrBool ZrLanguageServer_LspMetadataProvider_CreateImportedMemberHover(SZrLspMetadataProvider *provider,
                                                                       SZrSemanticAnalyzer *analyzer,
                                                                       const SZrLspResolvedMetadataMember *resolvedMember,
                                                                       SZrFileRange range,
                                                                       SZrLspHover **result);
TZrBool ZrLanguageServer_LspMetadataProvider_AppendImportedModuleCompletions(
    SZrLspMetadataProvider *provider,
    SZrSemanticAnalyzer *analyzer,
    const SZrLspResolvedImportedModule *resolvedModule,
    SZrArray *result);
TZrBool ZrLanguageServer_LspMetadataProvider_LoadBinaryModuleSource(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrIoSource **outSource);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryModuleUri(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrString **outUri);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveBinaryExportDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrString **outUri,
    SZrFileRange *outRange);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveNativeModuleUri(SZrLspMetadataProvider *provider,
                                                                    SZrLspProjectIndex *projectIndex,
                                                                    SZrString *moduleName,
                                                                    SZrString **outUri);
TZrBool ZrLanguageServer_LspMetadataProvider_ResolveNativeTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrLspResolvedMetadataMember *resolvedMember);
TZrBool ZrLanguageServer_LspMetadataProvider_FindNativeTypeMemberDeclaration(
    SZrLspMetadataProvider *provider,
    SZrLspProjectIndex *projectIndex,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspResolvedMetadataMember *outResolved);
const TZrChar *ZrLanguageServer_LspMetadataProvider_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind);

#endif
