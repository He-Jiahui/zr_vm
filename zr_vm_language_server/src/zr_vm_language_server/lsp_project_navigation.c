#include "lsp_module_metadata.h"
#include "lsp_project_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"

#ifdef ZR_VM_PLATFORM_IS_WIN
#include <windows.h>
#else
#include <dirent.h>
#endif

static TZrBool project_navigation_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLanguageServer_Lsp_FileUriToNativePath(uri, buffer, bufferSize);
}

static const TZrChar *project_navigation_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static SZrString *project_navigation_native_path_to_file_uri(SZrState *state, const TZrChar *path) {
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

static TZrBool append_lsp_location(SZrState *state, SZrArray *result, SZrString *uri, SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static SZrFileRange project_navigation_metadata_file_entry_range(SZrString *uri) {
    SZrFilePosition start = ZrParser_FilePosition_Create(0, 1, 1);
    return ZrParser_FileRange_Create(start, start, uri);
}

static TZrBool project_navigation_position_is_module_entry(SZrLspPosition position) {
    return position.line == 0 && position.character == 0;
}

static TZrBool project_navigation_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (range.start.offset > 0 && range.end.offset > 0 && position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

static SZrSymbol *find_global_symbol_by_name(SZrSemanticAnalyzer *analyzer, SZrString *name, SZrString *uri) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || analyzer->symbolTable->globalScope == ZR_NULL ||
        name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->symbolTable->globalScope->symbols.length; index++) {
        SZrSymbol **symbolPtr =
            (SZrSymbol **)ZrCore_Array_Get(&analyzer->symbolTable->globalScope->symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->name, name) &&
            (uri == ZR_NULL || ZrLanguageServer_Lsp_StringsEqual((*symbolPtr)->location.source, uri))) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool symbol_is_project_global(SZrSemanticAnalyzer *analyzer, SZrSymbol *symbol) {
    return analyzer != ZR_NULL && analyzer->symbolTable != ZR_NULL &&
           analyzer->symbolTable->globalScope != ZR_NULL && symbol != ZR_NULL &&
           symbol->scope == analyzer->symbolTable->globalScope;
}

typedef struct SZrLspProjectResolvedExternalImportedMember {
    SZrLspProjectIndex *projectIndex;
    SZrString *moduleName;
    SZrString *memberName;
    EZrLspImportedModuleSourceKind sourceKind;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
} SZrLspProjectResolvedExternalImportedMember;

typedef SZrLspExternalMetadataDeclaration SZrLspProjectResolvedExternalMetadataDeclaration;

static TZrBool append_symbol_references_from_tracker(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrSymbol *symbol,
                                                     TZrBool includeDeclaration,
                                                     SZrArray *result) {
    SZrArray references;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->referenceTracker == ZR_NULL || symbol == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &references, sizeof(SZrReference *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_ReferenceTracker_FindReferences(state, analyzer->referenceTracker, symbol, &references)) {
        ZrCore_Array_Free(state, &references);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < references.length; index++) {
        SZrReference **referencePtr = (SZrReference **)ZrCore_Array_Get(&references, index);
        if (referencePtr == ZR_NULL || *referencePtr == ZR_NULL ||
            ((*referencePtr)->type == ZR_REFERENCE_DEFINITION && !includeDeclaration)) {
            continue;
        }

        if (!append_lsp_location(state, result, (*referencePtr)->location.source, (*referencePtr)->location)) {
            ZrCore_Array_Free(state, &references);
            return ZR_FALSE;
        }
    }

    ZrCore_Array_Free(state, &references);
    return ZR_TRUE;
}

static TZrBool append_imported_member_locations_from_analyzer(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrString *memberName,
                                                              SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(state,
                                                                                 analyzer->ast,
                                                                                 &bindings,
                                                                                 moduleName,
                                                                                 memberName,
                                                                                 result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool append_imported_module_locations_from_analyzer(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrArray *result);
static TZrBool append_import_binding_locations_from_analyzer(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrString *moduleName,
                                                             SZrArray *result);
static TZrBool project_navigation_try_find_line_bounds(const TZrChar *content,
                                                       TZrSize contentLength,
                                                       TZrInt32 fileLine,
                                                       TZrSize *outLineStart,
                                                       TZrSize *outLineEnd) {
    TZrInt32 currentLine = 1;
    TZrSize lineStart = 0;

    if (outLineStart != ZR_NULL) {
        *outLineStart = 0;
    }
    if (outLineEnd != ZR_NULL) {
        *outLineEnd = 0;
    }
    if (content == ZR_NULL || fileLine <= 0 || outLineStart == ZR_NULL || outLineEnd == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < contentLength; index++) {
        if (currentLine == fileLine) {
            TZrSize lineEnd = index;
            while (lineEnd < contentLength && content[lineEnd] != '\n') {
                lineEnd++;
            }

            *outLineStart = lineStart;
            *outLineEnd = lineEnd;
            return ZR_TRUE;
        }

        if (content[index] == '\n') {
            currentLine++;
            lineStart = index + 1;
        }
    }

    if (currentLine == fileLine && lineStart <= contentLength) {
        *outLineStart = lineStart;
        *outLineEnd = contentLength;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrFileRange project_navigation_refine_import_module_path_location(const TZrChar *content,
                                                                          TZrSize contentLength,
                                                                          SZrFileRange range,
                                                                          SZrString *moduleName) {
    const TZrChar *moduleText;
    TZrSize moduleLength;
    TZrInt32 targetLine;
    TZrSize lineStart = 0;
    TZrSize lineEnd = 0;

    moduleText = project_navigation_string_text(moduleName);
    moduleLength = moduleText != ZR_NULL ? strlen(moduleText) : 0;
    targetLine = range.start.line > 0 ? range.start.line : range.end.line;
    if (content == ZR_NULL || moduleText == ZR_NULL || moduleLength == 0 || targetLine <= 0 ||
        !project_navigation_try_find_line_bounds(content, contentLength, targetLine, &lineStart, &lineEnd) ||
        lineEnd <= lineStart || lineEnd - lineStart < moduleLength) {
        return range;
    }

    for (TZrSize index = lineStart; index + moduleLength <= lineEnd; index++) {
        if (memcmp(content + index, moduleText, moduleLength) != 0) {
            continue;
        }

        if ((index == lineStart || content[index - 1] != '"') ||
            (index + moduleLength >= lineEnd || content[index + moduleLength] != '"')) {
            continue;
        }

        range.start.offset = index;
        range.end.offset = index + moduleLength;
        range.start.line = targetLine;
        range.end.line = targetLine;
        range.start.column = (TZrInt32)(index - lineStart) + 1;
        range.end.column = range.start.column + (TZrInt32)moduleLength;
        return range;
    }

    return range;
}

static void project_navigation_refine_import_binding_target_locations(const TZrChar *content,
                                                                      TZrSize contentLength,
                                                                      SZrArray *bindings) {
    if (content == ZR_NULL || bindings == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL || (*bindingPtr)->moduleName == ZR_NULL) {
            continue;
        }

        (*bindingPtr)->modulePathLocation =
            project_navigation_refine_import_module_path_location(content,
                                                                  contentLength,
                                                                  (*bindingPtr)->modulePathLocation,
                                                                  (*bindingPtr)->moduleName);
    }
}

static TZrBool append_import_target_locations_from_analyzer(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrString *moduleName,
                                                            SZrArray *result);

typedef TZrBool (*TZrLspProjectSourceReferenceAppender)(SZrState *state,
                                                        SZrLspContext *context,
                                                        SZrString *uri,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrString *moduleName,
                                                        SZrString *memberName,
                                                        SZrArray *result);

static TZrBool project_navigation_try_get_analyzer_for_uri(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           SZrSemanticAnalyzer **outAnalyzer) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    TZrNativeString sourceBuffer = ZR_NULL;
    TZrSize sourceLength = 0;
    TZrBool loadedFromDisk = ZR_FALSE;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        sourceBuffer = fileVersion->content;
        sourceLength = fileVersion->contentLength;
    } else if (state->global != ZR_NULL && project_navigation_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        sourceBuffer = ZrLibrary_File_ReadAll(state->global, nativePath);
        sourceLength = sourceBuffer != ZR_NULL ? strlen(sourceBuffer) : 0;
        loadedFromDisk = sourceBuffer != ZR_NULL;
    }

    if (sourceBuffer == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state,
                                                 context,
                                                 uri,
                                                 sourceBuffer,
                                                 sourceLength,
                                                 fileVersion != ZR_NULL ? fileVersion->version : 0,
                                                 ZR_FALSE)) {
        if (loadedFromDisk) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceBuffer,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        }
        return ZR_FALSE;
    }

    if (loadedFromDisk) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
    }

    return ZR_TRUE;
}

