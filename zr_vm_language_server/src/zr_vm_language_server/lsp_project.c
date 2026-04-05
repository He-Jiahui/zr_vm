#include "lsp_interface_internal.h"
#include "lsp_project_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef ZR_VM_PLATFORM_IS_WIN
#include <windows.h>
#else
#include <dirent.h>
#endif

#define ZR_LSP_PROJECT_HEX_DIGIT_INVALID ((TZrInt32)-1)

typedef struct SZrLspProjectSourceLoaderContext {
    SZrLspProjectIndex *projectIndex;
    FZrIoLoadSource fallbackSourceLoader;
    TZrPtr fallbackUserData;
} SZrLspProjectSourceLoaderContext;

static void get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static const TZrChar *get_string_text(SZrString *value) {
    TZrNativeString text;
    TZrSize length;

    get_string_view(value, &text, &length);
    return text;
}

static SZrString *create_string_from_const_text(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static void project_index_free(SZrState *state, SZrLspProjectIndex *projectIndex);
static TZrBool discover_project_path_for_uri(SZrString *uri,
                                             TZrChar *projectPath,
                                             TZrSize projectPathSize,
                                             TZrBool *outAmbiguous);
static TZrBool project_resolve_source_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);
static TZrBool project_resolve_binary_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);

static TZrBool project_load_resolved_file_to_io(SZrState *state,
                                                const TZrChar *path,
                                                TZrBool isBinary,
                                                SZrIo *io) {
    SZrLibrary_File_Reader *reader;

    if (state == ZR_NULL || path == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = ZrLibrary_File_OpenRead(state->global, (TZrNativeString)path, isBinary);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io->isBinary = isBinary;
    return ZR_TRUE;
}

static TZrBool project_source_loader_invoke_fallback(SZrState *state,
                                                     SZrLspProjectSourceLoaderContext *context,
                                                     TZrNativeString sourcePath,
                                                     TZrNativeString md5,
                                                     SZrIo *io) {
    TZrPtr previousUserData;
    TZrBool success;

    if (state == ZR_NULL || state->global == ZR_NULL || context == ZR_NULL || context->fallbackSourceLoader == ZR_NULL) {
        return ZR_FALSE;
    }

    previousUserData = state->global->userData;
    state->global->userData = context->fallbackUserData;
    success = context->fallbackSourceLoader(state, sourcePath, md5, io);
    state->global->userData = previousUserData;
    return success;
}

static TZrBool project_source_loader(SZrState *state,
                                     TZrNativeString sourcePath,
                                     TZrNativeString md5,
                                     SZrIo *io) {
    SZrLspProjectSourceLoaderContext *context;
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || state->global == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    context = (SZrLspProjectSourceLoaderContext *)state->global->userData;
    if (context == ZR_NULL || context->projectIndex == ZR_NULL || sourcePath == ZR_NULL) {
        return project_source_loader_invoke_fallback(state, context, sourcePath, md5, io);
    }

    if (project_resolve_source_path(context->projectIndex, sourcePath, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return project_load_resolved_file_to_io(state, resolvedPath, ZR_FALSE, io);
    }

    if (project_resolve_binary_path(context->projectIndex, sourcePath, resolvedPath, sizeof(resolvedPath)) &&
        ZrLibrary_File_Exist(resolvedPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return project_load_resolved_file_to_io(state, resolvedPath, ZR_TRUE, io);
    }

    return project_source_loader_invoke_fallback(state, context, sourcePath, md5, io);
}

static void project_preload_descriptor_plugin_imports(SZrState *state,
                                                      SZrLspProjectIndex *projectIndex,
                                                      SZrAstNode *ast) {
    SZrArray bindings;
    const TZrChar *projectDirectory;

    if (state == ZR_NULL || projectIndex == ZR_NULL || ast == ZR_NULL || state->global == ZR_NULL ||
        projectIndex->projectRootPath == ZR_NULL) {
        return;
    }

    projectDirectory = get_string_text(projectIndex->projectRootPath);
    if (projectDirectory == ZR_NULL || projectDirectory[0] == '\0') {
        return;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, ast, &bindings);
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        const TZrChar *moduleNameText;

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }

        moduleNameText = get_string_text((*bindingPtr)->moduleName);
        if (moduleNameText == ZR_NULL || moduleNameText[0] == '\0') {
            continue;
        }

        ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(state, projectDirectory, moduleNameText);
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
}

static void path_join_const_inputs(const TZrChar *path1, const TZrChar *path2, TZrChar *result) {
    ZrLibrary_File_PathJoin((TZrNativeString)path1, (TZrNativeString)path2, result);
}

static TZrBool string_ends_with(SZrString *value, const TZrChar *suffix) {
    TZrNativeString text;
    TZrSize length;
    TZrSize suffixLength;

    if (suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_view(value, &text, &length);
    suffixLength = strlen(suffix);
    return text != ZR_NULL && length >= suffixLength &&
           memcmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

static TZrBool native_path_has_dynamic_library_extension(const TZrChar *path) {
    TZrSize length;

    if (path == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(path);
    return (length >= 4 && strcmp(path + length - 4, ".dll") == 0) ||
           (length >= 3 && strcmp(path + length - 3, ".so") == 0) ||
           (length >= 6 && strcmp(path + length - 6, ".dylib") == 0);
}

static TZrBool is_hex_digit_char(TZrChar value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static TZrInt32 hex_digit_to_value(TZrChar value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }

    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }

    return ZR_LSP_PROJECT_HEX_DIGIT_INVALID;
}

static TZrBool lsp_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    TZrNativeString uriText;
    TZrSize uriLength;
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    get_string_view(uri, &uriText, &uriLength);
    if (uriText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (uriLength >= 7 && memcmp(uriText, "file://", 7) == 0) {
        readIndex = 7;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (readIndex < uriLength &&
        uriText[readIndex] == '/' &&
        readIndex + 2 < uriLength &&
        isalpha((unsigned char)uriText[readIndex + 1]) &&
        uriText[readIndex + 2] == ':') {
        readIndex++;
    }
#endif

    while (readIndex < uriLength && writeIndex + 1 < bufferSize) {
        TZrChar current = uriText[readIndex];
        if (current == '%' &&
            readIndex + 2 < uriLength &&
            is_hex_digit_char(uriText[readIndex + 1]) &&
            is_hex_digit_char(uriText[readIndex + 2])) {
            buffer[writeIndex++] = (TZrChar)((hex_digit_to_value(uriText[readIndex + 1]) << 4) |
                                             hex_digit_to_value(uriText[readIndex + 2]));
            readIndex += 3;
            continue;
        }

#ifdef ZR_VM_PLATFORM_IS_WIN
        buffer[writeIndex++] = current == '/' ? '\\' : current;
#else
        buffer[writeIndex++] = current;
#endif
        readIndex++;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

static SZrString *native_path_to_file_uri(SZrState *state, const TZrChar *path) {
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

static void normalize_path_for_compare(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; path[index] != '\0' && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = path[index];
        if (current == '\\') {
            current = '/';
        }
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }

    while (writeIndex > 1 && buffer[writeIndex - 1] == '/') {
        writeIndex--;
    }
    buffer[writeIndex] = '\0';
}

static TZrBool native_path_is_within_directory(const TZrChar *path, const TZrChar *directory) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize directoryLength;

    normalize_path_for_compare(path, normalizedPath, sizeof(normalizedPath));
    normalize_path_for_compare(directory, normalizedDirectory, sizeof(normalizedDirectory));
    directoryLength = strlen(normalizedDirectory);

    if (directoryLength == 0 || strncmp(normalizedPath, normalizedDirectory, directoryLength) != 0) {
        return ZR_FALSE;
    }

    return normalizedPath[directoryLength] == '\0' || normalizedPath[directoryLength] == '/';
}

static TZrBool normalize_module_key(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize) {
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

static TZrBool extract_explicit_module_name(SZrAstNode *ast, TZrChar *buffer, TZrSize bufferSize) {
    SZrAstNode *moduleNode;
    SZrAstNode *nameNode;
    TZrNativeString text;
    TZrSize length;

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT || ast->data.script.moduleName == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    moduleNode = ast->data.script.moduleName;
    if (moduleNode->type != ZR_AST_MODULE_DECLARATION || moduleNode->data.moduleDeclaration.name == ZR_NULL) {
        return ZR_FALSE;
    }

    nameNode = moduleNode->data.moduleDeclaration.name;
    if (nameNode->type != ZR_AST_STRING_LITERAL || nameNode->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_view(nameNode->data.stringLiteral.value, &text, &length);
    if (text == ZR_NULL || length == 0 || length >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return normalize_module_key(buffer, buffer, bufferSize);
}

static TZrBool project_script_contains_top_level_ffi_wrapper(SZrAstNode *ast) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL || ast->data.script.statements->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < ast->data.script.statements->count; index++) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index];
        if (statement == ZR_NULL) {
            continue;
        }

        if (statement->type == ZR_AST_EXTERN_BLOCK) {
            return ZR_TRUE;
        }

        if (statement->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            statement->data.compileTimeDeclaration.declaration != ZR_NULL &&
            statement->data.compileTimeDeclaration.declaration->type == ZR_AST_EXTERN_BLOCK) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool project_content_contains_ffi_wrapper_marker(const TZrChar *content) {
    return content != ZR_NULL && strstr(content, "%extern") != ZR_NULL;
}

static TZrBool derive_module_name_from_path(SZrLspProjectIndex *projectIndex,
                                            const TZrChar *path,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;
    const TZrChar *relative;

    if (projectIndex == ZR_NULL || path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    normalize_path_for_compare(path, normalizedPath, sizeof(normalizedPath));
    normalize_path_for_compare(get_string_text(projectIndex->sourceRootPath), normalizedRoot, sizeof(normalizedRoot));
    rootLength = strlen(normalizedRoot);
    if (rootLength == 0 || strncmp(normalizedPath, normalizedRoot, rootLength) != 0) {
        return ZR_FALSE;
    }

    relative = normalizedPath + rootLength;
    while (*relative == '/') {
        relative++;
    }

    return normalize_module_key(relative, buffer, bufferSize);
}

static TZrBool project_binary_root_path(SZrLspProjectIndex *projectIndex,
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

    projectDirectory = get_string_text(projectIndex->project->directory);
    projectBinary = get_string_text(projectIndex->project->binary);
    if (projectDirectory == ZR_NULL || projectBinary == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, projectBinary, buffer);
    return buffer[0] != '\0';
}

static TZrBool derive_binary_module_name_from_path(SZrLspProjectIndex *projectIndex,
                                                   const TZrChar *path,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;
    const TZrChar *relative;

    if (projectIndex == ZR_NULL || path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !project_binary_root_path(projectIndex, binaryRoot, sizeof(binaryRoot))) {
        return ZR_FALSE;
    }

    normalize_path_for_compare(path, normalizedPath, sizeof(normalizedPath));
    normalize_path_for_compare(binaryRoot, normalizedRoot, sizeof(normalizedRoot));
    rootLength = strlen(normalizedRoot);
    if (rootLength == 0 || strncmp(normalizedPath, normalizedRoot, rootLength) != 0) {
        return ZR_FALSE;
    }

    relative = normalizedPath + rootLength;
    while (*relative == '/') {
        relative++;
    }

    return normalize_module_key(relative, buffer, bufferSize);
}

TZrBool ZrLanguageServer_LspProject_DeriveBinaryModuleNameFromPath(SZrLspProjectIndex *projectIndex,
                                                                   const TZrChar *path,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize) {
    return derive_binary_module_name_from_path(projectIndex, path, buffer, bufferSize);
}

static void project_invalidate_module_cache_for_watched_path(SZrState *state,
                                                             SZrLspProjectIndex *projectIndex,
                                                             const TZrChar *affectedPath) {
    SZrString *cacheKey;
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || affectedPath == ZR_NULL) {
        return;
    }

    cacheKey = ZrCore_String_Create(state, (TZrNativeString)affectedPath, strlen(affectedPath));
    if (cacheKey != ZR_NULL) {
        ZrCore_Module_RemoveFromCache(state, cacheKey);
    }

    if (projectIndex == ZR_NULL) {
        return;
    }

    if (derive_module_name_from_path(projectIndex, affectedPath, moduleName, sizeof(moduleName)) ||
        derive_binary_module_name_from_path(projectIndex, affectedPath, moduleName, sizeof(moduleName))) {
        cacheKey = ZrCore_String_Create(state, moduleName, strlen(moduleName));
        if (cacheKey != ZR_NULL) {
            ZrCore_Module_RemoveFromCache(state, cacheKey);
        }
    }
}

static TZrBool project_resolve_source_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;

    if (projectIndex == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !normalize_module_key(moduleName, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; normalizedModule[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }

    if (writeIndex + 4 >= sizeof(relativePath)) {
        return ZR_FALSE;
    }

    memcpy(relativePath + writeIndex, ".zr", 4);
    writeIndex += 3;
    relativePath[writeIndex] = '\0';
    path_join_const_inputs(get_string_text(projectIndex->sourceRootPath), relativePath, buffer);
    return buffer[0] != '\0';
}

static TZrBool project_resolve_binary_path(SZrLspProjectIndex *projectIndex,
                                           const TZrChar *moduleName,
                                           TZrChar *buffer,
                                           TZrSize bufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize writeIndex = 0;
    TZrSize baseLength;

    if (projectIndex == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !normalize_module_key(moduleName, normalizedModule, sizeof(normalizedModule)) ||
        !project_binary_root_path(projectIndex, binaryRoot, sizeof(binaryRoot))) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; normalizedModule[index] != '\0' && writeIndex + 1 < sizeof(relativePath); index++) {
        relativePath[writeIndex++] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }

    baseLength = writeIndex;
    if (baseLength + 5 >= sizeof(relativePath)) {
        return ZR_FALSE;
    }

    memcpy(relativePath + baseLength, ".zro", 5);
    path_join_const_inputs(binaryRoot, relativePath, buffer);
    return ZrLibrary_File_Exist(buffer) == ZR_LIBRARY_FILE_IS_FILE;
}

SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByUri(SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri) {
    for (TZrSize index = 0; projectIndex != ZR_NULL && index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->uri, uri)) {
            return *recordPtr;
        }
    }

    return ZR_NULL;
}

SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByModuleName(SZrLspProjectIndex *projectIndex,
                                                                            SZrString *moduleName) {
    for (TZrSize index = 0; projectIndex != ZR_NULL && index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->moduleName, moduleName)) {
            return *recordPtr;
        }
    }

    return ZR_NULL;
}

SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectByProjectUri(SZrLspContext *context,
                                                                        SZrString *uri,
                                                                        TZrSize *outIndex) {
    for (TZrSize index = 0; context != ZR_NULL && index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*projectPtr)->projectFileUri, uri)) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return *projectPtr;
        }
    }

    return ZR_NULL;
}

SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectForUri(SZrLspContext *context, SZrString *uri) {
    TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrLspProjectIndex *bestProject = ZR_NULL;
    TZrSize bestRootLength = 0;

    if (context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*projectPtr)->projectFileUri, uri)) {
            return *projectPtr;
        }
    }

    if (!lsp_uri_to_native_path(uri, pathBuffer, sizeof(pathBuffer))) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        const TZrChar *sourceRoot;
        TZrSize sourceRootLength;

        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        sourceRoot = get_string_text((*projectPtr)->sourceRootPath);
        if (!native_path_is_within_directory(pathBuffer, sourceRoot)) {
            continue;
        }

        sourceRootLength = sourceRoot != ZR_NULL ? strlen(sourceRoot) : 0;
        if (bestProject == ZR_NULL || sourceRootLength > bestRootLength) {
            bestProject = *projectPtr;
            bestRootLength = sourceRootLength;
        }
    }

    return bestProject;
}

static void project_remove_file_record_at_index(SZrState *state,
                                                SZrLspProjectIndex *projectIndex,
                                                TZrSize recordIndex) {
    SZrLspProjectFileRecord **recordPtr;

    if (state == ZR_NULL || projectIndex == ZR_NULL || recordIndex >= projectIndex->files.length) {
        return;
    }

    recordPtr = (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, recordIndex);
    if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, *recordPtr, sizeof(SZrLspProjectFileRecord));
    }

    if (recordIndex + 1 < projectIndex->files.length) {
        memmove(projectIndex->files.head + recordIndex * projectIndex->files.elementSize,
                projectIndex->files.head + (recordIndex + 1) * projectIndex->files.elementSize,
                (projectIndex->files.length - recordIndex - 1) * projectIndex->files.elementSize);
    }
    projectIndex->files.length--;
}

TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri) {
    TZrSize projectIndexOffset;
    SZrLspProjectIndex **projectPtr;
    TZrChar targetPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTargetPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspProject_FindProjectByProjectUri(context, uri, &projectIndexOffset) == ZR_NULL) {
        if (!lsp_uri_to_native_path(uri, targetPath, sizeof(targetPath))) {
            return ZR_FALSE;
        }

        normalize_path_for_compare(targetPath, normalizedTargetPath, sizeof(normalizedTargetPath));
        for (projectIndexOffset = 0; projectIndexOffset < context->projectIndexes.length; projectIndexOffset++) {
            TZrChar normalizedProjectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
            if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL || (*projectPtr)->projectFilePath == ZR_NULL) {
                continue;
            }

            normalize_path_for_compare(get_string_text((*projectPtr)->projectFilePath),
                                       normalizedProjectPath,
                                       sizeof(normalizedProjectPath));
            if (strcmp(normalizedTargetPath, normalizedProjectPath) == 0) {
                break;
            }
        }

        if (projectIndexOffset >= context->projectIndexes.length) {
            return ZR_FALSE;
        }
    }

    projectPtr = (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
    if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
        for (TZrSize recordIndex = 0; recordIndex < (*projectPtr)->files.length; recordIndex++) {
            SZrLspProjectFileRecord **recordPtr =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(&(*projectPtr)->files, recordIndex);
            if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL && (*recordPtr)->uri != ZR_NULL) {
                ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, (*recordPtr)->uri);
                if (context->parser != ZR_NULL) {
                    ZrLanguageServer_IncrementalParser_RemoveFile(state, context->parser, (*recordPtr)->uri);
                }
            }
        }
        project_index_free(state, *projectPtr);
    }

    if (projectIndexOffset + 1 < context->projectIndexes.length) {
        memmove(context->projectIndexes.head + projectIndexOffset * context->projectIndexes.elementSize,
                context->projectIndexes.head + (projectIndexOffset + 1) * context->projectIndexes.elementSize,
                (context->projectIndexes.length - projectIndexOffset - 1) * context->projectIndexes.elementSize);
    }
    context->projectIndexes.length--;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri) {
    TZrChar targetPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTargetPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!lsp_uri_to_native_path(uri, targetPath, sizeof(targetPath))) {
        targetPath[0] = '\0';
    } else {
        normalize_path_for_compare(targetPath, normalizedTargetPath, sizeof(normalizedTargetPath));
    }

    for (TZrSize projectIndexOffset = 0; projectIndexOffset < context->projectIndexes.length; projectIndexOffset++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndexOffset);
        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        for (TZrSize recordIndex = 0; recordIndex < (*projectPtr)->files.length; recordIndex++) {
            SZrLspProjectFileRecord **recordPtr =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(&(*projectPtr)->files, recordIndex);
            TZrBool pathMatched = ZR_FALSE;

            if (targetPath[0] != '\0' && recordPtr != ZR_NULL && *recordPtr != ZR_NULL && (*recordPtr)->path != ZR_NULL) {
                TZrChar normalizedRecordPath[ZR_LIBRARY_MAX_PATH_LENGTH];
                normalize_path_for_compare(get_string_text((*recordPtr)->path),
                                           normalizedRecordPath,
                                           sizeof(normalizedRecordPath));
                pathMatched = strcmp(normalizedTargetPath, normalizedRecordPath) == 0 ? ZR_TRUE : ZR_FALSE;
            }

            if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL &&
                (ZrLanguageServer_Lsp_StringsEqual((*recordPtr)->uri, uri) || pathMatched)) {
                if ((*recordPtr)->uri != ZR_NULL) {
                    ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, (*recordPtr)->uri);
                    if (context->parser != ZR_NULL) {
                        ZrLanguageServer_IncrementalParser_RemoveFile(state, context->parser, (*recordPtr)->uri);
                    }
                }
                project_remove_file_record_at_index(state, *projectPtr, recordIndex);
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                                            SZrLspContext *context,
                                                                                            SZrString *uri) {
    TZrChar affectedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar discoveredProjectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrLspProjectIndex *bestProject = ZR_NULL;
    TZrSize bestRootLength = 0;
    const TZrChar *projectFilePath;
    SZrString *projectFileUri;
    TZrNativeString projectContent;
    TZrSize contentLength;
    TZrBool ambiguous = ZR_FALSE;
    TZrBool refreshed;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        !lsp_uri_to_native_path(uri, affectedPath, sizeof(affectedPath))) {
        return ZR_FALSE;
    }

    if (state->global != ZR_NULL && native_path_has_dynamic_library_extension(affectedPath)) {
        ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(state->global, affectedPath);
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        const TZrChar *projectRoot;
        TZrSize projectRootLength;

        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL || (*projectPtr)->projectRootPath == ZR_NULL) {
            continue;
        }

        projectRoot = get_string_text((*projectPtr)->projectRootPath);
        if (!native_path_is_within_directory(affectedPath, projectRoot)) {
            continue;
        }

        projectRootLength = projectRoot != ZR_NULL ? strlen(projectRoot) : 0;
        if (bestProject == ZR_NULL || projectRootLength > bestRootLength) {
            bestProject = *projectPtr;
            bestRootLength = projectRootLength;
        }
    }

    project_invalidate_module_cache_for_watched_path(state, bestProject, affectedPath);

    if (bestProject != ZR_NULL && bestProject->projectFilePath != ZR_NULL && bestProject->projectFileUri != ZR_NULL) {
        projectFilePath = get_string_text(bestProject->projectFilePath);
        projectFileUri = bestProject->projectFileUri;
    } else {
        if (!discover_project_path_for_uri(uri,
                                           discoveredProjectPath,
                                           sizeof(discoveredProjectPath),
                                           &ambiguous) ||
            ambiguous) {
            return ZR_FALSE;
        }

        projectFilePath = discoveredProjectPath;
        projectFileUri = native_path_to_file_uri(state, discoveredProjectPath);
        if (projectFileUri == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    projectContent = ZrLibrary_File_ReadAll(state->global, (TZrNativeString)projectFilePath);
    if (projectContent == ZR_NULL) {
        return ZR_FALSE;
    }

    contentLength = strlen(projectContent);
    refreshed = ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(state,
                                                                      context,
                                                                      projectFileUri,
                                                                      projectContent,
                                                                      contentLength);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  projectContent,
                                  contentLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return refreshed;
}

static void project_index_free(SZrState *state, SZrLspProjectIndex *projectIndex) {
    if (state == ZR_NULL || projectIndex == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < projectIndex->files.length; index++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, index);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *recordPtr, sizeof(SZrLspProjectFileRecord));
        }
    }

    ZrCore_Array_Free(state, &projectIndex->files);
    if (projectIndex->project != ZR_NULL) {
        ZrLibrary_Project_Free(state, projectIndex->project);
    }
    ZrCore_Memory_RawFree(state->global, projectIndex, sizeof(SZrLspProjectIndex));
}

