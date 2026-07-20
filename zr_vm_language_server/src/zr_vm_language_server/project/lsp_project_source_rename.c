#include "project/lsp_project_internal.h"

#include <ctype.h>
#include <string.h>

typedef struct SZrLspSourceRenameResolution {
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *record;
    TZrChar newPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar newModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrLspSourceRenameResolution;

static const TZrChar *source_rename_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(value)
                   : ZrCore_String_GetNativeString(value);
}

static TZrBool source_rename_uri_ends_with(SZrString *uri,
                                           const TZrChar *suffix) {
    const TZrChar *text = source_rename_string_text(uri);
    TZrSize textLength;
    TZrSize suffixLength;

    if (text == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }
    textLength = strlen(text);
    suffixLength = strlen(suffix);
    return textLength >= suffixLength &&
                   memcmp(text + textLength - suffixLength, suffix, suffixLength) == 0
           ? ZR_TRUE
           : ZR_FALSE;
}

static void source_rename_normalize_path(const TZrChar *path,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    TZrSize writeIndex = 0U;

    if (buffer == ZR_NULL || bufferSize == 0U) {
        return;
    }
    buffer[0] = '\0';
    if (path == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0U;
         path[index] != '\0' && writeIndex + 1U < bufferSize;
         index++) {
        TZrChar current = path[index] == '\\' ? '/' : path[index];
#ifdef ZR_VM_PLATFORM_IS_WIN
        current = (TZrChar)tolower((unsigned char)current);
#endif
        buffer[writeIndex++] = current;
    }
    while (writeIndex > 1U && buffer[writeIndex - 1U] == '/') {
        writeIndex--;
    }
    buffer[writeIndex] = '\0';
}

static TZrBool source_rename_path_is_within(const TZrChar *path,
                                            const TZrChar *directory) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize directoryLength;

    source_rename_normalize_path(path, normalizedPath, sizeof(normalizedPath));
    source_rename_normalize_path(
            directory, normalizedDirectory, sizeof(normalizedDirectory));
    directoryLength = strlen(normalizedDirectory);
    return directoryLength > 0U &&
                   strncmp(normalizedPath, normalizedDirectory, directoryLength) == 0 &&
                   (normalizedPath[directoryLength] == '\0' ||
                    normalizedPath[directoryLength] == '/')
           ? ZR_TRUE
           : ZR_FALSE;
}

