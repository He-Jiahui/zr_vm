#ifndef ZR_VM_LANGUAGE_SERVER_LSP_MODULE_METADATA_H
#define ZR_VM_LANGUAGE_SERVER_LSP_MODULE_METADATA_H

#include "lsp_project_internal.h"

#include "zr_vm_core/io.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compiler.h"

typedef enum EZrLspImportedModuleSourceKind {
    ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED = 0,
    ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE = 1,
    ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA = 2,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN = 3,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN = 4
} EZrLspImportedModuleSourceKind;

typedef struct SZrLspResolvedImportedModule {
    SZrString *moduleName;
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *sourceRecord;
    const ZrLibModuleDescriptor *nativeDescriptor;
    const SZrTypePrototypeInfo *modulePrototype;
    EZrLspImportedModuleSourceKind sourceKind;
} SZrLspResolvedImportedModule;

typedef struct SZrLspIntermediateExportSymbol {
    SZrString *name;
    SZrString *typeName;
    TZrBool isCallable;
    TZrInt32 declarationLine;
    TZrInt32 declarationStartColumn;
    TZrInt32 declarationEndColumn;
} SZrLspIntermediateExportSymbol;

typedef struct SZrLspIntermediateModuleMetadata {
    SZrArray exportedSymbols;
} SZrLspIntermediateModuleMetadata;

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
TZrBool ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleFunction(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrFunction **outFunction);
TZrBool ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleMetadata(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrLspIntermediateModuleMetadata *outMetadata);
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri);
const SZrLspIntermediateExportSymbol *ZrLanguageServer_LspModuleMetadata_FindIntermediateExportSymbol(
    const SZrLspIntermediateModuleMetadata *metadata,
    SZrString *symbolName);
void ZrLanguageServer_LspModuleMetadata_FreeIntermediateModuleMetadata(SZrState *state,
                                                                       SZrLspIntermediateModuleMetadata *metadata);
const TZrChar *ZrLanguageServer_LspModuleMetadata_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind);

#endif