static SZrLspProjectIndex *project_index_new_from_document(SZrState *state,
                                                           SZrString *projectUri,
                                                           const TZrChar *content,
                                                           TZrSize contentLength) {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceRootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *jsonBuffer;
    SZrLibrary_Project *project;
    SZrLspProjectIndex *projectIndex;

    if (state == ZR_NULL || projectUri == ZR_NULL || content == ZR_NULL ||
        !lsp_uri_to_native_path(projectUri, projectPath, sizeof(projectPath))) {
        return ZR_NULL;
    }

    jsonBuffer = (TZrChar *)malloc(contentLength + 1);
    if (jsonBuffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(jsonBuffer, content, contentLength);
    jsonBuffer[contentLength] = '\0';
    project = ZrLibrary_Project_New(state, jsonBuffer, projectPath);
    free(jsonBuffer);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }

    path_join_const_inputs(get_string_text(project->directory), get_string_text(project->source), sourceRootPath);
    projectIndex = (SZrLspProjectIndex *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectIndex));
    if (projectIndex == ZR_NULL) {
        ZrLibrary_Project_Free(state, project);
        return ZR_NULL;
    }

    projectIndex->project = project;
    projectIndex->projectFileUri = projectUri;
    projectIndex->projectFilePath = ZrCore_String_Create(state, projectPath, strlen(projectPath));
    projectIndex->projectRootPath = create_string_from_const_text(state, get_string_text(project->directory));
    projectIndex->sourceRootPath = ZrCore_String_Create(state, sourceRootPath, strlen(sourceRootPath));
    ZrCore_Array_Init(state,
                      &projectIndex->files,
                      sizeof(SZrLspProjectFileRecord *),
                      ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    return projectIndex;
}

