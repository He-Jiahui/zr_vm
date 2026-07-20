#include "project/lsp_project_internal.h"

#include <ctype.h>
#include <string.h>

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

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_PrepareSourceRename(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri) {
    TZrChar newPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || context == ZR_NULL || oldUri == ZR_NULL ||
        newUri == ZR_NULL || !source_rename_uri_ends_with(oldUri, ".zr") ||
        !source_rename_uri_ends_with(newUri, ".zr") ||
        !ZrLanguageServer_Lsp_FileUriToNativePath(
                newUri, newPath, sizeof(newPath))) {
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
        SZrString *newPathString;

        if (projectPtr == ZR_NULL || *projectPtr == ZR_NULL) {
            continue;
        }

        record = ZrLanguageServer_LspProject_FindRecordByUri(*projectPtr, oldUri);
        if (record == ZR_NULL) {
            continue;
        }
        if ((*projectPtr)->sourceRootPath == ZR_NULL ||
            !source_rename_path_is_within(
                    newPath,
                    source_rename_string_text((*projectPtr)->sourceRootPath))) {
            return ZR_FALSE;
        }

        collision = ZrLanguageServer_LspProject_FindRecordByUri(*projectPtr, newUri);
        if (collision != ZR_NULL && collision != record) {
            return ZR_FALSE;
        }

        newPathString = ZrCore_String_Create(state, newPath, strlen(newPath));
        if (newPathString == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrLanguageServer_Lsp_RemoveAnalyzer(state, context, oldUri);
        if (context->parser != ZR_NULL) {
            ZrLanguageServer_IncrementalParser_RemoveFile(
                    state, context->parser, oldUri);
        }

        record->uri = newUri;
        record->path = newPathString;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
