#ifndef ZR_VM_LANGUAGE_SERVER_LSP_MODULE_METADATA_H
#define ZR_VM_LANGUAGE_SERVER_LSP_MODULE_METADATA_H

#include "lsp_project_internal.h"

#include "zr_vm_core/io.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compiler.h"

typedef struct SZrLspResolvedImportedModule {
    SZrString *moduleName;
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *sourceRecord;
    const ZrLibModuleDescriptor *nativeDescriptor;
    const SZrTypePrototypeInfo *modulePrototype;
    EZrLspImportedModuleSourceKind sourceKind;
} SZrLspResolvedImportedModule;

const SZrTypePrototypeInfo *ZrLanguageServer_LspModuleMetadata_FindTypePrototype(SZrSemanticAnalyzer *analyzer,
                                                                                 const TZrChar *typeName);
const SZrTypePrototypeInfo *ZrLanguageServer_LspModuleMetadata_FindModulePrototype(SZrSemanticAnalyzer *analyzer,
                                                                                   SZrString *moduleName);
TZrBool ZrLanguageServer_LspModuleMetadata_ProjectHasBinaryModule(SZrLspProjectIndex *projectIndex,
                                                                  const TZrChar *moduleName,
                                                                  TZrChar *buffer,
                                                                  TZrSize bufferSize);
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(SZrState *state,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrLspProjectIndex *projectIndex,
                                                                 SZrString *moduleName,
                                                                 SZrLspResolvedImportedModule *outResolved);
const ZrLibModuleDescriptor *ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(SZrState *state,
                                                                                               const TZrChar *moduleName,
                                                                                               EZrLspImportedModuleSourceKind *outSourceKind);
const ZrLibTypeDescriptor *ZrLanguageServer_LspModuleMetadata_FindNativeTypeDescriptor(SZrState *state,
                                                                                       const TZrChar *typeName,
                                                                                       const ZrLibModuleDescriptor **outModule);
TZrBool ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrIoSource **outSource);
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri);
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryExportDeclaration(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrString **outUri,
                                                                          SZrFileRange *outRange);
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri);
const TZrChar *ZrLanguageServer_LspModuleMetadata_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind);

#endif
