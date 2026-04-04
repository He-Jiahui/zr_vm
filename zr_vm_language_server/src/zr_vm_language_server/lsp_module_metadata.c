#include "lsp_module_metadata.h"

#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"

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
    } else if (length >= 4 && memcmp(modulePath + length - 4, ".zri", 4) == 0) {
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
        buffer[writeIndex++] = path[index] == '\\' ? '/' : path[index];
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

    if (module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                             moduleName,
                                                             ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
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
    static const TZrChar *builtinModules[] = {
        "zr.math",
        "zr.container",
        "zr.system"
    };
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

    for (TZrSize index = 0; index < sizeof(builtinModules) / sizeof(builtinModules[0]); index++) {
        module = ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state, builtinModules[index], ZR_NULL);
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
    SZrIo io;

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

    ZrCore_Io_Init(state, &io, ZR_NULL, ZR_NULL, ZR_NULL);
    if (!ZrLibrary_File_SourceLoadImplementation(state, binaryPath, ZR_NULL, &io)) {
        return ZR_FALSE;
    }

    *outSource = ZrCore_Io_ReadSourceNew(&io);
    if (io.close != ZR_NULL) {
        io.close(state, io.customData);
    }

    return *outSource != ZR_NULL;
}

TZrBool ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleFunction(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrFunction **outFunction) {
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString sourceBuffer;
    TZrSize sourceLength;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }

    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || outFunction == ZR_NULL ||
        !module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                              module_metadata_string_text(moduleName),
                                                              ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
                                                              binaryPath,
                                                              sizeof(binaryPath))) {
        return ZR_FALSE;
    }

    sourceBuffer = ZrLibrary_File_ReadAll(state->global, binaryPath);
    if (sourceBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceLength = strlen(sourceBuffer);
    if (state->global == ZR_NULL || state->global->compileSource == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        return ZR_FALSE;
    }

    *outFunction = state->global->compileSource(state,
                                                sourceBuffer,
                                                sourceLength,
                                                ZrCore_String_Create(state,
                                                                     (TZrNativeString)module_metadata_string_text(moduleName),
                                                                     strlen(module_metadata_string_text(moduleName))));
    ZrCore_Memory_RawFreeWithType(state->global,
                                  sourceBuffer,
                                  sourceLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return *outFunction != ZR_NULL;
}

static const TZrChar *intermediate_metadata_trim_leading(const TZrChar *start, const TZrChar *end) {
    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }
    return start;
}

