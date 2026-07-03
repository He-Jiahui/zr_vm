#include "zr_vm_library/project.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void project_import_provider_location_set_error(TZrChar *errorBuffer,
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

static const TZrChar *project_import_provider_dynamic_library_extension(void) {
#if defined(ZR_PLATFORM_WIN)
    return ".dll";
#elif defined(ZR_PLATFORM_DARWIN)
    return ".dylib";
#else
    return ".so";
#endif
}

static const TZrChar *project_import_provider_backend_directory(EZrAotBackendKind backendKind) {
    switch (backendKind) {
        case ZR_AOT_BACKEND_KIND_C:
            return "aot_c";
        case ZR_AOT_BACKEND_KIND_LLVM:
            return "aot_llvm";
        default:
            return ZR_NULL;
    }
}

static TZrBool project_import_provider_copy_descriptor_module_name(const TZrChar *resolvedModuleKey,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize) {
    const TZrChar *moduleName;
    const TZrChar *versionCursor;

    if (resolvedModuleKey == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    if (resolvedModuleKey[0] != '$') {
        return snprintf(buffer, bufferSize, "%s", resolvedModuleKey) < (int)bufferSize;
    }

    versionCursor = strchr(resolvedModuleKey, '@');
    if (versionCursor == ZR_NULL) {
        return ZR_FALSE;
    }
    moduleName = strchr(versionCursor + 1, '/');
    if (moduleName == ZR_NULL || moduleName[1] == '\0') {
        return ZR_FALSE;
    }

    return snprintf(buffer, bufferSize, "%s", moduleName + 1) < (int)bufferSize;
}

static void project_import_provider_sanitize_module_name(const TZrChar *moduleName,
                                                         TZrChar *buffer,
                                                         TZrSize bufferSize) {
    TZrSize cursor = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (moduleName == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; moduleName[index] != '\0' && cursor + 1 < bufferSize; index++) {
        TZrChar current = moduleName[index];
        buffer[cursor++] = (TZrChar)(((current >= 'a' && current <= 'z') ||
                                      (current >= 'A' && current <= 'Z') ||
                                      (current >= '0' && current <= '9'))
                                             ? current
                                             : '_');
    }
    buffer[cursor] = '\0';
}

static TZrBool project_import_provider_path_ends_with_module_file(const TZrChar *path,
                                                                  const TZrChar *moduleName,
                                                                  const TZrChar *extension,
                                                                  TZrSize *outSuffixLength) {
    TZrSize pathLength;
    TZrSize moduleLength;
    TZrSize extensionLength;
    TZrSize suffixLength;
    const TZrChar *pathSuffix;

    if (outSuffixLength != ZR_NULL) {
        *outSuffixLength = 0;
    }
    if (path == ZR_NULL || moduleName == ZR_NULL || extension == ZR_NULL) {
        return ZR_FALSE;
    }

    pathLength = strlen(path);
    moduleLength = strlen(moduleName);
    extensionLength = strlen(extension);
    suffixLength = moduleLength + extensionLength;
    if (pathLength <= suffixLength) {
        return ZR_FALSE;
    }

    pathSuffix = path + pathLength - suffixLength;
    for (TZrSize index = 0; index < moduleLength; index++) {
        TZrChar pathChar = pathSuffix[index];
        TZrChar moduleChar = moduleName[index];
        if (pathChar == '\\') {
            pathChar = '/';
        }
        if (moduleChar == '\\') {
            moduleChar = '/';
        }
        if (pathChar != moduleChar) {
            return ZR_FALSE;
        }
    }
    if (memcmp(pathSuffix + moduleLength, extension, extensionLength) != 0) {
        return ZR_FALSE;
    }
    if (path[pathLength - suffixLength - 1u] != '/' && path[pathLength - suffixLength - 1u] != '\\') {
        return ZR_FALSE;
    }

    if (outSuffixLength != ZR_NULL) {
        *outSuffixLength = suffixLength;
    }
    return ZR_TRUE;
}

static TZrBool project_import_provider_copy_binary_root(const TZrChar *binaryPath,
                                                        const TZrChar *descriptorModuleName,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize) {
    TZrSize pathLength;
    TZrSize suffixLength;
    TZrSize rootLength;

    if (binaryPath == ZR_NULL || descriptorModuleName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0 ||
        !project_import_provider_path_ends_with_module_file(binaryPath,
                                                            descriptorModuleName,
                                                            ZR_VM_BINARY_MODULE_FILE_EXTENSION,
                                                            &suffixLength)) {
        return ZR_FALSE;
    }

    pathLength = strlen(binaryPath);
    rootLength = pathLength - suffixLength - 1u;
    if (rootLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, binaryPath, rootLength);
    buffer[rootLength] = '\0';
    return rootLength > 0;
}

static TZrBool project_import_provider_build_library_path(const TZrChar *binaryPath,
                                                          const TZrChar *descriptorModuleName,
                                                          EZrAotBackendKind backendKind,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sanitizedModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *backendDirectory;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';

    backendDirectory = project_import_provider_backend_directory(backendKind);
    if (backendDirectory == ZR_NULL ||
        !project_import_provider_copy_binary_root(binaryPath,
                                                  descriptorModuleName,
                                                  binaryRoot,
                                                  sizeof(binaryRoot))) {
        return ZR_FALSE;
    }

    project_import_provider_sanitize_module_name(descriptorModuleName,
                                                 sanitizedModuleName,
                                                 sizeof(sanitizedModuleName));
    if (sanitizedModuleName[0] == '\0') {
        return ZR_FALSE;
    }

    return snprintf(buffer,
                    bufferSize,
                    "%s%c%s%c%s%c%s%s%s",
                    binaryRoot,
                    ZR_SEPARATOR,
                    backendDirectory,
                    ZR_SEPARATOR,
                    "lib",
                    ZR_SEPARATOR,
                    "zrvm_aot_",
                    sanitizedModuleName,
                    project_import_provider_dynamic_library_extension()) < (int)bufferSize;
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportProviderLocation(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *rawSpecifier,
        TZrChar *resolvedModuleKey,
        TZrSize resolvedModuleKeySize,
        SZrLibrary_ProjectImportProviderLocation *outLocation,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    TZrChar localModuleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *moduleKeyBuffer;
    TZrSize moduleKeyBufferSize;

    if (outLocation == ZR_NULL) {
        project_import_provider_location_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "import provider location output cannot be null");
        return ZR_FALSE;
    }

    memset(outLocation, 0, sizeof(*outLocation));
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }

    if (resolvedModuleKey != ZR_NULL) {
        if (resolvedModuleKeySize == 0) {
            project_import_provider_location_set_error(errorBuffer,
                                                       errorBufferSize,
                                                       "resolved provider module key buffer cannot be empty");
            return ZR_FALSE;
        }
        resolvedModuleKey[0] = '\0';
        moduleKeyBuffer = resolvedModuleKey;
        moduleKeyBufferSize = resolvedModuleKeySize;
    } else {
        moduleKeyBuffer = localModuleKey;
        moduleKeyBufferSize = sizeof(localModuleKey);
    }

    if (!ZrLibrary_Project_ResolveImportModuleKey(project,
                                                 currentModuleKey,
                                                 rawSpecifier,
                                                 moduleKeyBuffer,
                                                 moduleKeyBufferSize,
                                                 errorBuffer,
                                                 errorBufferSize)) {
        return ZR_FALSE;
    }

    if (!ZrLibrary_Project_GetDependencyImportVersionRange(project,
                                                          currentModuleKey,
                                                          moduleKeyBuffer,
                                                          &outLocation->assemblyName,
                                                          &outLocation->requestedVersion,
                                                          &outLocation->minVersionInclusive,
                                                          &outLocation->maxVersionExclusive)) {
        project_import_provider_location_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "import '%s' does not resolve to a dependency provider",
                                                   rawSpecifier != ZR_NULL ? rawSpecifier : "");
        return ZR_FALSE;
    }

    if (ZrLibrary_Project_ResolveZrmModuleEntry(project,
                                               moduleKeyBuffer,
                                               &outLocation->archive,
                                               &outLocation->entry)) {
        outLocation->artifactKind = ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM;
        return ZR_TRUE;
    }

    {
        TZrBool hasSourcePath = ZrLibrary_Project_ResolveSourcePath(project,
                                                                    moduleKeyBuffer,
                                                                    outLocation->sourcePath,
                                                                    sizeof(outLocation->sourcePath));
        TZrBool hasBinaryPath = ZrLibrary_Project_ResolveBinaryPath(project,
                                                                    moduleKeyBuffer,
                                                                    outLocation->binaryPath,
                                                                    sizeof(outLocation->binaryPath));
        TZrBool hasIntermediatePath = ZrLibrary_Project_ResolveIntermediatePath(project,
                                                                               moduleKeyBuffer,
                                                                               outLocation->intermediatePath,
                                                                               sizeof(outLocation->intermediatePath));
        if (hasSourcePath || hasBinaryPath || hasIntermediatePath) {
            outLocation->artifactKind = ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_PROJECT;
            return ZR_TRUE;
        }
    }

    project_import_provider_location_set_error(errorBuffer,
                                               errorBufferSize,
                                               "dependency provider module '%s' could not be located",
                                               moduleKeyBuffer);
    return ZR_FALSE;
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportProviderAotLoadRequest(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *rawSpecifier,
        EZrAotBackendKind backendKind,
        SZrLibrary_ProjectImportProviderAotLoadRequest *outRequest,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    SZrLibrary_ProjectImportProviderLocation location;

    if (outRequest == ZR_NULL) {
        project_import_provider_location_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "import provider AOT load request output cannot be null");
        return ZR_FALSE;
    }

    memset(outRequest, 0, sizeof(*outRequest));
    outRequest->backendKind = backendKind;
    if (project_import_provider_backend_directory(backendKind) == ZR_NULL) {
        project_import_provider_location_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "unsupported provider AOT backend kind %u",
                                                   (unsigned)backendKind);
        return ZR_FALSE;
    }

    if (!ZrLibrary_Project_ResolveImportProviderLocation(project,
                                                        currentModuleKey,
                                                        rawSpecifier,
                                                        outRequest->resolvedModuleKey,
                                                        sizeof(outRequest->resolvedModuleKey),
                                                        &location,
                                                        errorBuffer,
                                                        errorBufferSize)) {
        return ZR_FALSE;
    }

    outRequest->artifactKind = location.artifactKind;
    outRequest->assemblyName = location.assemblyName;
    outRequest->requestedVersion = location.requestedVersion;
    outRequest->minVersionInclusive = location.minVersionInclusive;
    outRequest->maxVersionExclusive = location.maxVersionExclusive;
    outRequest->archive = location.archive;
    outRequest->entry = location.entry;
    snprintf(outRequest->sourcePath, sizeof(outRequest->sourcePath), "%s", location.sourcePath);
    snprintf(outRequest->binaryPath, sizeof(outRequest->binaryPath), "%s", location.binaryPath);
    snprintf(outRequest->intermediatePath, sizeof(outRequest->intermediatePath), "%s", location.intermediatePath);

    if (!project_import_provider_copy_descriptor_module_name(outRequest->resolvedModuleKey,
                                                            outRequest->descriptorModuleName,
                                                            sizeof(outRequest->descriptorModuleName))) {
        project_import_provider_location_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "provider module key '%s' does not contain a descriptor module name",
                                                   outRequest->resolvedModuleKey);
        return ZR_FALSE;
    }

    if (outRequest->artifactKind == ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_PROJECT &&
        !project_import_provider_build_library_path(outRequest->binaryPath,
                                                   outRequest->descriptorModuleName,
                                                   backendKind,
                                                   outRequest->libraryPath,
                                                   sizeof(outRequest->libraryPath))) {
        project_import_provider_location_set_error(errorBuffer,
                                                   errorBufferSize,
                                                   "provider AOT library path could not be derived for module '%s'",
                                                   outRequest->resolvedModuleKey);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