static TZrBool source_rename_resolve(SZrState *state,
                                     SZrLspContext *context,
                                     SZrString *oldUri,
                                     SZrString *newUri,
                                     SZrLspSourceRenameResolution *outResolution) {
    if (outResolution != ZR_NULL) {
        memset(outResolution, 0, sizeof(*outResolution));
    }
    if (state == ZR_NULL || context == ZR_NULL || oldUri == ZR_NULL ||
        newUri == ZR_NULL || outResolution == ZR_NULL ||
        ZrLanguageServer_Lsp_StringsEqual(oldUri, newUri) ||
        !source_rename_uri_ends_with(oldUri, ".zr") ||
        !source_rename_uri_ends_with(newUri, ".zr") ||
        !ZrLanguageServer_Lsp_FileUriToNativePath(
                newUri,
                outResolution->newPath,
                sizeof(outResolution->newPath))) {
        return ZR_FALSE;
    }

    for (TZrSize projectOffset = 0U;
         projectOffset < context->projectIndexes.length;
         projectOffset++) {
        SZrLspProjectIndex **projectPtr =
                (SZrLspProjectIndex **)ZrCore_Array_Get(
                        &context->projectIndexes, projectOffset);
        SZrLspProjectFileRecord *record;
        SZrLspProjectFileRecord *collision;

        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        record = ZrLanguageServer_LspProject_FindRecordByUri(*projectPtr, oldUri);
        if (record == ZR_NULL) {
            continue;
        }
        if ((*projectPtr)->sourceRootPath == ZR_NULL ||
            !source_rename_path_is_within(
                    outResolution->newPath,
                    source_rename_string_text((*projectPtr)->sourceRootPath))) {
            return ZR_FALSE;
        }

        collision = ZrLanguageServer_LspProject_FindRecordByUri(*projectPtr, newUri);
        if (collision != ZR_NULL && collision != record) {
            return ZR_FALSE;
        }
        if ((*projectPtr)->project == ZR_NULL ||
            !ZrLibrary_Project_DeriveCurrentModuleKey(
                    (*projectPtr)->project,
                    outResolution->newPath,
                    ZR_NULL,
                    outResolution->newModuleName,
                    sizeof(outResolution->newModuleName),
                    ZR_NULL,
                    0U)) {
            return ZR_FALSE;
        }

        outResolution->projectIndex = *projectPtr;
        outResolution->record = record;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool source_rename_append_location(SZrState *state,
                                             SZrLspContext *context,
                                             SZrArray *locations,
                                             SZrString *fallbackUri,
                                             SZrFileRange range) {
    SZrLspLocation *location;
    SZrString *locationUri;

    if (state == ZR_NULL || context == ZR_NULL || locations == ZR_NULL ||
        (fallbackUri == ZR_NULL && range.source == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (!locations->isValid) {
        ZrCore_Array_Init(state,
                          locations,
                          sizeof(SZrLspLocation *),
                          ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    locationUri = range.source != ZR_NULL ? range.source : fallbackUri;
    location->uri = locationUri;
    location->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, locationUri, range);
    ZrCore_Array_Push(state, locations, &location);
    return ZR_TRUE;
}

static TZrBool source_rename_append_explicit_module_declaration(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSourceRenameResolution *resolution,
        SZrString *oldUri,
        SZrArray *locations) {
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *moduleDeclaration;
    SZrAstNode *moduleName;
    SZrFileRange moduleNameRange;
    const TZrChar *moduleNameText;
    TZrSize moduleNameLength;

    if (state == ZR_NULL || context == ZR_NULL || resolution == ZR_NULL ||
        resolution->record == ZR_NULL || oldUri == ZR_NULL ||
        locations == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, oldUri);
    if (analyzer == ZR_NULL) {
        analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
                state, context, oldUri);
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL ||
        analyzer->ast->type != ZR_AST_SCRIPT) {
        return ZR_TRUE;
    }

    moduleDeclaration = analyzer->ast->data.script.moduleName;
    if (moduleDeclaration == ZR_NULL ||
        moduleDeclaration->type != ZR_AST_MODULE_DECLARATION) {
        return ZR_TRUE;
    }
    moduleName = moduleDeclaration->data.moduleDeclaration.name;
    if (moduleName == ZR_NULL || moduleName->type != ZR_AST_STRING_LITERAL ||
        moduleName->data.stringLiteral.value == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(
                moduleName->data.stringLiteral.value,
                resolution->record->moduleName)) {
        return ZR_TRUE;
    }

    moduleNameText = source_rename_string_text(
            moduleName->data.stringLiteral.value);
    if (moduleNameText == ZR_NULL) {
        return ZR_TRUE;
    }
    moduleNameRange = moduleName->location;
    moduleNameLength = strlen(moduleNameText);
    if (moduleNameRange.start.line == moduleNameRange.end.line &&
        moduleNameRange.end.column - moduleNameRange.start.column ==
                (TZrInt32)moduleNameLength + 2) {
        moduleNameRange.start.column++;
        moduleNameRange.end.column--;
    }
    if (moduleNameRange.end.offset >= moduleNameRange.start.offset &&
        moduleNameRange.end.offset - moduleNameRange.start.offset ==
                moduleNameLength + 2U) {
        moduleNameRange.start.offset++;
        moduleNameRange.end.offset--;
    }

    return source_rename_append_location(
            state, context, locations, oldUri, moduleNameRange);
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_PrepareSourceRename(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri) {
    SZrLspSourceRenameResolution resolution;
    SZrString *newPathString;

    if (!source_rename_resolve(
                state, context, oldUri, newUri, &resolution)) {
        return ZR_FALSE;
    }

    newPathString = ZrCore_String_Create(
            state, resolution.newPath, strlen(resolution.newPath));
    if (newPathString == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, oldUri);
    if (context->parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_RemoveFile(
                state, context->parser, oldUri);
    }

    resolution.record->uri = newUri;
    resolution.record->path = newPathString;
    return ZR_TRUE;
}

static TZrBool source_rename_collect_edits(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri,
        SZrString **outNewModuleName,
        SZrArray *outLocations) {
    SZrLspSourceRenameResolution resolution;
    SZrString *newModuleName;

    if (outNewModuleName != ZR_NULL) {
        *outNewModuleName = ZR_NULL;
    }
    if (outNewModuleName == ZR_NULL || outLocations == ZR_NULL ||
        outLocations->length != 0U ||
        !source_rename_resolve(
                state, context, oldUri, newUri, &resolution) ||
        resolution.record == ZR_NULL || resolution.record->moduleName == ZR_NULL ||
        source_rename_string_text(resolution.record->moduleName) == ZR_NULL ||
        strcmp(source_rename_string_text(resolution.record->moduleName),
               resolution.newModuleName) == 0) {
        return ZR_FALSE;
    }

    newModuleName = ZrCore_String_Create(
            state,
            resolution.newModuleName,
            strlen(resolution.newModuleName));
    if (newModuleName == ZR_NULL ||
        !source_rename_append_explicit_module_declaration(
                state,
                context,
                &resolution,
                oldUri,
                outLocations) ||
        !ZrLanguageServer_LspProject_AppendProjectImportTargetReferences(
                state,
                context,
                resolution.projectIndex,
                oldUri,
                resolution.record->moduleName,
                outLocations) ||
        outLocations->length == 0U) {
        return ZR_FALSE;
    }

    *outNewModuleName = newModuleName;
    return ZR_TRUE;
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_CollectSourceRenameEdits(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri,
        SZrString **outNewModuleName,
        SZrArray *outLocations) {
    return source_rename_collect_edits(
            state,
            context,
            oldUri,
            newUri,
            outNewModuleName,
            outLocations);
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_CollectSourceRenameEditPlan(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri,
        SZrString **outNewModuleName,
        SZrArray *outLocations,
        SZrArray *outDocumentSnapshots) {
    if (outDocumentSnapshots == ZR_NULL || outDocumentSnapshots->length != 0U ||
        !source_rename_collect_edits(
                state,
                context,
                oldUri,
                newUri,
                outNewModuleName,
                outLocations)) {
        return ZR_FALSE;
    }
    return ZrLanguageServer_LspWorkspaceEdit_CaptureDocumentSnapshots(
            state, context, outLocations, outDocumentSnapshots);
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_ValidateSourceRenameEditPlan(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *documentSnapshots) {
    return ZrLanguageServer_LspWorkspaceEdit_ValidateDocumentSnapshots(
            state, context, documentSnapshots);
}

ZR_LANGUAGE_SERVER_API const SZrLspSourceRenameDocumentSnapshot *
ZrLanguageServer_LspProject_FindSourceRenameDocumentSnapshot(
        const SZrArray *documentSnapshots,
        SZrString *uri) {
    return ZrLanguageServer_LspWorkspaceEdit_FindDocumentSnapshot(
            documentSnapshots, uri);
}
