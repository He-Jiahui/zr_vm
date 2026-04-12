#include "lsp_module_metadata.h"
#include "lsp_virtual_documents.h"

#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "module_init_analysis.h"

#include <ctype.h>
#include <string.h>

static const TZrChar *module_metadata_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool module_metadata_normalize_module_key(const TZrChar *modulePath,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize) {
    TZrSize length;
    TZrSize writeIndex = 0;

    if (modulePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(modulePath);
    while (length > 0 && (modulePath[length - 1] == '/' || modulePath[length - 1] == '\\')) {
        length--;
    }

    if (length >= 4 && memcmp(modulePath + length - 4, ".zro", 4) == 0) {
        length -= 4;
    } else if (length >= 3 && memcmp(modulePath + length - 3, ".zr", 3) == 0) {
        length -= 3;
    }

    while (length > 0 && (*modulePath == '/' || *modulePath == '\\')) {
        modulePath++;
        length--;
    }

    if (length == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = modulePath[index];
        buffer[writeIndex++] = current == '\\' ? '/' : current;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

static TZrBool module_metadata_resolve_module_file_path(const TZrChar *rootText,
                                                        const TZrChar *moduleName,
                                                        const TZrChar *extension,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;

    if (rootText == ZR_NULL || moduleName == ZR_NULL || extension == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !module_metadata_normalize_module_key(moduleName, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; normalizedModule[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }

    if (writeIndex + strlen(extension) + 1 >= sizeof(relativePath)) {
        return ZR_FALSE;
    }

    memcpy(relativePath + writeIndex, extension, strlen(extension) + 1);
    ZrLibrary_File_PathJoin(rootText, relativePath, buffer);
    return buffer[0] != '\0';
}

static TZrBool module_metadata_project_binary_root(SZrLspProjectIndex *projectIndex,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    const TZrChar *projectDirectory;
    const TZrChar *projectBinary;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }

    if (projectIndex == ZR_NULL || projectIndex->project == ZR_NULL || projectIndex->project->directory == ZR_NULL ||
        projectIndex->project->binary == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    projectDirectory = module_metadata_string_text(projectIndex->project->directory);
    projectBinary = module_metadata_string_text(projectIndex->project->binary);
    if (projectDirectory == ZR_NULL || projectBinary == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, projectBinary, buffer);
    return buffer[0] != '\0';
}

static TZrBool module_metadata_resolve_existing_project_binary_path(SZrLspProjectIndex *projectIndex,
                                                                    const TZrChar *moduleName,
                                                                    const TZrChar *extension,
                                                                    TZrChar *buffer,
                                                                    TZrSize bufferSize) {
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }

    if (moduleName == ZR_NULL || extension == ZR_NULL ||
        !module_metadata_project_binary_root(projectIndex, binaryRoot, sizeof(binaryRoot)) ||
        !module_metadata_resolve_module_file_path(binaryRoot, moduleName, extension, buffer, bufferSize)) {
        return ZR_FALSE;
    }

    return ZrLibrary_File_Exist(buffer) == ZR_LIBRARY_FILE_IS_FILE;
}

static SZrString *module_metadata_create_file_uri_from_native_path(SZrState *state, const TZrChar *path) {
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH * 2];
    TZrSize pathLength;
    TZrSize writeIndex = 0;

    if (state == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    pathLength = strlen(path);
    if (pathLength + 16 >= sizeof(buffer)) {
        return ZR_NULL;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    memcpy(buffer, "file:///", 8);
    writeIndex = 8;
#else
    memcpy(buffer, "file://", 7);
    writeIndex = 7;
#endif

    for (TZrSize index = 0; index < pathLength && writeIndex + 1 < sizeof(buffer); index++) {
        TZrChar ch = path[index] == '\\' ? '/' : path[index];

#ifdef ZR_VM_PLATFORM_IS_WIN
        if (index == 0 && pathLength >= 2 && isalpha((unsigned char)ch) && path[1] == ':') {
            ch = (TZrChar)toupper((unsigned char)ch);
        }
#endif

        buffer[writeIndex++] = ch;
    }

    buffer[writeIndex] = '\0';
    return ZrCore_String_Create(state, buffer, writeIndex);
}

static const ZrLibTypeDescriptor *module_metadata_find_type_descriptor_in_module(const ZrLibModuleDescriptor *module,
                                                                                 const TZrChar *typeName) {
    const TZrChar *start;
    const TZrChar *end;
    const TZrChar *cursor;
    TZrChar baseName[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrSize length;

    if (module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    start = typeName;
    end = typeName + strlen(typeName);
    for (cursor = typeName; *cursor != '\0'; cursor++) {
        if (*cursor == '<' || *cursor == '[') {
            end = cursor;
            break;
        }
    }

    for (cursor = end; cursor > start; cursor--) {
        if (cursor[-1] == '.') {
            start = cursor;
            break;
        }
    }

    length = (TZrSize)(end - start);
    if (length == 0 || length >= sizeof(baseName)) {
        return ZR_NULL;
    }

    memcpy(baseName, start, length);
    baseName[length] = '\0';
    for (TZrSize index = 0; index < module->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &module->types[index];
        if (typeDescriptor->name != ZR_NULL && strcmp(typeDescriptor->name, baseName) == 0) {
            return typeDescriptor;
        }
    }

    return ZR_NULL;
}

static const ZrLibModuleDescriptor *module_metadata_find_registered_native_module(SZrState *state,
                                                                                  const TZrChar *moduleName,
                                                                                  EZrLspImportedModuleSourceKind *outSourceKind) {
    ZrLibRegisteredModuleInfo moduleInfo;
    const ZrLibModuleDescriptor *descriptor;

    if (outSourceKind != ZR_NULL) {
        *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    memset(&moduleInfo, 0, sizeof(moduleInfo));
    if (ZrLibrary_NativeRegistry_GetModuleInfo(state->global, moduleName, &moduleInfo)) {
        if (outSourceKind != ZR_NULL) {
            *outSourceKind =
                moduleInfo.registrationKind == ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN ||
                        moduleInfo.isDescriptorPlugin
                    ? ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN
                    : ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN;
        }
        return moduleInfo.descriptor;
    }

    descriptor = ZrLibrary_NativeRegistry_FindModule(state->global, moduleName);
    if (descriptor != ZR_NULL && outSourceKind != ZR_NULL) {
        *outSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN;
    }
    return descriptor;
}

static TZrBool module_metadata_try_load_project_native_plugin(SZrState *state,
                                                              SZrLspProjectIndex *projectIndex,
                                                              const TZrChar *moduleName) {
    const TZrChar *projectDirectory;

    if (state == ZR_NULL || projectIndex == ZR_NULL || projectIndex->project == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    projectDirectory = module_metadata_string_text(projectIndex->project->directory);
    if (projectDirectory == ZR_NULL || projectDirectory[0] == '\0') {
        return ZR_FALSE;
    }

    return ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(state, projectDirectory, moduleName);
}

static const ZrLibModuleDescriptor *module_metadata_try_resolve_via_parent_module_links(
    SZrState *state,
    SZrLspProjectIndex *projectIndex,
    const TZrChar *moduleName,
    EZrLspImportedModuleSourceKind *outSourceKind) {
    const TZrChar *lastDot;
    TZrChar parentName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize parentLen;
    const TZrChar *childSegment;
    const ZrLibModuleDescriptor *parentDescriptor;
    TZrSize linkIndex;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    lastDot = strrchr(moduleName, '.');
    if (lastDot == ZR_NULL || lastDot == moduleName) {
        return ZR_NULL;
    }

    parentLen = (TZrSize)(lastDot - moduleName);
    if (parentLen == 0 || parentLen + 1 >= sizeof(parentName)) {
        return ZR_NULL;
    }

    memcpy(parentName, moduleName, parentLen);
    parentName[parentLen] = '\0';
    childSegment = lastDot + 1;
    if (childSegment[0] == '\0') {
        return ZR_NULL;
    }

    if (projectIndex != ZR_NULL) {
        module_metadata_try_load_project_native_plugin(state, projectIndex, parentName);
    }

    parentDescriptor = module_metadata_find_registered_native_module(state, parentName, outSourceKind);
    if (parentDescriptor == ZR_NULL) {
        parentDescriptor =
            module_metadata_try_resolve_via_parent_module_links(state, projectIndex, parentName, outSourceKind);
    }

    if (parentDescriptor == ZR_NULL || parentDescriptor->moduleLinks == ZR_NULL ||
        parentDescriptor->moduleLinkCount == 0) {
        return ZR_NULL;
    }

    for (linkIndex = 0; linkIndex < parentDescriptor->moduleLinkCount; linkIndex++) {
        const ZrLibModuleLinkDescriptor *link = &parentDescriptor->moduleLinks[linkIndex];

        if (link->moduleName != ZR_NULL && strcmp(link->moduleName, moduleName) == 0) {
            return module_metadata_find_registered_native_module(state, link->moduleName, outSourceKind);
        }

        if (link->name != ZR_NULL && strcmp(link->name, childSegment) == 0 && link->moduleName != ZR_NULL) {
            return module_metadata_find_registered_native_module(state, link->moduleName, outSourceKind);
        }
    }

    return ZR_NULL;
}

static const ZrLibModuleDescriptor *module_metadata_resolve_native_module_descriptor_internal(
    SZrState *state,
    SZrLspProjectIndex *projectIndex,
    const TZrChar *moduleName,
    EZrLspImportedModuleSourceKind *outSourceKind) {
    const ZrLibModuleDescriptor *descriptor;

    if (projectIndex != ZR_NULL) {
        module_metadata_try_load_project_native_plugin(state, projectIndex, moduleName);
    }

    descriptor = module_metadata_find_registered_native_module(state, moduleName, outSourceKind);
    if (descriptor != ZR_NULL) {
        return descriptor;
    }

    descriptor = module_metadata_try_resolve_via_parent_module_links(state, projectIndex, moduleName, outSourceKind);
    if (descriptor != ZR_NULL) {
        return descriptor;
    }

    return ZR_NULL;
}

const SZrTypePrototypeInfo *ZrLanguageServer_LspModuleMetadata_FindTypePrototype(SZrSemanticAnalyzer *analyzer,
                                                                                 const TZrChar *typeName) {
    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->compilerState->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype =
            (const SZrTypePrototypeInfo *)ZrCore_Array_Get(&analyzer->compilerState->typePrototypes, index);
        const TZrChar *prototypeName;

        if (prototype == ZR_NULL || prototype->name == ZR_NULL) {
            continue;
        }

        prototypeName = module_metadata_string_text(prototype->name);
        if (prototypeName != ZR_NULL && strcmp(prototypeName, typeName) == 0) {
            return prototype;
        }
    }

    return ZR_NULL;
}

const SZrTypePrototypeInfo *ZrLanguageServer_LspModuleMetadata_FindModulePrototype(SZrSemanticAnalyzer *analyzer,
                                                                                   SZrString *moduleName) {
    return ZrLanguageServer_LspModuleMetadata_FindTypePrototype(analyzer,
                                                                module_metadata_string_text(moduleName));
}

TZrBool ZrLanguageServer_LspModuleMetadata_ProjectHasBinaryModule(SZrLspProjectIndex *projectIndex,
                                                                  const TZrChar *moduleName,
                                                                  TZrChar *buffer,
                                                                  TZrSize bufferSize) {
    TZrChar binaryModulePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }

    if (projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                             moduleName,
                                                             ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                                             binaryModulePath,
                                                             sizeof(binaryModulePath))) {
        if (buffer != ZR_NULL && bufferSize > 0) {
            snprintf(buffer, bufferSize, "%s", binaryModulePath);
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(SZrState *state,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrLspProjectIndex *projectIndex,
                                                                 SZrString *moduleName,
                                                                 SZrLspResolvedImportedModule *outResolved) {
    const TZrChar *moduleText;

    if (outResolved == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outResolved, 0, sizeof(*outResolved));
    outResolved->moduleName = moduleName;
    outResolved->projectIndex = projectIndex;
    outResolved->modulePrototype = ZrLanguageServer_LspModuleMetadata_FindModulePrototype(analyzer, moduleName);
    outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;

    if (projectIndex != ZR_NULL) {
        outResolved->sourceRecord = ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, moduleName);
        if (outResolved->sourceRecord != ZR_NULL) {
            outResolved->sourceKind = outResolved->sourceRecord->isFfiWrapperSource
                                          ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
                                          : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
        }
    }

    moduleText = module_metadata_string_text(moduleName);
    if (outResolved->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED &&
        ZrLanguageServer_LspModuleMetadata_ProjectHasBinaryModule(projectIndex, moduleText, ZR_NULL, 0)) {
        outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA;
    }

    if (moduleText != ZR_NULL) {
        EZrLspImportedModuleSourceKind nativeSourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
        outResolved->nativeDescriptor =
            module_metadata_resolve_native_module_descriptor_internal(state,
                                                                      outResolved->sourceKind ==
                                                                              ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED
                                                                          ? projectIndex
                                                                          : ZR_NULL,
                                                                      moduleText,
                                                                      &nativeSourceKind);
        if (outResolved->nativeDescriptor != ZR_NULL &&
            outResolved->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED) {
            outResolved->sourceKind = nativeSourceKind;
        }
    }

    return outResolved->sourceRecord != ZR_NULL ||
           outResolved->modulePrototype != ZR_NULL ||
           outResolved->nativeDescriptor != ZR_NULL ||
           outResolved->sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA;
}

const ZrLibModuleDescriptor *ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(SZrState *state,
                                                                                               const TZrChar *moduleName,
                                                                                               EZrLspImportedModuleSourceKind *outSourceKind) {
    return module_metadata_resolve_native_module_descriptor_internal(state, ZR_NULL, moduleName, outSourceKind);
}

const ZrLibTypeDescriptor *ZrLanguageServer_LspModuleMetadata_FindNativeTypeDescriptor(SZrState *state,
                                                                                       const TZrChar *typeName,
                                                                                       const ZrLibModuleDescriptor **outModule) {
    const ZrLibModuleDescriptor *module;
    const ZrLibTypeDescriptor *typeDescriptor;
    const TZrChar *lastDot;
    TZrChar moduleName[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (outModule != ZR_NULL) {
        *outModule = ZR_NULL;
    }
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    lastDot = strrchr(typeName, '.');
    if (lastDot != ZR_NULL && lastDot != typeName) {
        TZrSize moduleLength = (TZrSize)(lastDot - typeName);
        if (moduleLength > 0 && moduleLength < sizeof(moduleName)) {
            memcpy(moduleName, typeName, moduleLength);
            moduleName[moduleLength] = '\0';
            module = ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state, moduleName, ZR_NULL);
            typeDescriptor = module_metadata_find_type_descriptor_in_module(module, typeName);
            if (typeDescriptor != ZR_NULL) {
                if (outModule != ZR_NULL) {
                    *outModule = module;
                }
                return typeDescriptor;
            }
        }
    }

    for (TZrSize index = 0; index < ZrLibrary_NativeRegistry_GetModuleCount(state->global); index++) {
        ZrLibRegisteredModuleInfo moduleInfo;

        memset(&moduleInfo, 0, sizeof(moduleInfo));
        if (!ZrLibrary_NativeRegistry_GetModuleInfoAt(state->global, index, &moduleInfo)) {
            continue;
        }

        module = moduleInfo.descriptor;
        typeDescriptor = module_metadata_find_type_descriptor_in_module(module, typeName);
        if (typeDescriptor != ZR_NULL) {
            if (outModule != ZR_NULL) {
                *outModule = module;
            }
            return typeDescriptor;
        }
    }

    return ZR_NULL;
}

TZrBool ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrIoSource **outSource) {
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outSource != ZR_NULL) {
        *outSource = ZR_NULL;
    }

    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || outSource == ZR_NULL ||
        !ZrLanguageServer_LspModuleMetadata_ProjectHasBinaryModule(projectIndex,
                                                                  module_metadata_string_text(moduleName),
                                                                  binaryPath,
                                                                  sizeof(binaryPath)) ||
        (strlen(binaryPath) >= 4 &&
         strcmp(binaryPath + strlen(binaryPath) - 4, ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION) == 0)) {
        return ZR_FALSE;
    }

    return ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromPath(state, binaryPath, outSource);
}

void ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(SZrGlobalState *global, SZrIoSource *source) {
    ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(global, source);
}

static const SZrIoFunctionTypedExportSymbol *module_metadata_find_binary_export_symbol(
    const SZrIoFunction *entryFunction,
    SZrString *memberName) {
    if (entryFunction == ZR_NULL || memberName == ZR_NULL || entryFunction->typedExportedSymbols == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &entryFunction->typedExportedSymbols[index];
        if (symbol->name != ZR_NULL && ZrLanguageServer_Lsp_StringsEqual(symbol->name, memberName)) {
            return symbol;
        }
    }

    return ZR_NULL;
}

static TZrBool module_metadata_binary_export_symbol_has_declaration_range(
    const SZrIoFunctionTypedExportSymbol *symbol) {
    return symbol != ZR_NULL && symbol->lineInSourceStart > 0 && symbol->columnInSourceStart > 0 &&
           symbol->lineInSourceEnd > 0 && symbol->columnInSourceEnd > 0;
}

static SZrFileRange module_metadata_binary_export_symbol_range(SZrString *uri,
                                                               const SZrIoFunctionTypedExportSymbol *symbol) {
    SZrFilePosition start;
    SZrFilePosition end;

    start = ZrParser_FilePosition_Create(0,
                                         symbol != ZR_NULL ? (TZrInt32)symbol->lineInSourceStart : 0,
                                         symbol != ZR_NULL ? (TZrInt32)symbol->columnInSourceStart : 0);
    end = ZrParser_FilePosition_Create(0,
                                       symbol != ZR_NULL ? (TZrInt32)symbol->lineInSourceEnd : 0,
                                       symbol != ZR_NULL ? (TZrInt32)symbol->columnInSourceEnd : 0);
    return ZrParser_FileRange_Create(start, end, uri);
}

TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri) {
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outUri != ZR_NULL) {
        *outUri = ZR_NULL;
    }

    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || outUri == ZR_NULL ||
        !module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                              module_metadata_string_text(moduleName),
                                                              ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                                              binaryPath,
                                                              sizeof(binaryPath))) {
        return ZR_FALSE;
    }

    *outUri = module_metadata_create_file_uri_from_native_path(state, binaryPath);
    return *outUri != ZR_NULL;
}

TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryExportDeclaration(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrString **outUri,
                                                                          SZrFileRange *outRange) {
    SZrIoSource *source = ZR_NULL;
    const SZrIoFunctionTypedExportSymbol *symbol = ZR_NULL;
    TZrBool resolved = ZR_FALSE;

    if (outUri != ZR_NULL) {
        *outUri = ZR_NULL;
    }
    if (outRange != ZR_NULL) {
        *outRange = ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 1, 1),
                                              ZrParser_FilePosition_Create(0, 1, 1),
                                              ZR_NULL);
    }
    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || memberName == ZR_NULL ||
        outUri == ZR_NULL || outRange == ZR_NULL ||
        !ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state, projectIndex, moduleName, outUri) ||
        *outUri == ZR_NULL ||
        !ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(state, projectIndex, moduleName, &source) ||
        source == ZR_NULL || source->modulesLength == 0 || source->modules == ZR_NULL ||
        source->modules[0].entryFunction == ZR_NULL) {
        if (source != ZR_NULL) {
            ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(state->global, source);
        }
        return ZR_FALSE;
    }

    symbol = module_metadata_find_binary_export_symbol(source->modules[0].entryFunction, memberName);
    if (module_metadata_binary_export_symbol_has_declaration_range(symbol)) {
        *outRange = module_metadata_binary_export_symbol_range(*outUri, symbol);
        resolved = ZR_TRUE;
    }

    ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(state->global, source);
    return resolved;
}

TZrBool ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri) {
    const TZrChar *moduleText;
    EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;

    if (outUri != ZR_NULL) {
        *outUri = ZR_NULL;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || moduleName == ZR_NULL || outUri == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleText = module_metadata_string_text(moduleName);
    if (moduleText == ZR_NULL ||
        module_metadata_resolve_native_module_descriptor_internal(state,
                                                                  projectIndex,
                                                                  moduleText,
                                                                  &sourceKind) == ZR_NULL ||
        (sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN &&
         sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN)) {
        return ZR_FALSE;
    }

    *outUri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, moduleText);
    return *outUri != ZR_NULL;
}
const TZrChar *ZrLanguageServer_LspModuleMetadata_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind) {
    switch (sourceKind) {
        case ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE:
            return "project source";
        case ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER:
            return "ffi source wrapper";
        case ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA:
            return "binary metadata";
        case ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN:
            return "native builtin";
        case ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN:
            return "native descriptor plugin";
        default:
            return "external/unresolved";
    }
}