static TZrBool project_navigation_append_imported_member_for_uri(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrString *uri,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrString *moduleName,
                                                                 SZrString *memberName,
                                                                 SZrArray *result) {
    ZR_UNUSED_PARAMETER(context);
    ZR_UNUSED_PARAMETER(uri);

    return analyzer != ZR_NULL
               ? append_imported_member_locations_from_analyzer(state, analyzer, moduleName, memberName, result)
               : ZR_TRUE;
}

static TZrBool project_navigation_append_imported_module_for_uri(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrString *uri,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrString *moduleName,
                                                                 SZrString *memberName,
                                                                 SZrArray *result) {
    ZR_UNUSED_PARAMETER(context);
    ZR_UNUSED_PARAMETER(uri);
    ZR_UNUSED_PARAMETER(memberName);

    return analyzer != ZR_NULL
               ? append_imported_module_locations_from_analyzer(state, analyzer, moduleName, result)
               : ZR_TRUE;
}

static TZrBool project_navigation_append_import_binding_for_uri(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrString *moduleName,
                                                                SZrString *memberName,
                                                                SZrArray *result) {
    ZR_UNUSED_PARAMETER(context);
    ZR_UNUSED_PARAMETER(uri);
    ZR_UNUSED_PARAMETER(memberName);

    return analyzer != ZR_NULL
               ? append_import_binding_locations_from_analyzer(state, analyzer, moduleName, result)
               : ZR_TRUE;
}

static TZrBool project_navigation_append_import_target_for_uri(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrString *moduleName,
                                                               SZrString *memberName,
                                                               SZrArray *result) {
    ZR_UNUSED_PARAMETER(analyzer);
    ZR_UNUSED_PARAMETER(memberName);

    return append_import_target_locations_from_analyzer(state, context, uri, moduleName, result);
}