static const TZrChar *intermediate_metadata_trim_trailing(const TZrChar *start, const TZrChar *end) {
    while (end > start && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    return end;
}

static TZrBool intermediate_metadata_append_symbol(SZrState *state,
                                                   SZrLspIntermediateModuleMetadata *metadata,
                                                   const TZrChar *nameStart,
                                                   const TZrChar *nameEnd,
                                                   const TZrChar *typeStart,
                                                   const TZrChar *typeEnd,
                                                   TZrBool isCallable) {
    SZrLspIntermediateExportSymbol symbol;

    if (state == ZR_NULL || metadata == ZR_NULL || nameStart == ZR_NULL || nameEnd == ZR_NULL ||
        typeStart == ZR_NULL || typeEnd == ZR_NULL || nameEnd <= nameStart || typeEnd <= typeStart) {
        return ZR_FALSE;
    }

    symbol.name = ZrCore_String_Create(state, (TZrNativeString)nameStart, (TZrSize)(nameEnd - nameStart));
    symbol.typeName = ZrCore_String_Create(state, (TZrNativeString)typeStart, (TZrSize)(typeEnd - typeStart));
    symbol.isCallable = isCallable;
    if (symbol.name == ZR_NULL || symbol.typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &metadata->exportedSymbols, &symbol);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspModuleMetadata_LoadIntermediateModuleMetadata(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrLspIntermediateModuleMetadata *outMetadata) {
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString sourceBuffer;
    const TZrChar *exportsSection;
    const TZrChar *cursor;

    if (outMetadata != ZR_NULL) {
        ZrCore_Array_Construct(&outMetadata->exportedSymbols);
    }

    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || outMetadata == ZR_NULL ||
        !module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                              module_metadata_string_text(moduleName),
                                                              ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
                                                              binaryPath,
                                                              sizeof(binaryPath))) {
        return ZR_FALSE;
    }

    sourceBuffer = ZrLibrary_File_ReadAll(state->global, binaryPath);
    if (sourceBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state,
                      &outMetadata->exportedSymbols,
                      sizeof(SZrLspIntermediateExportSymbol),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    exportsSection = strstr(sourceBuffer, "EXPORTED_SYMBOLS (");
    if (exportsSection == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      strlen(sourceBuffer) + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        return ZR_FALSE;
    }

    cursor = strchr(exportsSection, '\n');
    cursor = cursor != ZR_NULL ? cursor + 1 : exportsSection;
    while (cursor != ZR_NULL && *cursor != '\0') {
        const TZrChar *lineStart = cursor;
        const TZrChar *lineEnd = strchr(cursor, '\n');
        const TZrChar *trimmedStart;
        const TZrChar *trimmedEnd;
        TZrInt32 currentLine = 1;

        if (lineEnd == ZR_NULL) {
            lineEnd = cursor + strlen(cursor);
        }
        for (const TZrChar *lineCursor = sourceBuffer; lineCursor < lineStart; lineCursor++) {
            if (*lineCursor == '\n') {
                currentLine++;
            }
        }
        trimmedStart = intermediate_metadata_trim_leading(lineStart, lineEnd);
        trimmedEnd = intermediate_metadata_trim_trailing(trimmedStart, lineEnd);

        if (trimmedStart >= trimmedEnd) {
            cursor = *lineEnd == '\0' ? ZR_NULL : lineEnd + 1;
            continue;
        }

        if (strncmp(trimmedStart, "COMPILE_TIME_VARIABLES", strlen("COMPILE_TIME_VARIABLES")) == 0 ||
            strncmp(trimmedStart, "TYPE_TABLE", strlen("TYPE_TABLE")) == 0) {
            break;
        }

        if (strncmp(trimmedStart, "fn ", 3) == 0) {
            const TZrChar *nameStart = trimmedStart + 3;
            const TZrChar *openParen = strchr(nameStart, '(');
            const TZrChar *typeStart = openParen != ZR_NULL ? strstr(openParen, "): ") : ZR_NULL;
            if (openParen != ZR_NULL && typeStart != ZR_NULL) {
                TZrInt32 startColumn = (TZrInt32)(nameStart - lineStart) + 1;
                TZrInt32 endColumn = (TZrInt32)(openParen - lineStart) + 1;
                intermediate_metadata_append_symbol(state,
                                                    outMetadata,
                                                    nameStart,
                                                    openParen,
                                                    typeStart + 3,
                                                    trimmedEnd,
                                                    ZR_TRUE);
                if (outMetadata->exportedSymbols.length > 0) {
                    SZrLspIntermediateExportSymbol *symbol =
                        (SZrLspIntermediateExportSymbol *)ZrCore_Array_Get(&outMetadata->exportedSymbols,
                                                                            outMetadata->exportedSymbols.length - 1);
                    if (symbol != ZR_NULL) {
                        symbol->declarationLine = currentLine;
                        symbol->declarationStartColumn = startColumn;
                        symbol->declarationEndColumn = endColumn;
                    }
                }
            }
        } else if (strncmp(trimmedStart, "var ", 4) == 0) {
            const TZrChar *nameStart = trimmedStart + 4;
            const TZrChar *typeStart = strstr(nameStart, ": ");
            if (typeStart != ZR_NULL) {
                TZrInt32 startColumn = (TZrInt32)(nameStart - lineStart) + 1;
                TZrInt32 endColumn = (TZrInt32)(typeStart - lineStart) + 1;
                intermediate_metadata_append_symbol(state,
                                                    outMetadata,
                                                    nameStart,
                                                    typeStart,
                                                    typeStart + 2,
                                                    trimmedEnd,
                                                    ZR_FALSE);
                if (outMetadata->exportedSymbols.length > 0) {
                    SZrLspIntermediateExportSymbol *symbol =
                        (SZrLspIntermediateExportSymbol *)ZrCore_Array_Get(&outMetadata->exportedSymbols,
                                                                            outMetadata->exportedSymbols.length - 1);
                    if (symbol != ZR_NULL) {
                        symbol->declarationLine = currentLine;
                        symbol->declarationStartColumn = startColumn;
                        symbol->declarationEndColumn = endColumn;
                    }
                }
            }
        }

        cursor = *lineEnd == '\0' ? ZR_NULL : lineEnd + 1;
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  sourceBuffer,
                                  strlen(sourceBuffer) + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return outMetadata->exportedSymbols.length > 0;
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
        (!module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                               module_metadata_string_text(moduleName),
                                                               ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
                                                               binaryPath,
                                                               sizeof(binaryPath)) &&
         !module_metadata_resolve_existing_project_binary_path(projectIndex,
                                                               module_metadata_string_text(moduleName),
                                                               ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                                               binaryPath,
                                                               sizeof(binaryPath)))) {
        return ZR_FALSE;
    }

    *outUri = module_metadata_create_file_uri_from_native_path(state, binaryPath);
    return *outUri != ZR_NULL;
}

TZrBool ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri) {
    const TZrChar *moduleText;
    ZrLibRegisteredModuleInfo moduleInfo;
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
        sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN) {
        return ZR_FALSE;
    }

    memset(&moduleInfo, 0, sizeof(moduleInfo));
    if (!ZrLibrary_NativeRegistry_GetModuleInfo(state->global, moduleText, &moduleInfo) ||
        moduleInfo.sourcePath == ZR_NULL || moduleInfo.sourcePath[0] == '\0') {
        return ZR_FALSE;
    }

    *outUri = module_metadata_create_file_uri_from_native_path(state, moduleInfo.sourcePath);
    return *outUri != ZR_NULL;
}

const SZrLspIntermediateExportSymbol *ZrLanguageServer_LspModuleMetadata_FindIntermediateExportSymbol(
    const SZrLspIntermediateModuleMetadata *metadata,
    SZrString *symbolName) {
    if (metadata == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < metadata->exportedSymbols.length; index++) {
        const SZrLspIntermediateExportSymbol *symbol =
            (const SZrLspIntermediateExportSymbol *)ZrCore_Array_Get((SZrArray *)&metadata->exportedSymbols, index);
        if (symbol != ZR_NULL && symbol->name != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual(symbol->name, symbolName)) {
            return symbol;
        }
    }

    return ZR_NULL;
}

void ZrLanguageServer_LspModuleMetadata_FreeIntermediateModuleMetadata(SZrState *state,
                                                                       SZrLspIntermediateModuleMetadata *metadata) {
    if (state == ZR_NULL || metadata == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &metadata->exportedSymbols);
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