static SZrLspProjectIndex *project_index_new_from_path(SZrState *state, const TZrChar *projectPath) {
    TZrNativeString jsonBuffer;
    SZrString *projectUri;
    SZrLspProjectIndex *projectIndex;
    TZrSize contentLength;

    if (state == ZR_NULL || projectPath == ZR_NULL) {
        return ZR_NULL;
    }

    jsonBuffer = ZrLibrary_File_ReadAll(state->global, (TZrNativeString)projectPath);
    if (jsonBuffer == ZR_NULL) {
        return ZR_NULL;
    }

    contentLength = strlen(jsonBuffer);
    projectUri = native_path_to_file_uri(state, projectPath);
    projectIndex = projectUri != ZR_NULL
                   ? project_index_new_from_document(state, projectUri, jsonBuffer, contentLength)
                   : ZR_NULL;
    ZrCore_Memory_RawFreeWithType(state->global,
                                  jsonBuffer,
                                  contentLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return projectIndex;
}

static TZrBool path_get_parent_directory_in_place(TZrChar *path) {
    TZrSize length;

    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    length = strlen(path);
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) {
#ifdef ZR_VM_PLATFORM_IS_WIN
        if (length == 3 && isalpha((unsigned char)path[0]) && path[1] == ':') {
            break;
        }
#endif
        path[--length] = '\0';
    }

    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\') {
        length--;
    }

    if (length == 0) {
        return ZR_FALSE;
    }

    if (length == 1) {
        path[1] = '\0';
        return ZR_TRUE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (length == 3 && isalpha((unsigned char)path[0]) && path[1] == ':') {
        path[3] = '\0';
        return ZR_TRUE;
    }
#endif

    path[length - 1] = '\0';
    return ZR_TRUE;
}