static TZrBool project_navigation_append_source_reference_for_uri(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrString *moduleName,
                                                                  SZrString *memberName,
                                                                  TZrLspProjectSourceReferenceAppender appender,
                                                                  SZrArray *result) {
    SZrSemanticAnalyzer *analyzer = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleName == ZR_NULL ||
        appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!project_navigation_try_get_analyzer_for_uri(state, context, uri, &analyzer)) {
        return ZR_FALSE;
    }

    return appender(state, context, uri, analyzer, moduleName, memberName, result);
}

static TZrBool project_navigation_append_source_reference_for_path(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   const TZrChar *path,
                                                                   SZrString *moduleName,
                                                                   SZrString *memberName,
                                                                   TZrLspProjectSourceReferenceAppender appender,
                                                                   SZrArray *result) {
    SZrString *uri;

    if (state == ZR_NULL || context == ZR_NULL || path == ZR_NULL || moduleName == ZR_NULL ||
        appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    uri = project_navigation_native_path_to_file_uri(state, path);
    if (uri == ZR_NULL) {
        return ZR_FALSE;
    }

    return project_navigation_append_source_reference_for_uri(state,
                                                              context,
                                                              uri,
                                                              moduleName,
                                                              memberName,
                                                              appender,
                                                              result);
}

static TZrBool project_navigation_append_source_root_references_recursive(
    SZrState *state,
    SZrLspContext *context,
    const TZrChar *directory,
    SZrString *moduleName,
    SZrString *memberName,
    TZrLspProjectSourceReferenceAppender appender,
    SZrArray *result) {
    if (state == ZR_NULL || context == ZR_NULL || directory == ZR_NULL || moduleName == ZR_NULL ||
        appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    {
        TZrChar pattern[ZR_LIBRARY_MAX_PATH_LENGTH];
        WIN32_FIND_DATAA findData;
        HANDLE handle;

        ZrLibrary_File_PathJoin(directory, "*", pattern);
        handle = FindFirstFileA(pattern, &findData);
        if (handle == INVALID_HANDLE_VALUE) {
            return ZR_TRUE;
        }

        do {
            TZrChar childPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            TZrSize nameLength = strlen(findData.cFileName);

            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                continue;
            }

            ZrLibrary_File_PathJoin(directory, findData.cFileName, childPath);
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (!project_navigation_append_source_root_references_recursive(state,
                                                                                context,
                                                                                childPath,
                                                                                moduleName,
                                                                                memberName,
                                                                                appender,
                                                                                result)) {
                    FindClose(handle);
                    return ZR_FALSE;
                }
                continue;
            }

            if (nameLength >= 3 &&
                strcmp(findData.cFileName + nameLength - 3, ".zr") == 0 &&
                !project_navigation_append_source_reference_for_path(state,
                                                                    context,
                                                                    childPath,
                                                                    moduleName,
                                                                    memberName,
                                                                    appender,
                                                                    result)) {
                FindClose(handle);
                return ZR_FALSE;
            }
        } while (FindNextFileA(handle, &findData) != 0);

        FindClose(handle);
        return ZR_TRUE;
    }
#else
    {
        DIR *dir = opendir(directory);
        struct dirent *entry;

        if (dir == ZR_NULL) {
            return ZR_TRUE;
        }

        while ((entry = readdir(dir)) != ZR_NULL) {
            TZrChar childPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            TZrSize nameLength;
            EZrLibrary_File_Exist fileExist;

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            ZrLibrary_File_PathJoin((TZrNativeString)directory, entry->d_name, childPath);
            fileExist = ZrLibrary_File_Exist(childPath);
            if (fileExist == ZR_LIBRARY_FILE_IS_DIRECTORY) {
                if (!project_navigation_append_source_root_references_recursive(state,
                                                                                context,
                                                                                childPath,
                                                                                moduleName,
                                                                                memberName,
                                                                                appender,
                                                                                result)) {
                    closedir(dir);
                    return ZR_FALSE;
                }
                continue;
            }

            nameLength = strlen(entry->d_name);
            if (fileExist == ZR_LIBRARY_FILE_IS_FILE &&
                nameLength >= 3 &&
                strcmp(entry->d_name + nameLength - 3, ".zr") == 0 &&
                !project_navigation_append_source_reference_for_path(state,
                                                                    context,
                                                                    childPath,
                                                                    moduleName,
                                                                    memberName,
                                                                    appender,
                                                                    result)) {
                closedir(dir);
                return ZR_FALSE;
            }
        }

        closedir(dir);
        return ZR_TRUE;
    }
#endif
}

static TZrBool project_navigation_append_project_source_references(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrString *fallbackUri,
    SZrString *moduleName,
    SZrString *memberName,
    TZrLspProjectSourceReferenceAppender appender,
    SZrArray *result) {
    const TZrChar *sourceRootPath;

    if (state == ZR_NULL || context == ZR_NULL || moduleName == ZR_NULL || appender == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (projectIndex == ZR_NULL) {
        return fallbackUri != ZR_NULL
                   ? project_navigation_append_source_reference_for_uri(state,
                                                                       context,
                                                                       fallbackUri,
                                                                       moduleName,
                                                                       memberName,
                                                                       appender,
                                                                       result)
                   : ZR_FALSE;
    }

    sourceRootPath = project_navigation_string_text(projectIndex->sourceRootPath);
    if (sourceRootPath == ZR_NULL || sourceRootPath[0] == '\0') {
        return ZR_TRUE;
    }

    return project_navigation_append_source_root_references_recursive(state,
                                                                      context,
                                                                      sourceRootPath,
                                                                      moduleName,
                                                                      memberName,
                                                                      appender,
                                                                      result);
}

static TZrBool append_project_imported_references(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrLspProjectIndex *projectIndex,
                                                  SZrString *fallbackUri,
                                                  SZrString *moduleName,
                                                  SZrString *memberName,
                                                  SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               memberName,
                                                               project_navigation_append_imported_member_for_uri,
                                                               result);
}

