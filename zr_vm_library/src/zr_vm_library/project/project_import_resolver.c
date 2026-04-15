#include "zr_vm_library/project.h"

#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const TZrChar *project_resolver_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static void project_resolver_set_error(TZrChar *errorBuffer,
                                       TZrSize errorBufferSize,
                                       const TZrChar *format,
                                       ...) {
    va_list arguments;

    if (errorBuffer == ZR_NULL || errorBufferSize == 0) {
        return;
    }

    errorBuffer[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(errorBuffer, errorBufferSize, format, arguments);
    va_end(arguments);
}

static TZrBool project_resolver_copy_text(const TZrChar *text, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (length + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, text, length + 1);
    return ZR_TRUE;
}

static TZrBool project_resolver_has_suffix(const TZrChar *text,
                                           TZrSize length,
                                           const TZrChar *suffix,
                                           TZrSize suffixLength) {
    return text != ZR_NULL &&
           suffix != ZR_NULL &&
           length >= suffixLength &&
           memcmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

static TZrBool project_resolver_is_path_separator(TZrChar ch) {
    return ch == '/' || ch == '\\';
}

static TZrBool project_resolver_path_prefix_matches(const TZrChar *lhs,
                                                    const TZrChar *rhs,
                                                    TZrSize length) {
    if (lhs == ZR_NULL || rhs == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length; index++) {
        TZrChar left = lhs[index];
        TZrChar right = rhs[index];

        if (left == right) {
            continue;
        }
        if (project_resolver_is_path_separator(left) && project_resolver_is_path_separator(right)) {
            continue;
        }
#if defined(ZR_PLATFORM_WIN)
        if (left >= 'A' && left <= 'Z') {
            left = (TZrChar)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (TZrChar)(right - 'A' + 'a');
        }
        if (left == right) {
            continue;
        }
#endif
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool project_resolver_is_absolute_path(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return ZR_TRUE;
    }

    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
           path[1] == ':';
}

static TZrBool project_resolver_normalize_module_key_text(const TZrChar *modulePath,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    TZrSize start = 0;
    TZrSize length;
    TZrSize writeIndex = 0;
    TZrBool previousWasSeparator = ZR_FALSE;

    if (modulePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    length = strlen(modulePath);
    while (length > 0 && (modulePath[length - 1] == '/' || modulePath[length - 1] == '\\')) {
        length--;
    }

    if (project_resolver_has_suffix(modulePath,
                                    length,
                                    ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
                                    ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION_LENGTH)) {
        length -= ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION_LENGTH;
    } else if (project_resolver_has_suffix(modulePath,
                                           length,
                                           ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                           ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH)) {
        length -= ZR_VM_BINARY_MODULE_FILE_EXTENSION_LENGTH;
    } else if (project_resolver_has_suffix(modulePath,
                                           length,
                                           ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                           ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH)) {
        length -= ZR_VM_SOURCE_MODULE_FILE_EXTENSION_LENGTH;
    }

    while (start < length && (modulePath[start] == '/' || modulePath[start] == '\\')) {
        start++;
    }

    if (start >= length) {
        return ZR_FALSE;
    }

    for (; start < length; start++) {
        TZrChar current = modulePath[start];
        if (current == '/' || current == '\\') {
            if (previousWasSeparator) {
                continue;
            }
            if (writeIndex + 1 >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[writeIndex++] = '/';
            previousWasSeparator = ZR_TRUE;
            continue;
        }

        if (writeIndex + 1 >= bufferSize) {
            return ZR_FALSE;
        }
        buffer[writeIndex++] = current;
        previousWasSeparator = ZR_FALSE;
    }

    while (writeIndex > 0 && buffer[writeIndex - 1] == '/') {
        writeIndex--;
    }
    if (writeIndex == 0) {
        return ZR_FALSE;
    }

    buffer[writeIndex] = '\0';
    return ZR_TRUE;
}

static TZrBool project_resolver_build_source_root(const SZrLibrary_Project *project,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    TZrChar joinedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *projectDirectory;
    const TZrChar *sourceRoot;

    if (project == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    projectDirectory = project_resolver_string_text(project->directory);
    sourceRoot = project_resolver_string_text(project->source);
    if (projectDirectory == ZR_NULL || sourceRoot == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, sourceRoot, joinedPath);
    return ZrLibrary_File_NormalizePath(joinedPath, buffer, bufferSize);
}

static TZrBool project_resolver_relative_path_from_root(const TZrChar *normalizedRoot,
                                                        const TZrChar *normalizedPath,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize) {
    TZrSize rootLength;

    if (normalizedRoot == ZR_NULL || normalizedPath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    rootLength = strlen(normalizedRoot);
    if (rootLength == 0 || !project_resolver_path_prefix_matches(normalizedRoot, normalizedPath, rootLength)) {
        return ZR_FALSE;
    }

    if (normalizedPath[rootLength] == '\0') {
        return project_resolver_copy_text("", buffer, bufferSize);
    }

    if (!project_resolver_is_path_separator(normalizedPath[rootLength])) {
        return ZR_FALSE;
    }

    return project_resolver_copy_text(normalizedPath + rootLength + 1, buffer, bufferSize);
}

static TZrBool project_resolver_convert_dot_suffix(const TZrChar *text,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    TZrSize writeIndex = 0;
    TZrSize segmentLength = 0;
    TZrSize index = 0;

    if (text == ZR_NULL || text[0] == '\0' || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    for (;;) {
        TZrChar current = text[index++];
        if (current == '/' || current == '\\') {
            return ZR_FALSE;
        }

        if (current == '.' || current == '\0') {
            if (segmentLength == 0) {
                return ZR_FALSE;
            }
            if (current == '\0') {
                break;
            }
            if (writeIndex + 1 >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[writeIndex++] = '/';
            segmentLength = 0;
            continue;
        }

        if (writeIndex + 1 >= bufferSize) {
            return ZR_FALSE;
        }
        buffer[writeIndex++] = current;
        segmentLength++;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

static TZrBool project_resolver_join_module_paths(const TZrChar *prefix,
                                                  const TZrChar *suffix,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    TZrSize prefixLength;
    TZrSize suffixLength;
    TZrSize totalLength;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (prefix == ZR_NULL || prefix[0] == '\0') {
        return project_resolver_copy_text(suffix, buffer, bufferSize);
    }
    if (suffix == ZR_NULL || suffix[0] == '\0') {
        return project_resolver_copy_text(prefix, buffer, bufferSize);
    }

    prefixLength = strlen(prefix);
    suffixLength = strlen(suffix);
    totalLength = prefixLength + 1 + suffixLength;
    if (totalLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, prefix, prefixLength);
    buffer[prefixLength] = '/';
    memcpy(buffer + prefixLength + 1, suffix, suffixLength);
    buffer[totalLength] = '\0';
    return ZR_TRUE;
}

static TZrBool project_resolver_apply_relative_import(const TZrChar *currentModuleKey,
                                                      TZrSize parentLevels,
                                                      const TZrChar *suffixPath,
                                                      TZrChar *buffer,
                                                      TZrSize bufferSize) {
    TZrChar currentNormalized[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar currentDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *lastSlash;

    if (currentModuleKey == ZR_NULL || suffixPath == ZR_NULL || suffixPath[0] == '\0' ||
        buffer == ZR_NULL || bufferSize == 0 ||
        !ZrLibrary_Project_NormalizeModuleKey(currentModuleKey, currentNormalized, sizeof(currentNormalized))) {
        return ZR_FALSE;
    }

    lastSlash = strrchr(currentNormalized, '/');
    if (lastSlash == ZR_NULL) {
        currentDirectory[0] = '\0';
    } else {
        TZrSize directoryLength = (TZrSize)(lastSlash - currentNormalized);
        if (directoryLength + 1 > sizeof(currentDirectory)) {
            return ZR_FALSE;
        }
        memcpy(currentDirectory, currentNormalized, directoryLength);
        currentDirectory[directoryLength] = '\0';
    }

    while (parentLevels > 0) {
        lastSlash = strrchr(currentDirectory, '/');
        if (currentDirectory[0] == '\0') {
            return ZR_FALSE;
        }
        if (lastSlash == ZR_NULL) {
            currentDirectory[0] = '\0';
        } else {
            *lastSlash = '\0';
        }
        parentLevels--;
    }

    return project_resolver_join_module_paths(currentDirectory, suffixPath, buffer, bufferSize);
}

static const SZrLibrary_Project *project_resolver_from_candidate(TZrPtr candidate) {
    const SZrLibrary_Project *project = (const SZrLibrary_Project *)candidate;

    return project != ZR_NULL && project->signature == ZR_LIBRARY_PROJECT_SIGNATURE ? project : ZR_NULL;
}

ZR_LIBRARY_API const SZrLibrary_Project *ZrLibrary_Project_GetFromGlobal(const SZrGlobalState *global) {
    const SZrLibrary_Project *project;

    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    project = project_resolver_from_candidate(global->sourceLoaderUserData);
    if (project != ZR_NULL) {
        return project;
    }

    return project_resolver_from_candidate(global->userData);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_NormalizeModuleKey(const TZrChar *modulePath,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize) {
    return project_resolver_normalize_module_key_text(modulePath, buffer, bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_DeriveCurrentModuleKey(const SZrLibrary_Project *project,
                                                                const TZrChar *sourceName,
                                                                const TZrChar *explicitModuleKey,
                                                                TZrChar *buffer,
                                                                TZrSize bufferSize,
                                                                TZrChar *errorBuffer,
                                                                TZrSize errorBufferSize) {
    TZrChar explicitNormalized[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar derivedFromPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool hasExplicit = explicitModuleKey != ZR_NULL && explicitModuleKey[0] != '\0';
    TZrBool hasDerived = ZR_FALSE;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }

    if (hasExplicit &&
        !ZrLibrary_Project_NormalizeModuleKey(explicitModuleKey, explicitNormalized, sizeof(explicitNormalized))) {
        project_resolver_set_error(errorBuffer, errorBufferSize, "invalid explicit module key '%s'", explicitModuleKey);
        return ZR_FALSE;
    }

    if (project != ZR_NULL && sourceName != ZR_NULL && sourceName[0] != '\0') {
        TZrChar sourceRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrChar normalizedSourceName[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrChar relativeSourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];

        if (project_resolver_build_source_root(project, sourceRoot, sizeof(sourceRoot)) &&
            ZrLibrary_File_NormalizePath(sourceName, normalizedSourceName, sizeof(normalizedSourceName)) &&
            project_resolver_relative_path_from_root(sourceRoot,
                                                    normalizedSourceName,
                                                    relativeSourcePath,
                                                    sizeof(relativeSourcePath))) {
            if (relativeSourcePath[0] == '\0' ||
                !ZrLibrary_Project_NormalizeModuleKey(relativeSourcePath,
                                                      derivedFromPath,
                                                      sizeof(derivedFromPath))) {
                project_resolver_set_error(errorBuffer,
                                           errorBufferSize,
                                           "failed to derive a project module key from '%s'",
                                           sourceName);
                return ZR_FALSE;
            }
            hasDerived = ZR_TRUE;
        } else if (project_resolver_is_absolute_path(sourceName)) {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "source '%s' is outside the project source root",
                                       sourceName);
            return ZR_FALSE;
        }
    }

    if (!hasDerived && sourceName != ZR_NULL && sourceName[0] != '\0') {
        if (!ZrLibrary_Project_NormalizeModuleKey(sourceName, derivedFromPath, sizeof(derivedFromPath))) {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "failed to derive a current module key from '%s'",
                                       sourceName);
            return ZR_FALSE;
        }
        hasDerived = ZR_TRUE;
    }

    if (hasExplicit && hasDerived && strcmp(explicitNormalized, derivedFromPath) != 0) {
        project_resolver_set_error(errorBuffer,
                                   errorBufferSize,
                                   "explicit module key '%s' does not match project path '%s'",
                                   explicitNormalized,
                                   derivedFromPath);
        return ZR_FALSE;
    }

    if (hasExplicit) {
        return project_resolver_copy_text(explicitNormalized, buffer, bufferSize);
    }

    if (!hasDerived) {
        project_resolver_set_error(errorBuffer,
                                   errorBufferSize,
                                   "unable to derive a current module key");
        return ZR_FALSE;
    }

    return project_resolver_copy_text(derivedFromPath, buffer, bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportModuleKey(const SZrLibrary_Project *project,
                                                                const TZrChar *currentModuleKey,
                                                                const TZrChar *rawSpecifier,
                                                                TZrChar *buffer,
                                                                TZrSize bufferSize,
                                                                TZrChar *errorBuffer,
                                                                TZrSize errorBufferSize) {
    TZrChar normalized[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }

    if (rawSpecifier == ZR_NULL || rawSpecifier[0] == '\0') {
        project_resolver_set_error(errorBuffer, errorBufferSize, "import specifier cannot be empty");
        return ZR_FALSE;
    }

    if (rawSpecifier[0] == '@') {
        const TZrChar *suffix = rawSpecifier + 1;
        TZrSize aliasLength = 0;
        const SZrLibrary_ProjectPathAlias *matchedAlias = ZR_NULL;

        while (suffix[aliasLength] != '\0' && suffix[aliasLength] != '.' &&
               suffix[aliasLength] != '/' && suffix[aliasLength] != '\\') {
            aliasLength++;
        }

        if (aliasLength == 0) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "invalid alias import '%s'", rawSpecifier);
            return ZR_FALSE;
        }
        if (suffix[aliasLength] == '/' || suffix[aliasLength] == '\\') {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "alias imports must use dotted suffixes: '%s'",
                                       rawSpecifier);
            return ZR_FALSE;
        }

        if (project != ZR_NULL && project->pathAliases != ZR_NULL) {
            for (TZrSize index = 0; index < project->pathAliasCount; index++) {
                const TZrChar *aliasText = project_resolver_string_text(project->pathAliases[index].alias);
                if (aliasText != ZR_NULL &&
                    aliasText[0] == '@' &&
                    strlen(aliasText) == aliasLength + 1 &&
                    strncmp(aliasText + 1, suffix, aliasLength) == 0) {
                    matchedAlias = &project->pathAliases[index];
                    break;
                }
            }
        }

        if (matchedAlias == ZR_NULL) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "unknown import alias '%.*s'", (int)(aliasLength + 1), rawSpecifier);
            return ZR_FALSE;
        }

        if (suffix[aliasLength] == '\0') {
            return project_resolver_copy_text(project_resolver_string_text(matchedAlias->modulePrefix),
                                              buffer,
                                              bufferSize);
        }

        if (suffix[aliasLength + 1] == '\0' ||
            !project_resolver_convert_dot_suffix(suffix + aliasLength + 1, normalized, sizeof(normalized)) ||
            !project_resolver_join_module_paths(project_resolver_string_text(matchedAlias->modulePrefix),
                                               normalized,
                                               buffer,
                                               bufferSize)) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "invalid alias import '%s'", rawSpecifier);
            return ZR_FALSE;
        }

        return ZR_TRUE;
    }

    if (rawSpecifier[0] == '.') {
        TZrSize dotCount = 0;
        TZrSize parentLevels;

        while (rawSpecifier[dotCount] == '.') {
            dotCount++;
        }

        if (rawSpecifier[dotCount] == '\0') {
            project_resolver_set_error(errorBuffer, errorBufferSize, "relative import '%s' is incomplete", rawSpecifier);
            return ZR_FALSE;
        }
        if (rawSpecifier[dotCount] == '/' || rawSpecifier[dotCount] == '\\') {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "relative imports must use leading dots, not filesystem syntax: '%s'",
                                       rawSpecifier);
            return ZR_FALSE;
        }
        if (!project_resolver_convert_dot_suffix(rawSpecifier + dotCount, normalized, sizeof(normalized))) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "invalid relative import '%s'", rawSpecifier);
            return ZR_FALSE;
        }

        parentLevels = dotCount > 0 ? dotCount - 1 : 0;
        if (!project_resolver_apply_relative_import(currentModuleKey,
                                                    parentLevels,
                                                    normalized,
                                                    buffer,
                                                    bufferSize)) {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "relative import '%s' escapes the project source root",
                                       rawSpecifier);
            return ZR_FALSE;
        }

        return ZR_TRUE;
    }

    if (!ZrLibrary_Project_NormalizeModuleKey(rawSpecifier, buffer, bufferSize)) {
        project_resolver_set_error(errorBuffer, errorBufferSize, "invalid import specifier '%s'", rawSpecifier);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