static TZrSize directory_count_project_files(const TZrChar *directory,
                                             TZrChar *firstProjectPath,
                                             TZrSize bufferSize) {
    TZrSize count = 0;

    if (directory == ZR_NULL) {
        return 0;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    TZrChar pattern[ZR_LIBRARY_MAX_PATH_LENGTH];
    WIN32_FIND_DATAA findData;
    HANDLE handle;

    ZrLibrary_File_PathJoin(directory, "*.zrp", pattern);
    handle = FindFirstFileA(pattern, &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        count++;
        if (count == 1 && firstProjectPath != ZR_NULL && bufferSize > 0) {
            ZrLibrary_File_PathJoin(directory, findData.cFileName, firstProjectPath);
        }
    } while (FindNextFileA(handle, &findData) != 0);

    FindClose(handle);
#else
    DIR *dir = opendir(directory);
    struct dirent *entry;

    if (dir == ZR_NULL) {
        return 0;
    }

    while ((entry = readdir(dir)) != ZR_NULL) {
        TZrSize nameLength;

        if (entry->d_name[0] == '\0') {
            continue;
        }

        nameLength = strlen(entry->d_name);
        if (nameLength < 4 || strcmp(entry->d_name + nameLength - 4, ".zrp") != 0) {
            continue;
        }

        count++;
        if (count == 1 && firstProjectPath != ZR_NULL && bufferSize > 0) {
            ZrLibrary_File_PathJoin((TZrNativeString)directory, entry->d_name, firstProjectPath);
        }
    }

    closedir(dir);
#endif

    return count;
}

static TZrBool discover_project_path_for_uri(SZrString *uri,
                                             TZrChar *projectPath,
                                             TZrSize projectPathSize,
                                             TZrBool *outAmbiguous) {
    TZrChar currentPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar currentDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize projectCount;

    if (projectPath != ZR_NULL && projectPathSize > 0) {
        projectPath[0] = '\0';
    }
    if (outAmbiguous != ZR_NULL) {
        *outAmbiguous = ZR_FALSE;
    }

    if (!lsp_uri_to_native_path(uri, currentPath, sizeof(currentPath)) ||
        !ZrLibrary_File_GetDirectory(currentPath, currentDirectory)) {
        return ZR_FALSE;
    }

    while (currentDirectory[0] != '\0') {
        projectCount = directory_count_project_files(currentDirectory, projectPath, projectPathSize);
        if (projectCount == 1) {
            return ZR_TRUE;
        }
        if (projectCount > 1) {
            if (outAmbiguous != ZR_NULL) {
                *outAmbiguous = ZR_TRUE;
            }
            return ZR_FALSE;
        }
        if (!path_get_parent_directory_in_place(currentDirectory)) {
            break;
        }
    }

    return ZR_FALSE;
}

static void project_invalidate_loaded_analyzers(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex) {
    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL) {
        return;
    }

    for (TZrSize recordIndex = 0; recordIndex < projectIndex->files.length; recordIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, recordIndex);
        if (recordPtr != ZR_NULL && *recordPtr != ZR_NULL && (*recordPtr)->uri != ZR_NULL) {
            ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, (*recordPtr)->uri);
        }
    }
}

static TZrBool project_reanalyze_loaded_document(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrLspProjectIndex *projectIndex,
                                                 SZrString *uri) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;
    FZrIoLoadSource previousSourceLoader = ZR_NULL;
    TZrPtr previousUserData = ZR_NULL;
    SZrLspProjectSourceLoaderContext sourceLoaderContext;
    TZrBool analyzeSuccess;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion == ZR_NULL || fileVersion->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_ClearCache(state, analyzer);
    project_preload_descriptor_plugin_imports(state, projectIndex, fileVersion->ast);
    if (state->global != ZR_NULL) {
        sourceLoaderContext.projectIndex = projectIndex;
        sourceLoaderContext.fallbackSourceLoader = state->global->sourceLoader;
        previousUserData = state->global->userData;
        sourceLoaderContext.fallbackUserData = previousUserData;
        previousSourceLoader = state->global->sourceLoader;
        state->global->userData = &sourceLoaderContext;
        state->global->sourceLoader = project_source_loader;
    }
    analyzeSuccess = ZrLanguageServer_SemanticAnalyzer_Analyze(state, analyzer, fileVersion->ast);
    if (state->global != ZR_NULL) {
        state->global->userData = previousUserData;
        state->global->sourceLoader = previousSourceLoader;
    }

    return analyzeSuccess;
}

static TZrBool project_collect_loaded_source_uris(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrLspProjectIndex *projectIndex,
                                                  SZrArray *uris) {
    if (state == ZR_NULL || context == ZR_NULL || context->parser == ZR_NULL || projectIndex == ZR_NULL || uris == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!uris->isValid) {
        ZrCore_Array_Init(state, uris, sizeof(SZrString *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize bucketIndex = 0; bucketIndex < context->parser->uriToFileMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = context->parser->uriToFileMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                SZrFileVersion *fileVersion = (SZrFileVersion *)pair->value.value.nativeObject.nativePointer;
                TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

                if (fileVersion != ZR_NULL &&
                    fileVersion->uri != ZR_NULL &&
                    string_ends_with(fileVersion->uri, ".zr") &&
                    lsp_uri_to_native_path(fileVersion->uri, pathBuffer, sizeof(pathBuffer)) &&
                    native_path_is_within_directory(pathBuffer, get_string_text(projectIndex->sourceRootPath))) {
                    SZrString *uri = fileVersion->uri;
                    ZrCore_Array_Push(state, uris, &uri);
                }
            }

            pair = pair->next;
        }
    }

    return ZR_TRUE;
}