static TZrBool append_imported_module_locations_from_analyzer(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrString *moduleName,
                                                              SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportedModuleLocations(state,
                                                                                 analyzer->ast,
                                                                                 &bindings,
                                                                                 moduleName,
                                                                                 result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool append_import_binding_locations_from_analyzer(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrString *moduleName,
                                                             SZrArray *result) {
    SZrArray bindings;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL || moduleName == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    appended = ZrLanguageServer_LspProject_AppendMatchingImportBindingLocations(state,
                                                                                &bindings,
                                                                                moduleName,
                                                                                result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool append_import_target_locations_from_analyzer(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrString *moduleName,
                                                            SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrArray bindings;
    TZrBool appended;
    TZrBool hasMatchingImport = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        project_navigation_refine_import_binding_target_locations(fileVersion->content,
                                                                  fileVersion->contentLength,
                                                                  &bindings);
    }
    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->moduleName, moduleName)) {
            hasMatchingImport = ZR_TRUE;
            break;
        }
    }

    if (!hasMatchingImport) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        return ZR_TRUE;
    }

    appended = ZrLanguageServer_LspProject_AppendMatchingImportTargetLocations(state,
                                                                               &bindings,
                                                                               moduleName,
                                                                               result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool append_project_imported_module_references(SZrState *state,
                                                         SZrLspContext *context,
                                                         SZrLspProjectIndex *projectIndex,
                                                         SZrString *fallbackUri,
                                                         SZrString *moduleName,
                                                         SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               ZR_NULL,
                                                               project_navigation_append_imported_module_for_uri,
                                                               result);
}

static TZrBool append_project_import_binding_references(SZrState *state,
                                                        SZrLspContext *context,
                                                        SZrLspProjectIndex *projectIndex,
                                                        SZrString *fallbackUri,
                                                        SZrString *moduleName,
                                                        SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               ZR_NULL,
                                                               project_navigation_append_import_binding_for_uri,
                                                               result);
}

static TZrBool append_project_import_target_references(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrLspProjectIndex *projectIndex,
                                                       SZrString *fallbackUri,
                                                       SZrString *moduleName,
                                                       SZrArray *result) {
    return project_navigation_append_project_source_references(state,
                                                               context,
                                                               projectIndex,
                                                               fallbackUri,
                                                               moduleName,
                                                               ZR_NULL,
                                                               project_navigation_append_import_target_for_uri,
                                                               result);
}

static TZrBool project_navigation_resolve_descriptor_plugin_module_from_project(SZrState *state,
                                                                                SZrLspContext *context,
                                                                                SZrLspProjectIndex *projectIndex,
                                                                                SZrString *targetUri,
                                                                                SZrString **outModuleName) {
    if (outModuleName != ZR_NULL) {
        *outModuleName = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || projectIndex == ZR_NULL || targetUri == ZR_NULL ||
        outModuleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize fileIndex = 0; fileIndex < projectIndex->files.length; fileIndex++) {
        SZrLspProjectFileRecord **recordPtr =
            (SZrLspProjectFileRecord **)ZrCore_Array_Get(&projectIndex->files, fileIndex);
        SZrSemanticAnalyzer *analyzer;
        SZrArray bindings;

        if (recordPtr == ZR_NULL || *recordPtr == ZR_NULL) {
            continue;
        }

        analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, (*recordPtr)->uri);
        if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
            continue;
        }

        ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
        ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
        for (TZrSize bindingIndex = 0; bindingIndex < bindings.length; bindingIndex++) {
            SZrLspImportBinding **bindingPtr =
                (SZrLspImportBinding **)ZrCore_Array_Get(&bindings, bindingIndex);
            SZrLspResolvedImportedModule resolved;
            SZrString *declarationUri = ZR_NULL;

            if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL) {
                continue;
            }

            memset(&resolved, 0, sizeof(resolved));
            if (!ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                         analyzer,
                                                                         projectIndex,
                                                                         (*bindingPtr)->moduleName,
                                                                         &resolved) ||
                resolved.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
                resolved.sourceRecord != ZR_NULL ||
                !ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(state,
                                                                          projectIndex,
                                                                          (*bindingPtr)->moduleName,
                                                                          &declarationUri) ||
                declarationUri == ZR_NULL ||
                !ZrLanguageServer_Lsp_StringsEqual(declarationUri, targetUri)) {
                continue;
            }

            *outModuleName = (*bindingPtr)->moduleName;
            ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
            return ZR_TRUE;
        }

        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    }

    return ZR_FALSE;
}

static TZrBool append_locations_as_document_highlights(SZrState *state,
                                                       SZrArray *locations,
                                                       SZrString *uri,
                                                       TZrInt32 kind,
                                                       SZrArray *result) {
    if (state == ZR_NULL || locations == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    for (TZrSize index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        SZrLspDocumentHighlight *highlight;

        if (locationPtr == ZR_NULL || *locationPtr == ZR_NULL || (*locationPtr)->uri == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*locationPtr)->uri, uri)) {
            continue;
        }

        highlight =
            (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
        if (highlight == ZR_NULL) {
            return ZR_FALSE;
        }

        highlight->range = (*locationPtr)->range;
        highlight->kind = kind;
        ZrCore_Array_Push(state, result, &highlight);
    }

    return result->length > 0;
}

