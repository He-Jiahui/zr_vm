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

static TZrBool project_resolver_parse_dependency_module_key(const TZrChar *moduleKey,
                                                            TZrChar *nameBuffer,
                                                            TZrSize nameBufferSize,
                                                            TZrChar *versionBuffer,
                                                            TZrSize versionBufferSize,
                                                            const TZrChar **outModulePath) {
    TZrSize nameStart = 1;
    TZrSize nameEnd;
    TZrSize versionStart;
    TZrSize versionEnd;
    TZrSize nameLength;
    TZrSize versionLength;

    if (outModulePath != ZR_NULL) {
        *outModulePath = ZR_NULL;
    }
    if (moduleKey == ZR_NULL || moduleKey[0] != '$' || nameBuffer == ZR_NULL || nameBufferSize == 0 ||
        versionBuffer == ZR_NULL || versionBufferSize == 0) {
        return ZR_FALSE;
    }

    nameEnd = nameStart;
    while (moduleKey[nameEnd] != '\0' && moduleKey[nameEnd] != '@') {
        if (moduleKey[nameEnd] == '/' || moduleKey[nameEnd] == '\\') {
            return ZR_FALSE;
        }
        nameEnd++;
    }
    if (moduleKey[nameEnd] != '@' || nameEnd == nameStart) {
        return ZR_FALSE;
    }

    versionStart = nameEnd + 1;
    versionEnd = versionStart;
    while (moduleKey[versionEnd] != '\0' && moduleKey[versionEnd] != '/') {
        if (moduleKey[versionEnd] == '\\') {
            return ZR_FALSE;
        }
        versionEnd++;
    }
    if (versionEnd == versionStart) {
        return ZR_FALSE;
    }

    nameLength = nameEnd - nameStart;
    versionLength = versionEnd - versionStart;
    if (nameLength + 1 > nameBufferSize || versionLength + 1 > versionBufferSize) {
        return ZR_FALSE;
    }

    memcpy(nameBuffer, moduleKey + nameStart, nameLength);
    nameBuffer[nameLength] = '\0';
    memcpy(versionBuffer, moduleKey + versionStart, versionLength);
    versionBuffer[versionLength] = '\0';
    if (outModulePath != ZR_NULL) {
        *outModulePath = moduleKey[versionEnd] == '/' ? moduleKey + versionEnd + 1 : "";
    }
    return ZR_TRUE;
}

static const SZrLibrary_ProjectDependencyPackage *project_resolver_find_dependency_package(
        const SZrLibrary_Project *project,
        const TZrChar *name,
        const TZrChar *version,
        TZrSize *outIndex) {
    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (project == ZR_NULL || name == ZR_NULL || version == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < project->dependencyPackageCount; index++) {
        const TZrChar *packageName = project_resolver_string_text(project->dependencyPackages[index].name);
        const TZrChar *packageVersion = project_resolver_string_text(project->dependencyPackages[index].version);
        if (packageName != ZR_NULL && packageVersion != ZR_NULL &&
            strcmp(packageName, name) == 0 && strcmp(packageVersion, version) == 0) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return &project->dependencyPackages[index];
        }
    }

    return ZR_NULL;
}