static TZrBool project_register_loaded_document(SZrState *state,
                                                SZrLspContext *context,
                                                SZrLspProjectIndex *projectIndex,
                                                SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    TZrChar pathBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar moduleBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *pathString;
    SZrString *moduleString;
    SZrLspProjectFileRecord *record;
    SZrFileVersion *fileVersion;
    TZrBool isFfiWrapperSource;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL ||
        !lsp_uri_to_native_path(uri, pathBuffer, sizeof(pathBuffer))) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!extract_explicit_module_name(analyzer->ast, moduleBuffer, sizeof(moduleBuffer)) &&
        !derive_module_name_from_path(projectIndex, pathBuffer, moduleBuffer, sizeof(moduleBuffer))) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    isFfiWrapperSource = project_script_contains_top_level_ffi_wrapper(analyzer->ast) ||
                         (fileVersion != ZR_NULL &&
                          project_content_contains_ffi_wrapper_marker(fileVersion->content));

    pathString = ZrCore_String_Create(state, pathBuffer, strlen(pathBuffer));
    moduleString = ZrCore_String_Create(state, moduleBuffer, strlen(moduleBuffer));
    if (pathString == ZR_NULL || moduleString == ZR_NULL) {
        return ZR_FALSE;
    }

    record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri);
    if (record == ZR_NULL) {
        record = (SZrLspProjectFileRecord *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspProjectFileRecord));
        if (record == ZR_NULL) {
            return ZR_FALSE;
        }

        record->uri = uri;
        record->path = pathString;
        record->moduleName = moduleString;
        record->isFfiWrapperSource = isFfiWrapperSource;
        ZrCore_Array_Push(state, &projectIndex->files, &record);
        return ZR_TRUE;
    }

    record->path = pathString;
    record->moduleName = moduleString;
    record->isFfiWrapperSource = isFfiWrapperSource;
    return ZR_TRUE;
}

static TZrBool project_load_imports_from_uri(SZrState *state,
                                             SZrLspContext *context,
                                             SZrLspProjectIndex *projectIndex,
                                             SZrString *uri);

static TZrBool project_ensure_module_loaded(SZrState *state,
                                            SZrLspContext *context,
                                            SZrLspProjectIndex *projectIndex,
                                            SZrString *moduleName) {
    TZrChar resolvedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrString *moduleUri;
    SZrFileVersion *fileVersion;
    TZrNativeString sourceCode;
    TZrSize sourceLength;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, moduleName) != ZR_NULL) {
        return ZR_TRUE;
    }

    if (!project_resolve_source_path(projectIndex, get_string_text(moduleName), resolvedPath, sizeof(resolvedPath)) ||
        ZrLibrary_File_Exist(resolvedPath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_FALSE;
    }

    moduleUri = native_path_to_file_uri(state, resolvedPath);
    if (moduleUri == ZR_NULL) {
        return ZR_FALSE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, moduleUri);
    if (fileVersion == ZR_NULL) {
        sourceCode = ZrLibrary_File_ReadAll(state->global, resolvedPath);
        if (sourceCode == ZR_NULL) {
            return ZR_FALSE;
        }

        sourceLength = strlen(sourceCode);
        if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state, context, moduleUri, sourceCode, sourceLength, 0, ZR_FALSE)) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceCode,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
            return ZR_FALSE;
        }

        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceCode,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    if (!project_reanalyze_loaded_document(state, context, projectIndex, moduleUri)) {
        return ZR_FALSE;
    }

    return project_register_loaded_document(state, context, projectIndex, moduleUri) &&
           project_load_imports_from_uri(state, context, projectIndex, moduleUri);
}

static TZrBool project_load_imports_from_uri(SZrState *state,
                                             SZrLspContext *context,
                                             SZrLspProjectIndex *projectIndex,
                                             SZrString *uri) {
    SZrSemanticAnalyzer *analyzer;
    SZrArray bindings;

    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL) {
            project_ensure_module_loaded(state, context, projectIndex, (*bindingPtr)->moduleName);
        }
    }

    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return ZR_TRUE;
}

SZrLspProjectIndex *ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(SZrState *state,
                                                                    SZrLspContext *context,
                                                                    SZrString *uri) {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool ambiguous = ZR_FALSE;
    SZrLspProjectIndex *projectIndex;
    TZrSize existingIndex;
    SZrString *projectUri;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_NULL;
    }

    projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(context, uri);
    if (projectIndex != ZR_NULL) {
        return projectIndex;
    }

    if (!discover_project_path_for_uri(uri, projectPath, sizeof(projectPath), &ambiguous) || ambiguous) {
        return ZR_NULL;
    }

    projectUri = native_path_to_file_uri(state, projectPath);
    if (projectUri != ZR_NULL) {
        projectIndex = ZrLanguageServer_LspProject_FindProjectByProjectUri(context,
                                                                           projectUri,
                                                                           &existingIndex);
        if (projectIndex != ZR_NULL) {
            return projectIndex;
        }
    }

    projectIndex = project_index_new_from_path(state, projectPath);
    if (projectIndex == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
    project_ensure_module_loaded(state, context, projectIndex, projectIndex->project->entry);
    return projectIndex;
}

void ZrLanguageServer_Lsp_ProjectIndexes_Free(SZrState *state, SZrLspContext *context) {
    if (state == ZR_NULL || context == ZR_NULL || !context->projectIndexes.isValid) {
        return;
    }

    for (TZrSize index = 0; index < context->projectIndexes.length; index++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, index);
        if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
            project_index_free(state, *projectPtr);
        }
    }

    ZrCore_Array_Free(state, &context->projectIndexes);
}