static TZrBool append_document_highlight(SZrState *state,
                                         SZrArray *result,
                                         SZrFileRange range,
                                         TZrInt32 kind) {
    SZrLspDocumentHighlight *highlight;

    if (state == ZR_NULL || result == ZR_NULL || range.source == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight->range = ZrLanguageServer_LspRange_FromFileRange(range);
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

static TZrBool project_try_find_imported_member_resolution(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *uri,
                                                           SZrLspPosition position,
                                                           SZrLspProjectIndex **outProjectIndex,
                                                           SZrSemanticAnalyzer **outAnalyzer,
                                                           SZrLspImportedMemberHit *outHit,
                                                           SZrLspResolvedImportedModule *outResolved) {
    SZrLspProjectIndex *projectIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrArray bindings;
    SZrLspImportedMemberHit hit;
    SZrLspResolvedImportedModule resolved;
    TZrBool found = ZR_FALSE;

    if (outProjectIndex != ZR_NULL) {
        *outProjectIndex = ZR_NULL;
    }
    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (ZrLanguageServer_LspProject_FindImportedMemberHit(analyzer->ast, &bindings, fileRange, &hit)) {
        memset(&resolved, 0, sizeof(resolved));
        if (ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(state,
                                                                     analyzer,
                                                                     projectIndex,
                                                                     hit.moduleName,
                                                                     &resolved)) {
            *outHit = hit;
            if (outProjectIndex != ZR_NULL) {
                *outProjectIndex = projectIndex;
            }
            if (outAnalyzer != ZR_NULL) {
                *outAnalyzer = analyzer;
            }
            if (outResolved != ZR_NULL) {
                *outResolved = resolved;
            }
            found = ZR_TRUE;
        }
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return found;
}

static TZrBool project_navigation_try_find_binary_export_declaration_at(
    SZrState *state,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *uri,
    SZrLspPosition position,
    SZrString **outMemberName,
    SZrFileRange *outRange) {
    SZrIoSource *binarySource = ZR_NULL;
    SZrFilePosition filePosition;
    SZrFileRange positionRange;

    if (outMemberName != ZR_NULL) {
        *outMemberName = ZR_NULL;
    }
    if (outRange != ZR_NULL) {
        *outRange = project_navigation_metadata_file_entry_range(uri);
    }
    if (state == ZR_NULL || projectIndex == ZR_NULL || moduleName == ZR_NULL || uri == ZR_NULL ||
        outMemberName == ZR_NULL || outRange == ZR_NULL ||
        !ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(state, projectIndex, moduleName, &binarySource) ||
        binarySource == ZR_NULL || binarySource->modulesLength == 0 || binarySource->modules == ZR_NULL ||
        binarySource->modules[0].entryFunction == ZR_NULL ||
        binarySource->modules[0].entryFunction->typedExportedSymbols == ZR_NULL) {
        if (binarySource != ZR_NULL) {
            ZrCore_Io_ReadSourceFree(state->global, binarySource);
        }
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_LspPosition_ToFilePosition(position);
    positionRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    for (TZrSize index = 0; index < binarySource->modules[0].entryFunction->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol =
            &binarySource->modules[0].entryFunction->typedExportedSymbols[index];
        SZrFileRange symbolRange;
        const TZrChar *symbolNameText;

        if (symbol->name == ZR_NULL || symbol->lineInSourceStart == 0 || symbol->columnInSourceStart == 0 ||
            symbol->lineInSourceEnd == 0 || symbol->columnInSourceEnd == 0) {
            continue;
        }

        symbolRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, (TZrInt32)symbol->lineInSourceStart, (TZrInt32)symbol->columnInSourceStart),
            ZrParser_FilePosition_Create(0, (TZrInt32)symbol->lineInSourceEnd, (TZrInt32)symbol->columnInSourceEnd),
            uri);
        if (!project_navigation_file_range_contains_position(symbolRange, positionRange)) {
            continue;
        }

        symbolNameText = project_navigation_string_text(symbol->name);
        *outMemberName = symbolNameText != ZR_NULL
                             ? ZrCore_String_Create(state, (TZrNativeString)symbolNameText, strlen(symbolNameText))
                             : ZR_NULL;
        *outRange = symbolRange;
        ZrCore_Io_ReadSourceFree(state->global, binarySource);
        return *outMemberName != ZR_NULL;
    }

    ZrCore_Io_ReadSourceFree(state->global, binarySource);
    return ZR_FALSE;
}

static TZrBool project_try_resolve_external_imported_member(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrString *uri,
                                                            SZrLspPosition position,
                                                            SZrLspProjectResolvedExternalImportedMember *outResolved) {
    SZrLspProjectIndex *projectIndex = ZR_NULL;
    SZrLspImportedMemberHit hit;
    SZrLspResolvedImportedModule resolved;

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL ||
        !project_try_find_imported_member_resolution(state,
                                                     context,
                                                     uri,
                                                     position,
                                                     &projectIndex,
                                                     ZR_NULL,
                                                     &hit,
                                                     &resolved) ||
        resolved.sourceRecord != ZR_NULL) {
        return ZR_FALSE;
    }

    outResolved->projectIndex = projectIndex;
    outResolved->moduleName = hit.moduleName;
    outResolved->memberName = hit.memberName;
    outResolved->sourceKind = resolved.sourceKind;

    if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
        projectIndex != ZR_NULL &&
        ZrLanguageServer_LspModuleMetadata_ResolveBinaryExportDeclaration(state,
                                                                          projectIndex,
                                                                          hit.moduleName,
                                                                          hit.memberName,
                                                                          &outResolved->declarationUri,
                                                                          &outResolved->declarationRange)) {
        outResolved->hasDeclaration = ZR_TRUE;
    } else if (resolved.sourceKind == ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA &&
               projectIndex != ZR_NULL &&
               ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state,
                                                                         projectIndex,
                                                                         hit.moduleName,
                                                                         &outResolved->declarationUri) &&
               outResolved->declarationUri != ZR_NULL) {
        outResolved->declarationRange = project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    } else if (ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(state,
                                                                         projectIndex,
                                                                         hit.moduleName,
                                                                         &outResolved->declarationUri) &&
               outResolved->declarationUri != ZR_NULL) {
        outResolved->declarationRange =
            project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspProjectResolvedExternalMetadataDeclaration *outResolved) {
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *sourceRecord;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar moduleNameBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outResolved != ZR_NULL) {
        memset(outResolved, 0, sizeof(*outResolved));
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    if (projectIndex == ZR_NULL || !project_navigation_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        return ZR_FALSE;
    }

    sourceRecord = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri);
    if (sourceRecord != ZR_NULL &&
        sourceRecord->moduleName != ZR_NULL &&
        project_navigation_position_is_module_entry(position)) {
        outResolved->projectIndex = projectIndex;
        outResolved->moduleName = sourceRecord->moduleName;
        outResolved->sourceKind = sourceRecord->isFfiWrapperSource
                                      ? ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER
                                      : ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE;
        outResolved->declarationUri = sourceRecord->uri != ZR_NULL ? sourceRecord->uri : uri;
        outResolved->declarationRange =
            project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        outResolved->hasDeclaration = ZR_TRUE;
        return ZR_TRUE;
    }

    if (ZrLanguageServer_LspProject_DeriveBinaryModuleNameFromPath(projectIndex,
                                                                   nativePath,
                                                                   moduleNameBuffer,
                                                                   sizeof(moduleNameBuffer))) {
        SZrString *binaryDeclarationUri = ZR_NULL;

        outResolved->projectIndex = projectIndex;
        outResolved->moduleName = ZrCore_String_CreateFromNative(state, moduleNameBuffer);
        outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA;
        outResolved->hasDeclaration = outResolved->moduleName != ZR_NULL &&
                                      ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(state,
                                                                                                projectIndex,
                                                                                                outResolved->moduleName,
                                                                                                &binaryDeclarationUri) &&
                                      binaryDeclarationUri != ZR_NULL &&
                                      ZrLanguageServer_Lsp_StringsEqual(binaryDeclarationUri, uri);
        if (!outResolved->hasDeclaration) {
            return ZR_FALSE;
        }

        outResolved->declarationUri = binaryDeclarationUri;
        outResolved->declarationRange = project_navigation_metadata_file_entry_range(outResolved->declarationUri);
        if (!project_navigation_position_is_module_entry(position)) {
            if (!project_navigation_try_find_binary_export_declaration_at(state,
                                                                          projectIndex,
                                                                          outResolved->moduleName,
                                                                          outResolved->declarationUri,
                                                                          position,
                                                                          &outResolved->memberName,
                                                                          &outResolved->declarationRange)) {
                return ZR_FALSE;
            }
            outResolved->hasDeclaration = outResolved->memberName != ZR_NULL;
        }
        return ZR_TRUE;
    }

    if (project_navigation_resolve_descriptor_plugin_module_from_project(state,
                                                                         context,
                                                                         projectIndex,
                                                                         uri,
                                                                         &outResolved->moduleName)) {
        outResolved->projectIndex = projectIndex;
        outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
        outResolved->declarationUri = uri;
        outResolved->declarationRange = project_navigation_metadata_file_entry_range(uri);
        outResolved->hasDeclaration = outResolved->moduleName != ZR_NULL;
        return outResolved->hasDeclaration;
    }

    if (state->global != ZR_NULL) {
        ZrLibRegisteredModuleInfo moduleInfo;

        memset(&moduleInfo, 0, sizeof(moduleInfo));
        if (ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(state->global, nativePath, &moduleInfo) &&
            moduleInfo.moduleName != ZR_NULL &&
            (moduleInfo.registrationKind == ZR_LIB_NATIVE_MODULE_REGISTRATION_KIND_DESCRIPTOR_PLUGIN ||
             moduleInfo.isDescriptorPlugin)) {
            outResolved->projectIndex = projectIndex;
            outResolved->moduleName = ZrCore_String_Create(state,
                                                           (TZrNativeString)moduleInfo.moduleName,
                                                           strlen(moduleInfo.moduleName));
            outResolved->sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN;
            outResolved->declarationUri = uri;
            outResolved->declarationRange = project_navigation_metadata_file_entry_range(uri);
            outResolved->hasDeclaration = outResolved->moduleName != ZR_NULL;
            return outResolved->hasDeclaration;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationReferences(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    TZrBool includeDeclaration,
    SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || resolved == ZR_NULL || result == ZR_NULL ||
        resolved->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (includeDeclaration &&
        resolved->hasDeclaration &&
        resolved->declarationUri != ZR_NULL &&
        !append_lsp_location(state, result, resolved->declarationUri, resolved->declarationRange)) {
        return ZR_FALSE;
    }

    if (resolved->memberName != ZR_NULL) {
        appended = append_project_imported_references(state,
                                                      context,
                                                      resolved->projectIndex,
                                                      queryUri,
                                                      resolved->moduleName,
                                                      resolved->memberName,
                                                      result);
    } else {
        appended = append_project_import_target_references(state,
                                                           context,
                                                           resolved->projectIndex,
                                                           queryUri,
                                                           resolved->moduleName,
                                                           result);
        appended = append_project_import_binding_references(state,
                                                            context,
                                                            resolved->projectIndex,
                                                            queryUri,
                                                            resolved->moduleName,
                                                            result) || appended;
        appended = append_project_imported_module_references(state,
                                                             context,
                                                             resolved->projectIndex,
                                                             queryUri,
                                                             resolved->moduleName,
                                                             result) || appended;
    }

    return appended || result->length > 0;
}

TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationHighlights(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    SZrArray *result) {
    SZrArray locations;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || resolved == ZR_NULL || queryUri == ZR_NULL || result == ZR_NULL ||
        resolved->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (resolved->hasDeclaration &&
        resolved->declarationUri != ZR_NULL &&
        ZrLanguageServer_Lsp_StringsEqual(resolved->declarationUri, queryUri)) {
        appended = append_document_highlight(state, result, resolved->declarationRange, 3);
    }

    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (resolved->memberName != ZR_NULL) {
        appended = append_project_imported_references(state,
                                                      context,
                                                      resolved->projectIndex,
                                                      queryUri,
                                                      resolved->moduleName,
                                                      resolved->memberName,
                                                      &locations) || appended;
    } else {
        appended = append_project_import_target_references(state,
                                                           context,
                                                           resolved->projectIndex,
                                                           queryUri,
                                                           resolved->moduleName,
                                                           &locations) || appended;
        appended = append_project_import_binding_references(state,
                                                            context,
                                                            resolved->projectIndex,
                                                            queryUri,
                                                            resolved->moduleName,
                                                            &locations) || appended;
        appended = append_project_imported_module_references(state,
                                                             context,
                                                             resolved->projectIndex,
                                                             queryUri,
                                                             resolved->moduleName,
                                                             &locations) || appended;
    }

    if (locations.length > 0) {
        appended = append_locations_as_document_highlights(state, &locations, queryUri, 2, result) || appended;
    }

    ZrCore_Array_Free(state, &locations);
    return appended;
}

static TZrBool project_resolve_symbol_at_position(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  TZrBool allowLocalProjectSymbol,
                                                  SZrLspProjectResolvedSymbol *outResolved) {
    SZrLspProjectIndex *projectIndex;
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrArray bindings;
    SZrLspImportedMemberHit hit;
    SZrLspProjectFileRecord *record;
    SZrSemanticAnalyzer *targetAnalyzer;
    SZrSymbol *targetSymbol;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outResolved == ZR_NULL) {
        return ZR_FALSE;
    }

    projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    }
    if (projectIndex == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    if (ZrLanguageServer_LspProject_FindImportedMemberHit(analyzer->ast, &bindings, fileRange, &hit)) {
        ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
        record = ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, hit.moduleName);
        if (record == ZR_NULL) {
            return ZR_FALSE;
        }

        targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
        if (targetAnalyzer == ZR_NULL) {
            targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, record->uri);
        }
        targetSymbol = find_global_symbol_by_name(targetAnalyzer, hit.memberName, record->uri);
        if (targetAnalyzer == ZR_NULL || targetSymbol == ZR_NULL) {
            return ZR_FALSE;
        }

        outResolved->projectIndex = projectIndex;
        outResolved->record = record;
        outResolved->analyzer = targetAnalyzer;
        outResolved->symbol = targetSymbol;
        return ZR_TRUE;
    }
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);

    if (!allowLocalProjectSymbol) {
        return ZR_FALSE;
    }

    targetSymbol = ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(analyzer, fileRange);
    if (!symbol_is_project_global(analyzer, targetSymbol) || targetSymbol->location.source == ZR_NULL) {
        return ZR_FALSE;
    }

    record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, targetSymbol->location.source);
    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    targetAnalyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, record->uri);
    if (targetAnalyzer == ZR_NULL) {
        targetAnalyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, record->uri);
    }
    if (targetAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    outResolved->projectIndex = projectIndex;
    outResolved->record = record;
    outResolved->analyzer = targetAnalyzer;
    outResolved->symbol = targetSymbol;
    return ZR_TRUE;
}