static const SZrLibrary_ProjectDependencyPackage *project_resolver_current_dependency_package(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar **outModulePath) {
    TZrChar name[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar version[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outModulePath != ZR_NULL) {
        *outModulePath = ZR_NULL;
    }
    if (project == ZR_NULL || currentModuleKey == ZR_NULL || currentModuleKey[0] != '$' ||
        !project_resolver_parse_dependency_module_key(currentModuleKey,
                                                      name,
                                                      sizeof(name),
                                                      version,
                                                      sizeof(version),
                                                      outModulePath)) {
        return ZR_NULL;
    }

    return project_resolver_find_dependency_package(project, name, version, ZR_NULL);
}

static const SZrLibrary_ProjectDependencyReference *project_resolver_find_dependency_ref(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectDependencyPackage *ownerPackage,
        const TZrChar *name,
        TZrSize nameLength) {
    const SZrLibrary_ProjectDependencyReference *refs;
    TZrSize refCount;

    if (project == ZR_NULL || name == ZR_NULL || nameLength == 0) {
        return ZR_NULL;
    }

    if (ownerPackage != ZR_NULL) {
        refs = ownerPackage->dependencyRefs;
        refCount = ownerPackage->dependencyRefCount;
    } else {
        refs = project->dependencyRefs;
        refCount = project->dependencyRefCount;
    }

    for (TZrSize index = 0; index < refCount; index++) {
        const TZrChar *refName = project_resolver_string_text(refs[index].name);
        if (refName != ZR_NULL && strlen(refName) == nameLength && strncmp(refName, name, nameLength) == 0) {
            return &refs[index];
        }
    }

    return ZR_NULL;
}

static const SZrLibrary_ProjectDependencyReference *project_resolver_find_dependency_ref_by_package_index(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectDependencyPackage *ownerPackage,
        TZrSize packageIndex) {
    const SZrLibrary_ProjectDependencyReference *refs;
    TZrSize refCount;

    if (project == ZR_NULL || packageIndex >= project->dependencyPackageCount) {
        return ZR_NULL;
    }

    if (ownerPackage != ZR_NULL) {
        refs = ownerPackage->dependencyRefs;
        refCount = ownerPackage->dependencyRefCount;
    } else {
        refs = project->dependencyRefs;
        refCount = project->dependencyRefCount;
    }

    for (TZrSize index = 0; index < refCount; index++) {
        if (refs[index].packageIndex == packageIndex) {
            return &refs[index];
        }
    }

    return ZR_NULL;
}

static TZrBool project_resolver_dependency_package_key(const SZrLibrary_ProjectDependencyPackage *package,
                                                       const TZrChar *modulePath,
                                                       TZrChar *buffer,
                                                       TZrSize bufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *name;
    const TZrChar *version;
    const TZrChar *effectiveModule;
    TZrSize nameLength;
    TZrSize versionLength;
    TZrSize moduleLength;
    TZrSize totalLength;

    if (package == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    name = project_resolver_string_text(package->name);
    version = project_resolver_string_text(package->version);
    effectiveModule = modulePath != ZR_NULL && modulePath[0] != '\0'
                    ? modulePath
                    : project_resolver_string_text(package->entry);
    if (name == ZR_NULL || version == ZR_NULL || effectiveModule == ZR_NULL ||
        !ZrLibrary_Project_NormalizeModuleKey(effectiveModule, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    nameLength = strlen(name);
    versionLength = strlen(version);
    moduleLength = strlen(normalizedModule);
    totalLength = 1 + nameLength + 1 + versionLength + 1 + moduleLength;
    if (totalLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    buffer[0] = '$';
    memcpy(buffer + 1, name, nameLength);
    buffer[1 + nameLength] = '@';
    memcpy(buffer + 1 + nameLength + 1, version, versionLength);
    buffer[1 + nameLength + 1 + versionLength] = '/';
    memcpy(buffer + 1 + nameLength + 1 + versionLength + 1, normalizedModule, moduleLength);
    buffer[totalLength] = '\0';
    return ZR_TRUE;
}

static TZrBool project_resolver_build_package_source_root(const SZrLibrary_ProjectDependencyPackage *package,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    TZrChar joinedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *directory;
    const TZrChar *sourceRoot;

    if (package == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    directory = project_resolver_string_text(package->directory);
    sourceRoot = project_resolver_string_text(package->source);
    if (directory == ZR_NULL || sourceRoot == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(directory, sourceRoot, joinedPath);
    return ZrLibrary_File_NormalizePath(joinedPath, buffer, bufferSize);
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
        } else {
            for (TZrSize packageIndex = 0; packageIndex < project->dependencyPackageCount; packageIndex++) {
                const SZrLibrary_ProjectDependencyPackage *package = &project->dependencyPackages[packageIndex];
                if (project_resolver_build_package_source_root(package, sourceRoot, sizeof(sourceRoot)) &&
                    ZrLibrary_File_NormalizePath(sourceName, normalizedSourceName, sizeof(normalizedSourceName)) &&
                    project_resolver_relative_path_from_root(sourceRoot,
                                                            normalizedSourceName,
                                                            relativeSourcePath,
                                                            sizeof(relativeSourcePath))) {
                    if (relativeSourcePath[0] == '\0' ||
                        !project_resolver_dependency_package_key(package,
                                                                 relativeSourcePath,
                                                                 derivedFromPath,
                                                                 sizeof(derivedFromPath))) {
                        project_resolver_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "failed to derive a dependency module key from '%s'",
                                                   sourceName);
                        return ZR_FALSE;
                    }
                    hasDerived = ZR_TRUE;
                    break;
                }
            }
        }

        if (!hasDerived && project_resolver_is_absolute_path(sourceName)) {
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
    const TZrChar *currentDependencyModulePath = ZR_NULL;
    const SZrLibrary_ProjectDependencyPackage *currentDependencyPackage;

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

    currentDependencyPackage = project_resolver_current_dependency_package(project,
                                                                          currentModuleKey,
                                                                          &currentDependencyModulePath);

    if (rawSpecifier[0] == '&') {
        const TZrChar *suffix = rawSpecifier + 1;
        TZrSize dependencyNameLength = 0;
        const SZrLibrary_ProjectDependencyReference *dependencyRef;
        const SZrLibrary_ProjectDependencyPackage *dependencyPackage;
        const TZrChar *moduleSuffix;

        while (suffix[dependencyNameLength] != '\0' && suffix[dependencyNameLength] != '.' &&
               suffix[dependencyNameLength] != '/' && suffix[dependencyNameLength] != '\\') {
            dependencyNameLength++;
        }

        if (dependencyNameLength == 0) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "invalid dependency import '%s'", rawSpecifier);
            return ZR_FALSE;
        }
        if (suffix[dependencyNameLength] == '/' || suffix[dependencyNameLength] == '\\') {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "dependency imports must use dotted suffixes: '%s'",
                                       rawSpecifier);
            return ZR_FALSE;
        }

        dependencyRef = project_resolver_find_dependency_ref(project,
                                                            currentDependencyPackage,
                                                            suffix,
                                                            dependencyNameLength);
        if (dependencyRef == ZR_NULL || project == ZR_NULL || dependencyRef->packageIndex >= project->dependencyPackageCount) {
            project_resolver_set_error(errorBuffer,
                                       errorBufferSize,
                                       "unknown dependency import '&%.*s'",
                                       (int)dependencyNameLength,
                                       suffix);
            return ZR_FALSE;
        }

        dependencyPackage = &project->dependencyPackages[dependencyRef->packageIndex];
        if (suffix[dependencyNameLength] == '\0') {
            return project_resolver_dependency_package_key(dependencyPackage, ZR_NULL, buffer, bufferSize);
        }

        moduleSuffix = suffix + dependencyNameLength + 1;
        if (moduleSuffix[0] == '\0' ||
            !project_resolver_convert_dot_suffix(moduleSuffix, normalized, sizeof(normalized)) ||
            !project_resolver_dependency_package_key(dependencyPackage, normalized, buffer, bufferSize)) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "invalid dependency import '%s'", rawSpecifier);
            return ZR_FALSE;
        }

        return ZR_TRUE;
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

        if (currentDependencyPackage != ZR_NULL && currentDependencyPackage->pathAliases != ZR_NULL) {
            for (TZrSize index = 0; index < currentDependencyPackage->pathAliasCount; index++) {
                const TZrChar *aliasText = project_resolver_string_text(currentDependencyPackage->pathAliases[index].alias);
                if (aliasText != ZR_NULL &&
                    aliasText[0] == '@' &&
                    strlen(aliasText) == aliasLength + 1 &&
                    strncmp(aliasText + 1, suffix, aliasLength) == 0) {
                    matchedAlias = &currentDependencyPackage->pathAliases[index];
                    break;
                }
            }
        } else if (project != ZR_NULL && project->pathAliases != ZR_NULL) {
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
            if (currentDependencyPackage != ZR_NULL) {
                return project_resolver_dependency_package_key(currentDependencyPackage,
                                                               project_resolver_string_text(matchedAlias->modulePrefix),
                                                               buffer,
                                                               bufferSize);
            }
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

        if (currentDependencyPackage != ZR_NULL) {
            TZrChar aliasResolved[ZR_LIBRARY_MAX_PATH_LENGTH];
            if (!project_resolver_copy_text(buffer, aliasResolved, sizeof(aliasResolved)) ||
                !project_resolver_dependency_package_key(currentDependencyPackage,
                                                         aliasResolved,
                                                         buffer,
                                                         bufferSize)) {
                project_resolver_set_error(errorBuffer, errorBufferSize, "invalid alias import '%s'", rawSpecifier);
                return ZR_FALSE;
            }
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
        if (!project_resolver_apply_relative_import(currentDependencyPackage != ZR_NULL
                                                            ? currentDependencyModulePath
                                                            : currentModuleKey,
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

        if (currentDependencyPackage != ZR_NULL) {
            TZrChar relativeResolved[ZR_LIBRARY_MAX_PATH_LENGTH];
            if (!project_resolver_copy_text(buffer, relativeResolved, sizeof(relativeResolved)) ||
                !project_resolver_dependency_package_key(currentDependencyPackage,
                                                         relativeResolved,
                                                         buffer,
                                                         bufferSize)) {
                project_resolver_set_error(errorBuffer, errorBufferSize, "invalid relative import '%s'", rawSpecifier);
                return ZR_FALSE;
            }
        }

        return ZR_TRUE;
    }

    if (currentDependencyPackage != ZR_NULL && strncmp(rawSpecifier, "zr.", 3) != 0 && strcmp(rawSpecifier, "zr") != 0) {
        if (!((strchr(rawSpecifier, '/') != ZR_NULL || strchr(rawSpecifier, '\\') != ZR_NULL)
                      ? ZrLibrary_Project_NormalizeModuleKey(rawSpecifier, normalized, sizeof(normalized))
                      : project_resolver_convert_dot_suffix(rawSpecifier, normalized, sizeof(normalized))) ||
            !project_resolver_dependency_package_key(currentDependencyPackage, normalized, buffer, bufferSize)) {
            project_resolver_set_error(errorBuffer, errorBufferSize, "invalid import specifier '%s'", rawSpecifier);
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

ZR_LIBRARY_API TZrBool ZrLibrary_Project_GetDependencyImportVersionRange(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *resolvedModuleKey,
        SZrString **outAssemblyName,
        SZrString **outRequestedVersion,
        SZrString **outMinVersionInclusive,
        SZrString **outMaxVersionExclusive) {
    TZrChar targetName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar targetVersion[ZR_LIBRARY_MAX_PATH_LENGTH];
    const SZrLibrary_ProjectDependencyPackage *ownerPackage;
    const SZrLibrary_ProjectDependencyPackage *targetPackage;
    const SZrLibrary_ProjectDependencyReference *dependencyRef;
    TZrSize targetPackageIndex = 0;

    if (outAssemblyName != ZR_NULL) {
        *outAssemblyName = ZR_NULL;
    }
    if (outRequestedVersion != ZR_NULL) {
        *outRequestedVersion = ZR_NULL;
    }
    if (outMinVersionInclusive != ZR_NULL) {
        *outMinVersionInclusive = ZR_NULL;
    }
    if (outMaxVersionExclusive != ZR_NULL) {
        *outMaxVersionExclusive = ZR_NULL;
    }
    if (project == ZR_NULL || resolvedModuleKey == ZR_NULL ||
        outAssemblyName == ZR_NULL || outRequestedVersion == ZR_NULL || outMinVersionInclusive == ZR_NULL ||
        outMaxVersionExclusive == ZR_NULL ||
        !project_resolver_parse_dependency_module_key(resolvedModuleKey,
                                                      targetName,
                                                      sizeof(targetName),
                                                      targetVersion,
                                                      sizeof(targetVersion),
                                                      ZR_NULL)) {
        return ZR_FALSE;
    }

    ownerPackage = project_resolver_current_dependency_package(project, currentModuleKey, ZR_NULL);
    targetPackage = project_resolver_find_dependency_package(project,
                                                             targetName,
                                                             targetVersion,
                                                             &targetPackageIndex);
    if (targetPackage == ZR_NULL) {
        return ZR_FALSE;
    }

    dependencyRef = project_resolver_find_dependency_ref_by_package_index(project,
                                                                         ownerPackage,
                                                                         targetPackageIndex);
    if (dependencyRef == ZR_NULL) {
        return ZR_FALSE;
    }

    if (dependencyRef->assemblyName != ZR_NULL) {
        *outAssemblyName = dependencyRef->assemblyName;
    } else if (dependencyRef->useAliasForModuleKey && targetPackage->assemblyName != ZR_NULL) {
        *outAssemblyName = targetPackage->assemblyName;
    }
    *outRequestedVersion = targetPackage->version;
    *outMinVersionInclusive = dependencyRef->minVersionInclusive;
    *outMaxVersionExclusive = dependencyRef->maxVersionExclusive;
    return ZR_TRUE;
}

static TZrBool project_resolver_module_file_from_root(const TZrChar *rootDirectory,
                                                      const TZrChar *modulePath,
                                                      const TZrChar *extension,
                                                      TZrChar *buffer,
                                                      TZrSize bufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar moduleFile[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize normalizedLength;
    TZrSize extensionLength;
    TZrSize totalLength;

    if (rootDirectory == ZR_NULL || modulePath == ZR_NULL || extension == ZR_NULL ||
        buffer == ZR_NULL || bufferSize == 0 ||
        !ZrLibrary_Project_NormalizeModuleKey(modulePath, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    normalizedLength = strlen(normalizedModule);
    extensionLength = strlen(extension);
    totalLength = normalizedLength + extensionLength;
    if (totalLength + 1 > sizeof(moduleFile)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < normalizedLength; index++) {
        moduleFile[index] = normalizedModule[index] == '/' ? ZR_SEPARATOR : normalizedModule[index];
    }
    memcpy(moduleFile + normalizedLength, extension, extensionLength);
    moduleFile[totalLength] = '\0';

    ZrLibrary_File_PathJoin(rootDirectory, moduleFile, buffer);
    return buffer[0] != '\0';
}

static TZrBool project_resolver_build_project_root_path(const SZrLibrary_Project *project,
                                                        SZrString *root,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize) {
    TZrChar joinedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *projectDirectory;
    const TZrChar *rootText;

    if (project == ZR_NULL || root == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    projectDirectory = project_resolver_string_text(project->directory);
    rootText = project_resolver_string_text(root);
    if (projectDirectory == ZR_NULL || rootText == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, rootText, joinedPath);
    return ZrLibrary_File_NormalizePath(joinedPath, buffer, bufferSize);
}

static TZrBool project_resolver_build_package_root_path(const SZrLibrary_ProjectDependencyPackage *package,
                                                        SZrString *root,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize) {
    TZrChar joinedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *packageDirectory;
    const TZrChar *rootText;

    if (package == ZR_NULL || root == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    packageDirectory = project_resolver_string_text(package->directory);
    rootText = project_resolver_string_text(root);
    if (packageDirectory == ZR_NULL || rootText == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(packageDirectory, rootText, joinedPath);
    return ZrLibrary_File_NormalizePath(joinedPath, buffer, bufferSize);
}

static TZrBool project_resolver_resolve_project_relative_path(const SZrLibrary_Project *project,
                                                              const TZrChar *relativePath,
                                                              TZrChar *buffer,
                                                              TZrSize bufferSize) {
    TZrChar joinedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *projectDirectory;

    if (project == ZR_NULL || relativePath == ZR_NULL || relativePath[0] == '\0' ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (ZrLibrary_File_IsAbsolutePath((TZrNativeString)relativePath)) {
        return ZrLibrary_File_NormalizePath((TZrNativeString)relativePath, buffer, bufferSize);
    }

    projectDirectory = project_resolver_string_text(project->directory);
    if (projectDirectory == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(projectDirectory, relativePath, joinedPath);
    return joinedPath[0] != '\0' && ZrLibrary_File_NormalizePath(joinedPath, buffer, bufferSize);
}

static TZrBool project_resolver_resolve_module_path(const SZrLibrary_Project *project,
                                                    const TZrChar *moduleName,
                                                    const TZrChar *extension,
                                                    TZrBool useBinaryRoot,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize) {
    TZrChar dependencyName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar dependencyVersion[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar rootPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *dependencyModulePath;
    const SZrLibrary_ProjectDependencyPackage *package;

    if (project == ZR_NULL || moduleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';

    if (moduleName[0] == '$') {
        if (!project_resolver_parse_dependency_module_key(moduleName,
                                                          dependencyName,
                                                          sizeof(dependencyName),
                                                          dependencyVersion,
                                                          sizeof(dependencyVersion),
                                                          &dependencyModulePath)) {
            return ZR_FALSE;
        }
        package = project_resolver_find_dependency_package(project, dependencyName, dependencyVersion, ZR_NULL);
        if (package == ZR_NULL || package->artifactKind == ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM ||
            dependencyModulePath == ZR_NULL || dependencyModulePath[0] == '\0' ||
            !project_resolver_build_package_root_path(package,
                                                      useBinaryRoot ? package->binary : package->source,
                                                      rootPath,
                                                      sizeof(rootPath))) {
            return ZR_FALSE;
        }

        return project_resolver_module_file_from_root(rootPath,
                                                      dependencyModulePath,
                                                      extension,
                                                      buffer,
                                                      bufferSize);
    }

    if (!project_resolver_build_project_root_path(project,
                                                  useBinaryRoot ? project->binary : project->source,
                                                  rootPath,
                                                  sizeof(rootPath))) {
        return ZR_FALSE;
    }

    return project_resolver_module_file_from_root(rootPath,
                                                  moduleName,
                                                  extension,
                                                  buffer,
                                                  bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveSourcePath(const SZrLibrary_Project *project,
                                                           const TZrChar *moduleName,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize) {
    return project_resolver_resolve_module_path(project,
                                                moduleName,
                                                ZR_VM_SOURCE_MODULE_FILE_EXTENSION,
                                                ZR_FALSE,
                                                buffer,
                                                bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveBinaryPath(const SZrLibrary_Project *project,
                                                           const TZrChar *moduleName,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize) {
    return project_resolver_resolve_module_path(project,
                                                moduleName,
                                                ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                                ZR_TRUE,
                                                buffer,
                                                bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveIntermediatePath(const SZrLibrary_Project *project,
                                                                 const TZrChar *moduleName,
                                                                 TZrChar *buffer,
                                                                 TZrSize bufferSize) {
    return project_resolver_resolve_module_path(project,
                                                moduleName,
                                                ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION,
                                                ZR_TRUE,
                                                buffer,
                                                bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveAssemblyOutputPath(const SZrLibrary_Project *project,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize) {
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar fileName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar outputPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *assemblyOutput;
    const TZrChar *assemblyName;
    int written;

    if (project == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';

    assemblyOutput = project_resolver_string_text(project->assemblyOutput);
    if (assemblyOutput != ZR_NULL && assemblyOutput[0] != '\0') {
        return project_resolver_resolve_project_relative_path(project, assemblyOutput, buffer, bufferSize);
    }

    assemblyName = project_resolver_string_text(project->assemblyName);
    if (assemblyName == ZR_NULL || assemblyName[0] == '\0') {
        assemblyName = project_resolver_string_text(project->name);
    }
    if (assemblyName == ZR_NULL || assemblyName[0] == '\0' ||
        !project_resolver_build_project_root_path(project, project->binary, binaryRoot, sizeof(binaryRoot))) {
        return ZR_FALSE;
    }

    written = snprintf(fileName, sizeof(fileName), "%s%s", assemblyName, ZR_LIBRARY_ZRM_FILE_EXTENSION);
    if (written < 0 || (TZrSize)written + 1 > sizeof(fileName)) {
        return ZR_FALSE;
    }
    ZrLibrary_File_PathJoin(binaryRoot, fileName, outputPath);
    return outputPath[0] != '\0' && ZrLibrary_File_NormalizePath(outputPath, buffer, bufferSize);
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveZrmModuleEntry(
        const SZrLibrary_Project *project,
        const TZrChar *moduleName,
        const SZrLibrary_ZrmArchive **outArchive,
        const SZrLibrary_ZrmEntryInfo **outEntry) {
    TZrChar dependencyName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar dependencyVersion[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *dependencyModulePath;
    const SZrLibrary_ProjectDependencyPackage *package;
    const SZrLibrary_ZrmEntryInfo *entry;

    if (outArchive != ZR_NULL) {
        *outArchive = ZR_NULL;
    }
    if (outEntry != ZR_NULL) {
        *outEntry = ZR_NULL;
    }
    if (project == ZR_NULL || moduleName == ZR_NULL || outArchive == ZR_NULL || outEntry == ZR_NULL ||
        !project_resolver_parse_dependency_module_key(moduleName,
                                                      dependencyName,
                                                      sizeof(dependencyName),
                                                      dependencyVersion,
                                                      sizeof(dependencyVersion),
                                                      &dependencyModulePath)) {
        return ZR_FALSE;
    }

    package = project_resolver_find_dependency_package(project, dependencyName, dependencyVersion, ZR_NULL);
    if (package == ZR_NULL ||
        package->artifactKind != ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM ||
        !package->zrmArchiveOpen ||
        dependencyModulePath == ZR_NULL ||
        dependencyModulePath[0] == '\0') {
        return ZR_FALSE;
    }

    entry = ZrLibrary_Zrm_FindModule(&package->zrmArchive, dependencyModulePath);
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    *outArchive = &package->zrmArchive;
    *outEntry = entry;
    return ZR_TRUE;
}