TZrBool ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength) {
    SZrLspProjectIndex *projectIndex;
    TZrSize existingIndex;
    SZrArray loadedUris;

    ZrCore_Array_Construct(&loadedUris);

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (string_ends_with(uri, ".zrp")) {
        projectIndex = project_index_new_from_document(state, uri, content, contentLength);
        if (projectIndex == ZR_NULL) {
            return ZR_FALSE;
        }

        if (ZrLanguageServer_LspProject_FindProjectByProjectUri(context, uri, &existingIndex) != ZR_NULL) {
            SZrLspProjectIndex **projectPtr =
                (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, existingIndex);
            if (projectPtr != ZR_NULL && *projectPtr != ZR_NULL) {
                project_invalidate_loaded_analyzers(state, context, *projectPtr);
                project_index_free(state, *projectPtr);
            }

            memmove(context->projectIndexes.head + existingIndex * context->projectIndexes.elementSize,
                    context->projectIndexes.head + (existingIndex + 1) * context->projectIndexes.elementSize,
                    (context->projectIndexes.length - existingIndex - 1) * context->projectIndexes.elementSize);
            context->projectIndexes.length--;
        }

        ZrCore_Array_Push(state, &context->projectIndexes, &projectIndex);
        if (!project_ensure_module_loaded(state, context, projectIndex, projectIndex->project->entry) ||
            !project_collect_loaded_source_uris(state, context, projectIndex, &loadedUris)) {
            ZrCore_Array_Free(state, &loadedUris);
            return ZR_FALSE;
        }

        for (TZrSize uriIndex = 0; uriIndex < loadedUris.length; uriIndex++) {
            SZrString **loadedUriPtr = (SZrString **)ZrCore_Array_Get(&loadedUris, uriIndex);
            if (loadedUriPtr == ZR_NULL || *loadedUriPtr == ZR_NULL) {
                continue;
            }

            if (!project_reanalyze_loaded_document(state, context, projectIndex, *loadedUriPtr) ||
                !project_register_loaded_document(state, context, projectIndex, *loadedUriPtr) ||
                !project_load_imports_from_uri(state, context, projectIndex, *loadedUriPtr)) {
                ZrCore_Array_Free(state, &loadedUris);
                return ZR_FALSE;
            }
        }

        ZrCore_Array_Free(state, &loadedUris);
        return ZR_TRUE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    if (projectIndex == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!project_reanalyze_loaded_document(state, context, projectIndex, uri) ||
        !project_register_loaded_document(state, context, projectIndex, uri) ||
        !project_load_imports_from_uri(state, context, projectIndex, uri) ||
        !project_collect_loaded_source_uris(state, context, projectIndex, &loadedUris)) {
        ZrCore_Array_Free(state, &loadedUris);
        return ZR_FALSE;
    }

    for (TZrSize uriIndex = 0; uriIndex < loadedUris.length; uriIndex++) {
        SZrString **loadedUriPtr = (SZrString **)ZrCore_Array_Get(&loadedUris, uriIndex);

        if (loadedUriPtr == ZR_NULL || *loadedUriPtr == ZR_NULL ||
            ZrLanguageServer_Lsp_StringsEqual(*loadedUriPtr, uri)) {
            continue;
        }

        if (!project_reanalyze_loaded_document(state, context, projectIndex, *loadedUriPtr) ||
            !project_register_loaded_document(state, context, projectIndex, *loadedUriPtr) ||
            !project_load_imports_from_uri(state, context, projectIndex, *loadedUriPtr)) {
            ZrCore_Array_Free(state, &loadedUris);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(state, &loadedUris);
    return ZR_TRUE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_ProjectContainsUri(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri) {
    ZR_UNUSED_PARAMETER(state);
    return ZrLanguageServer_LspProject_FindProjectForUri(context, uri) != ZR_NULL;
}

TZrBool ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *query,
                                                           SZrArray *result) {
    TZrBool appendedAny = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize projectIndex = 0; projectIndex < context->projectIndexes.length; projectIndex++) {
        SZrLspProjectIndex **projectPtr =
            (SZrLspProjectIndex **)ZrCore_Array_Get(&context->projectIndexes, projectIndex);
        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        for (TZrSize fileIndex = 0; fileIndex < (*projectPtr)->files.length; fileIndex++) {
            SZrLspProjectFileRecord **recordPtr =
                (SZrLspProjectFileRecord **)ZrCore_Array_Get(&(*projectPtr)->files, fileIndex);
            SZrSemanticAnalyzer *analyzer;

            if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL) {
                continue;
            }

            analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*recordPtr)->uri);
            if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
                analyzer->symbolTable->globalScope == ZR_NULL) {
                continue;
            }

            for (TZrSize symbolIndex = 0; symbolIndex < analyzer->symbolTable->globalScope->symbols.length; symbolIndex++) {
                SZrSymbol **symbolPtr =
                    (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, symbolIndex);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                    ZrLanguageServer_Lsp_StringContainsCaseInsensitive((*symbolPtr)->name, query)) {
                    SZrLspSymbolInformation *info =
                        ZrLanguageServer_Lsp_CreateSymbolInformation(state, *symbolPtr);
                    if (info != ZR_NULL) {
                        ZrCore_Array_Push(state, result, &info);
                        appendedAny = ZR_TRUE;
                    }
                }
            }
        }
    }

    return appendedAny;
}