static TZrBool project_try_append_external_imported_member_definition(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result) {
    SZrLspProjectResolvedExternalImportedMember resolved;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL ||
        !project_try_resolve_external_imported_member(state, context, uri, position, &resolved) ||
        !resolved.hasDeclaration || resolved.declarationUri == ZR_NULL) {
        return ZR_FALSE;
    }

    return append_lsp_location(state,
                               result,
                               resolved.declarationUri,
                               resolved.declarationRange);
}

TZrBool ZrLanguageServer_Lsp_ProjectTryGetDefinition(SZrState *state,
                                                     SZrLspContext *context,
                                                     SZrString *uri,
                                                     SZrLspPosition position,
                                                     SZrArray *result) {
    SZrLspProjectResolvedSymbol resolved;
    SZrLspProjectResolvedExternalMetadataDeclaration externalDeclaration;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (project_try_append_external_imported_member_definition(state, context, uri, position, result)) {
        return ZR_TRUE;
    }

    if (ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(state,
                                                                       context,
                                                                       uri,
                                                                       position,
                                                                       &externalDeclaration) &&
        externalDeclaration.hasDeclaration && externalDeclaration.declarationUri != ZR_NULL) {
        return append_lsp_location(state,
                                   result,
                                   externalDeclaration.declarationUri,
                                   externalDeclaration.declarationRange);
    }

    if (!project_resolve_symbol_at_position(state, context, uri, position, ZR_FALSE, &resolved)) {
        return ZR_FALSE;
    }

    return append_lsp_location(state,
                               result,
                               resolved.record->uri,
                               ZrLanguageServer_Lsp_GetSymbolLookupRange(resolved.symbol));
}

TZrBool ZrLanguageServer_Lsp_ProjectTryFindReferences(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrLspPosition position,
                                                      TZrBool includeDeclaration,
                                                      SZrArray *result) {
    SZrLspProjectResolvedSymbol resolved;
    SZrLspProjectResolvedExternalMetadataDeclaration externalDeclaration;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    {
        SZrLspProjectResolvedExternalImportedMember externalResolved;

        if (project_try_resolve_external_imported_member(state, context, uri, position, &externalResolved)) {
            if (includeDeclaration && externalResolved.hasDeclaration &&
                !append_lsp_location(state,
                                     result,
                                     externalResolved.declarationUri,
                                     externalResolved.declarationRange)) {
                return ZR_FALSE;
            }

            return append_project_imported_references(state,
                                                      context,
                                                      externalResolved.projectIndex,
                                                      uri,
                                                      externalResolved.moduleName,
                                                      externalResolved.memberName,
                                                      result);
        }
    }

    if (ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(state,
                                                                       context,
                                                                       uri,
                                                                       position,
                                                                       &externalDeclaration)) {
        if (includeDeclaration && externalDeclaration.hasDeclaration &&
            !append_lsp_location(state,
                                 result,
                                 externalDeclaration.declarationUri,
                                 externalDeclaration.declarationRange)) {
            return ZR_FALSE;
        }

        return externalDeclaration.memberName != ZR_NULL
                   ? append_project_imported_references(state,
                                                       context,
                                                       externalDeclaration.projectIndex,
                                                       uri,
                                                       externalDeclaration.moduleName,
                                                       externalDeclaration.memberName,
                                                       result)
                   : (append_project_import_target_references(state,
                                                              context,
                                                              externalDeclaration.projectIndex,
                                                              uri,
                                                              externalDeclaration.moduleName,
                                                              result) &&
                      append_project_import_binding_references(state,
                                                               context,
                                                               externalDeclaration.projectIndex,
                                                               uri,
                                                               externalDeclaration.moduleName,
                                                               result) &&
                      append_project_imported_module_references(state,
                                                                context,
                                                                externalDeclaration.projectIndex,
                                                                uri,
                                                                externalDeclaration.moduleName,
                                                                result));
    }

    if (!project_resolve_symbol_at_position(state, context, uri, position, ZR_TRUE, &resolved)) {
        return ZR_FALSE;
    }

    if (!append_symbol_references_from_tracker(state,
                                               resolved.analyzer,
                                               resolved.symbol,
                                               includeDeclaration,
                                               result)) {
        return ZR_FALSE;
    }

    return append_project_imported_references(state,
                                              context,
                                              resolved.projectIndex,
                                              uri,
                                              resolved.record->moduleName,
                                              resolved.symbol->name,
                                              result);
}

TZrBool ZrLanguageServer_Lsp_ProjectTryGetDocumentHighlights(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrString *uri,
                                                             SZrLspPosition position,
                                                             SZrArray *result) {
    SZrLspProjectResolvedExternalMetadataDeclaration externalDeclaration;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrLspImportedMemberHit hit;
    SZrArray locations;
    TZrBool appended;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(state,
                                                                       context,
                                                                       uri,
                                                                       position,
                                                                       &externalDeclaration) &&
        externalDeclaration.hasDeclaration &&
        externalDeclaration.declarationUri != ZR_NULL &&
        ZrLanguageServer_Lsp_StringsEqual(externalDeclaration.declarationUri, uri)) {
        return append_document_highlight(state, result, externalDeclaration.declarationRange, 3);
    }

    if (!project_try_find_imported_member_resolution(state,
                                                     context,
                                                     uri,
                                                     position,
                                                     ZR_NULL,
                                                     &analyzer,
                                                     &hit,
                                                     ZR_NULL)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &locations, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    appended = append_imported_member_locations_from_analyzer(state,
                                                              analyzer,
                                                              hit.moduleName,
                                                              hit.memberName,
                                                              &locations);
    if (!appended) {
        ZrCore_Array_Free(state, &locations);
        return ZR_FALSE;
    }

    appended = append_locations_as_document_highlights(state, &locations, uri, 2, result);
    ZrCore_Array_Free(state, &locations);
    return appended;
}
