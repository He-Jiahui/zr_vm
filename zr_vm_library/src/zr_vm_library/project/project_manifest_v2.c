#include "project/project_manifest_v2.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

static TZrBool library_project_manifest_v2_has_required_string(cJSON *manifestJson, const TZrChar *fieldName) {
    cJSON *field;

    if (manifestJson == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    field = cJSON_GetObjectItemCaseSensitive(manifestJson, fieldName);
    return cJSON_IsString(field) && field->valuestring != ZR_NULL && field->valuestring[0] != '\0';
}

TZrBool library_project_manifest_validate_version(cJSON *manifestJson, TZrUInt32 *outManifestVersion) {
    cJSON *manifestVersionJson;
    TZrUInt32 manifestVersion;

    if (manifestJson == ZR_NULL || outManifestVersion == ZR_NULL || !cJSON_IsObject(manifestJson)) {
        return ZR_FALSE;
    }

    manifestVersionJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "manifestVersion");
    if (manifestVersionJson == ZR_NULL) {
        *outManifestVersion = 1u;
        return ZR_TRUE;
    }
    if (!cJSON_IsNumber(manifestVersionJson) ||
        (manifestVersionJson->valueint != 1 && manifestVersionJson->valueint != 2) ||
        manifestVersionJson->valuedouble != (double)manifestVersionJson->valueint) {
        return ZR_FALSE;
    }

    manifestVersion = (TZrUInt32)manifestVersionJson->valueint;
    *outManifestVersion = manifestVersion;
    return ZR_TRUE;
}

TZrBool library_project_manifest_v2_validate_base(cJSON *manifestJson) {
    return library_project_manifest_v2_has_required_string(manifestJson, "name") &&
           library_project_manifest_v2_has_required_string(manifestJson, "version") &&
           library_project_manifest_v2_has_required_string(manifestJson, "kind") &&
           library_project_manifest_v2_has_required_string(manifestJson, "source") &&
           library_project_manifest_v2_has_required_string(manifestJson, "binary") &&
           library_project_manifest_v2_has_required_string(manifestJson, "entry");
}

static const TZrChar *library_project_manifest_v2_string_text(const SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

static TZrBool library_project_manifest_v2_parse_specifier(const TZrChar *literal,
                                                            SZrLibrary_ModuleSpecifier *outSpecifier) {
    TZrChar errorBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(errorBuffer, 0, sizeof(errorBuffer));
    return ZrLibrary_ModuleSpecifier_Parse(literal, outSpecifier, errorBuffer, sizeof(errorBuffer));
}

static TZrBool library_project_manifest_v2_parse_package_root(const TZrChar *literal,
                                                               SZrLibrary_ModuleIdentity *outIdentity) {
    SZrLibrary_ModuleSpecifier specifier;

    if (literal == ZR_NULL || outIdentity == ZR_NULL ||
        !library_project_manifest_v2_parse_specifier(literal, &specifier) ||
        specifier.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE ||
        specifier.identity.segments[0] != '\0') {
        return ZR_FALSE;
    }

    *outIdentity = specifier.identity;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_parse_alias_root(const TZrChar *literal) {
    SZrLibrary_ModuleSpecifier specifier;

    return literal != ZR_NULL &&
           library_project_manifest_v2_parse_specifier(literal, &specifier) &&
           specifier.kind == ZR_LIBRARY_MODULE_SPECIFIER_KIND_ALIAS &&
           specifier.identity.segments[0] == '\0';
}

static TZrBool library_project_manifest_v2_alias_target_is_supported(const SZrLibrary_ModuleSpecifier *target) {
    if (target == ZR_NULL ||
        target->kind == ZR_LIBRARY_MODULE_SPECIFIER_KIND_ALIAS ||
        target->kind == ZR_LIBRARY_MODULE_SPECIFIER_KIND_RELATIVE ||
        target->kind == ZR_LIBRARY_MODULE_SPECIFIER_KIND_INVALID) {
        return ZR_FALSE;
    }

    return target->kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE || target->identity.segments[0] == '\0';
}

static TZrBool library_project_manifest_v2_export_key_to_canonical(const TZrChar *rawKey,
                                                                    TZrChar *outKey,
                                                                    TZrSize outKeySize) {
    SZrLibrary_ModuleSpecifier keySpecifier;
    int written;

    if (rawKey == ZR_NULL || outKey == ZR_NULL || outKeySize == 0u) {
        return ZR_FALSE;
    }
    if (strcmp(rawKey, ".") == 0) {
        if (outKeySize < 2u) {
            return ZR_FALSE;
        }
        outKey[0] = '.';
        outKey[1] = '\0';
        return ZR_TRUE;
    }
    if (strncmp(rawKey, "./", 2u) != 0 || rawKey[2] == '\0' ||
        !library_project_manifest_v2_parse_specifier(rawKey + 2u, &keySpecifier) ||
        keySpecifier.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE) {
        return ZR_FALSE;
    }

    written = snprintf(outKey, outKeySize, "./%s", keySpecifier.identity.segments);
    return written >= 0 && (TZrSize)written < outKeySize;
}

static TZrBool library_project_manifest_v2_parse_aliases(SZrState *state,
                                                          SZrLibrary_Project *project,
                                                          cJSON *manifestJson) {
    cJSON *aliasesJson;
    cJSON *aliasJson;
    TZrSize count = 0u;
    TZrSize index = 0u;

    aliasesJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "aliases");
    if (aliasesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(aliasesJson)) {
        return ZR_FALSE;
    }
    cJSON_ArrayForEach(aliasJson, aliasesJson) {
        count++;
    }
    if (count == 0u) {
        return ZR_TRUE;
    }

    project->manifestAliases = (SZrLibrary_ProjectManifestAlias *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*project->manifestAliases) * count,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->manifestAliases == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->manifestAliases, 0, sizeof(*project->manifestAliases) * count);
    project->manifestAliasCapacity = count;

    cJSON_ArrayForEach(aliasJson, aliasesJson) {
        SZrLibrary_ProjectManifestAlias *alias = &project->manifestAliases[index];
        SZrLibrary_ModuleSpecifier target;
        TZrSize priorIndex;

        if (aliasJson->string == ZR_NULL || !cJSON_IsString(aliasJson) || aliasJson->valuestring == ZR_NULL ||
            !library_project_manifest_v2_parse_alias_root(aliasJson->string) ||
            !library_project_manifest_v2_parse_specifier(aliasJson->valuestring, &target) ||
            !library_project_manifest_v2_alias_target_is_supported(&target)) {
            return ZR_FALSE;
        }
        for (priorIndex = 0u; priorIndex < index; priorIndex++) {
            const TZrChar *priorRoot = library_project_manifest_v2_string_text(project->manifestAliases[priorIndex].root);
            if (priorRoot != ZR_NULL && strcmp(priorRoot, aliasJson->string) == 0) {
                return ZR_FALSE;
            }
        }

        alias->root = ZrCore_String_CreateTryHitCache(state, aliasJson->string);
        if (alias->root == ZR_NULL) {
            return ZR_FALSE;
        }
        alias->target = target;
        index++;
    }
    project->manifestAliasCount = index;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_parse_package(SZrState *state,
                                                          SZrLibrary_Project *project,
                                                          cJSON *manifestJson) {
    cJSON *packageJson;
    cJSON *nameJson;
    cJSON *exportsJson;
    cJSON *exportJson;
    TZrSize count = 0u;
    TZrSize index = 0u;

    packageJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "package");
    if (packageJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(packageJson)) {
        return ZR_FALSE;
    }
    nameJson = cJSON_GetObjectItemCaseSensitive(packageJson, "name");
    exportsJson = cJSON_GetObjectItemCaseSensitive(packageJson, "exports");
    if (!cJSON_IsString(nameJson) || nameJson->valuestring == ZR_NULL ||
        !library_project_manifest_v2_parse_package_root(nameJson->valuestring, &project->packageIdentity) ||
        !cJSON_IsObject(exportsJson)) {
        return ZR_FALSE;
    }
    cJSON_ArrayForEach(exportJson, exportsJson) {
        count++;
    }
    if (count == 0u) {
        return ZR_FALSE;
    }

    project->packageExports = (SZrLibrary_ProjectPackageExport *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*project->packageExports) * count,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->packageExports == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->packageExports, 0, sizeof(*project->packageExports) * count);
    project->packageExportCapacity = count;

    cJSON_ArrayForEach(exportJson, exportsJson) {
        SZrLibrary_ProjectPackageExport *projectExport = &project->packageExports[index];
        SZrLibrary_ModuleSpecifier target;
        TZrChar canonicalKey[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize priorIndex;

        if (exportJson->string == ZR_NULL || !cJSON_IsString(exportJson) || exportJson->valuestring == ZR_NULL ||
            !library_project_manifest_v2_export_key_to_canonical(exportJson->string,
                                                                  canonicalKey,
                                                                  sizeof(canonicalKey)) ||
            !library_project_manifest_v2_parse_specifier(exportJson->valuestring, &target) ||
            target.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE) {
            return ZR_FALSE;
        }
        for (priorIndex = 0u; priorIndex < index; priorIndex++) {
            const TZrChar *priorKey = library_project_manifest_v2_string_text(project->packageExports[priorIndex].key);
            if (priorKey != ZR_NULL && strcmp(priorKey, canonicalKey) == 0) {
                return ZR_FALSE;
            }
        }

        projectExport->key = ZrCore_String_CreateTryHitCache(state, canonicalKey);
        if (projectExport->key == ZR_NULL) {
            return ZR_FALSE;
        }
        projectExport->target = target;
        index++;
    }
    project->packageExportCount = index;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_parse_dependency_source(
        SZrState *state,
        cJSON *dependencyJson,
        SZrLibrary_ProjectManifestDependency *outDependency) {
    cJSON *field;
    cJSON *sourceJson = ZR_NULL;
    EZrLibrary_ProjectManifestDependencySourceKind sourceKind =
            ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
    TZrSize sourceCount = 0u;

    cJSON_ArrayForEach(field, dependencyJson) {
        if (field->string == ZR_NULL ||
            (strcmp(field->string, "version") != 0 &&
             strcmp(field->string, "path") != 0 &&
             strcmp(field->string, "registry") != 0 &&
             strcmp(field->string, "git") != 0)) {
            return ZR_FALSE;
        }
        if (strcmp(field->string, "version") != 0) {
            if (!cJSON_IsString(field) || field->valuestring == ZR_NULL || field->valuestring[0] == '\0') {
                return ZR_FALSE;
            }
            sourceCount++;
            sourceJson = field;
            if (strcmp(field->string, "path") == 0) {
                sourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
            } else if (strcmp(field->string, "registry") == 0) {
                sourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY;
            } else {
                sourceKind = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT;
            }
        }
    }
    field = cJSON_GetObjectItemCaseSensitive(dependencyJson, "version");
    if (!cJSON_IsString(field) || field->valuestring == ZR_NULL || field->valuestring[0] == '\0' ||
        sourceCount != 1u || sourceJson == ZR_NULL) {
        return ZR_FALSE;
    }

    outDependency->versionRequirement = ZrCore_String_CreateTryHitCache(state, field->valuestring);
    outDependency->source = ZrCore_String_CreateTryHitCache(state, sourceJson->valuestring);
    if (outDependency->versionRequirement == ZR_NULL || outDependency->source == ZR_NULL) {
        return ZR_FALSE;
    }
    outDependency->sourceKind = sourceKind;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_parse_dependencies(SZrState *state,
                                                               SZrLibrary_Project *project,
                                                               cJSON *manifestJson) {
    cJSON *dependenciesJson;
    cJSON *dependencyJson;
    TZrSize count = 0u;
    TZrSize index = 0u;

    dependenciesJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "dependencies");
    if (dependenciesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(dependenciesJson)) {
        return ZR_FALSE;
    }
    cJSON_ArrayForEach(dependencyJson, dependenciesJson) {
        count++;
    }
    if (count == 0u) {
        return ZR_TRUE;
    }

    project->manifestDependencies = (SZrLibrary_ProjectManifestDependency *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*project->manifestDependencies) * count,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->manifestDependencies == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->manifestDependencies, 0, sizeof(*project->manifestDependencies) * count);
    project->manifestDependencyCapacity = count;

    cJSON_ArrayForEach(dependencyJson, dependenciesJson) {
        SZrLibrary_ProjectManifestDependency *dependency = &project->manifestDependencies[index];
        TZrSize priorIndex;

        if (dependencyJson->string == ZR_NULL || !cJSON_IsObject(dependencyJson) ||
            !library_project_manifest_v2_parse_package_root(dependencyJson->string, &dependency->packageIdentity) ||
            !library_project_manifest_v2_parse_dependency_source(state, dependencyJson, dependency)) {
            return ZR_FALSE;
        }
        for (priorIndex = 0u; priorIndex < index; priorIndex++) {
            if (strcmp(project->manifestDependencies[priorIndex].packageIdentity.packageName,
                       dependency->packageIdentity.packageName) == 0) {
                return ZR_FALSE;
            }
        }
        index++;
    }
    project->manifestDependencyCount = index;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_declares_package(const SZrLibrary_Project *project,
                                                            const TZrChar *packageName) {
    TZrSize index;

    if (project == ZR_NULL || packageName == ZR_NULL || packageName[0] == '\0') {
        return ZR_FALSE;
    }
    if (project->packageIdentity.domain == ZR_LIBRARY_MODULE_DOMAIN_PACKAGE &&
        strcmp(project->packageIdentity.packageName, packageName) == 0) {
        return ZR_TRUE;
    }
    for (index = 0u; index < project->manifestDependencyCount; index++) {
        if (strcmp(project->manifestDependencies[index].packageIdentity.packageName, packageName) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool library_project_manifest_v2_validate_alias_package_targets(const SZrLibrary_Project *project) {
    TZrSize index;

    if (project == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0u; index < project->manifestAliasCount; index++) {
        const SZrLibrary_ModuleSpecifier *target = &project->manifestAliases[index].target;

        if (target->kind == ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE &&
            !library_project_manifest_v2_declares_package(project, target->identity.packageName)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_append_file_alias_suffix(const TZrChar *baseLocator,
                                                                     const TZrChar *suffix,
                                                                     TZrChar *outLocator,
                                                                     TZrSize outLocatorSize) {
    TZrChar pathSuffix[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *pathTail;
    const TZrChar *extension;
    TZrSize index;
    TZrSize baseLength;
    int written;

    if (baseLocator == ZR_NULL || suffix == ZR_NULL || outLocator == ZR_NULL || outLocatorSize == 0u ||
        baseLocator[0] == '\0' || suffix[0] == '\0') {
        return ZR_FALSE;
    }
    pathTail = strrchr(baseLocator, '/');
    pathTail = pathTail == ZR_NULL ? baseLocator : pathTail + 1u;
    extension = strrchr(pathTail, '.');
    if (extension != ZR_NULL && strcmp(extension, ".zrp") != 0) {
        return ZR_FALSE;
    }
    if (strlen(suffix) + 1u > sizeof(pathSuffix)) {
        return ZR_FALSE;
    }
    strcpy(pathSuffix, suffix);
    for (index = 0u; pathSuffix[index] != '\0'; index++) {
        if (pathSuffix[index] == '.') {
            pathSuffix[index] = '/';
        }
    }

    baseLength = strlen(baseLocator);
    written = snprintf(outLocator,
                       outLocatorSize,
                       "%s%s%s",
                       baseLocator,
                       baseLocator[baseLength - 1u] == '/' ? "" : "/",
                       pathSuffix);
    return written >= 0 && (TZrSize)written < outLocatorSize;
}

void library_project_manifest_v2_free_declarations(SZrGlobalState *global,
                                                    SZrLibrary_Project *project) {
    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }
    if (project->manifestAliases != ZR_NULL && project->manifestAliasCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->manifestAliases,
                                      sizeof(*project->manifestAliases) * project->manifestAliasCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    if (project->packageExports != ZR_NULL && project->packageExportCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->packageExports,
                                      sizeof(*project->packageExports) * project->packageExportCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    if (project->manifestDependencies != ZR_NULL && project->manifestDependencyCapacity > 0u) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->manifestDependencies,
                                      sizeof(*project->manifestDependencies) * project->manifestDependencyCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->manifestAliases = ZR_NULL;
    project->manifestAliasCount = 0u;
    project->manifestAliasCapacity = 0u;
    memset(&project->packageIdentity, 0, sizeof(project->packageIdentity));
    project->packageExports = ZR_NULL;
    project->packageExportCount = 0u;
    project->packageExportCapacity = 0u;
    project->manifestDependencies = ZR_NULL;
    project->manifestDependencyCount = 0u;
    project->manifestDependencyCapacity = 0u;
}

TZrBool library_project_manifest_v2_parse_declarations(SZrState *state,
                                                        SZrLibrary_Project *project,
                                                        cJSON *manifestJson) {
    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || manifestJson == ZR_NULL ||
        project->manifestVersion != 2u || !cJSON_IsObject(manifestJson) ||
        cJSON_GetObjectItemCaseSensitive(manifestJson, "pathAliases") != ZR_NULL ||
        cJSON_GetObjectItemCaseSensitive(manifestJson, "references") != ZR_NULL ||
        cJSON_GetObjectItemCaseSensitive(manifestJson, "dependency") != ZR_NULL ||
        cJSON_GetObjectItemCaseSensitive(manifestJson, "local") != ZR_NULL) {
        return ZR_FALSE;
    }

    if (!library_project_manifest_v2_parse_aliases(state, project, manifestJson) ||
        !library_project_manifest_v2_parse_package(state, project, manifestJson) ||
        !library_project_manifest_v2_parse_dependencies(state, project, manifestJson) ||
        !library_project_manifest_v2_validate_alias_package_targets(project)) {
        library_project_manifest_v2_free_declarations(state->global, project);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveManifestAlias(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleSpecifier *aliasSpecifier,
        SZrLibrary_ModuleSpecifier *outTargetSpecifier) {
    TZrSize index;

    if (outTargetSpecifier != ZR_NULL) {
        memset(outTargetSpecifier, 0, sizeof(*outTargetSpecifier));
    }
    if (project == ZR_NULL || aliasSpecifier == ZR_NULL || outTargetSpecifier == ZR_NULL ||
        aliasSpecifier->kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_ALIAS) {
        return ZR_FALSE;
    }

    for (index = 0u; index < project->manifestAliasCount; index++) {
        const SZrLibrary_ProjectManifestAlias *alias = &project->manifestAliases[index];
        const TZrChar *root = library_project_manifest_v2_string_text(alias->root);

        if (root != ZR_NULL && root[0] == '#' && strcmp(root + 1u, aliasSpecifier->aliasRoot) == 0) {
            *outTargetSpecifier = alias->target;
            if (aliasSpecifier->identity.segments[0] == '\0') {
                return ZR_TRUE;
            }
            switch (outTargetSpecifier->kind) {
            case ZR_LIBRARY_MODULE_SPECIFIER_KIND_OFFICIAL_NATIVE:
            case ZR_LIBRARY_MODULE_SPECIFIER_KIND_REGISTERED_NATIVE:
            case ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE: {
                int written = snprintf(outTargetSpecifier->identity.segments,
                                       sizeof(outTargetSpecifier->identity.segments),
                                       "%s.%s",
                                       alias->target.identity.segments,
                                       aliasSpecifier->identity.segments);

                if (written < 0 || (TZrSize)written >= sizeof(outTargetSpecifier->identity.segments)) {
                    memset(outTargetSpecifier, 0, sizeof(*outTargetSpecifier));
                    return ZR_FALSE;
                }
                return ZR_TRUE;
            }
            case ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE: {
                int written = snprintf(outTargetSpecifier->identity.segments,
                                       sizeof(outTargetSpecifier->identity.segments),
                                       "%s",
                                       aliasSpecifier->identity.segments);

                if (written < 0 || (TZrSize)written >= sizeof(outTargetSpecifier->identity.segments)) {
                    memset(outTargetSpecifier, 0, sizeof(*outTargetSpecifier));
                    return ZR_FALSE;
                }
                return ZR_TRUE;
            }
            case ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE:
                if (library_project_manifest_v2_append_file_alias_suffix(alias->target.locator,
                                                                          aliasSpecifier->identity.segments,
                                                                          outTargetSpecifier->locator,
                                                                          sizeof(outTargetSpecifier->locator))) {
                    return ZR_TRUE;
                }
                memset(outTargetSpecifier, 0, sizeof(*outTargetSpecifier));
                return ZR_FALSE;
            default:
                memset(outTargetSpecifier, 0, sizeof(*outTargetSpecifier));
                return ZR_FALSE;
            }
        }
    }
    return ZR_FALSE;
}

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolvePackageExport(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleSpecifier *packageSpecifier,
        SZrLibrary_ModuleSpecifier *outTargetSpecifier) {
    TZrChar exportKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize index;
    int written;

    if (outTargetSpecifier != ZR_NULL) {
        memset(outTargetSpecifier, 0, sizeof(*outTargetSpecifier));
    }
    if (project == ZR_NULL || packageSpecifier == ZR_NULL || outTargetSpecifier == ZR_NULL ||
        packageSpecifier->kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE ||
        project->packageIdentity.domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE ||
        strcmp(project->packageIdentity.packageName, packageSpecifier->identity.packageName) != 0) {
        return ZR_FALSE;
    }

    if (packageSpecifier->identity.segments[0] == '\0') {
        exportKey[0] = '.';
        exportKey[1] = '\0';
    } else {
        written = snprintf(exportKey, sizeof(exportKey), "./%s", packageSpecifier->identity.segments);
        if (written < 0 || (TZrSize)written >= sizeof(exportKey)) {
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < project->packageExportCount; index++) {
        const SZrLibrary_ProjectPackageExport *projectExport = &project->packageExports[index];
        const TZrChar *key = library_project_manifest_v2_string_text(projectExport->key);

        if (key != ZR_NULL && strcmp(key, exportKey) == 0) {
            *outTargetSpecifier = projectExport->target;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool library_project_manifest_v2_has_nonempty_string(const SZrString *value) {
    const TZrChar *text = library_project_manifest_v2_string_text(value);

    return text != ZR_NULL && text[0] != '\0';
}

static TZrBool library_project_manifest_v2_path_source_is_portable(const TZrChar *source) {
    if (source == ZR_NULL || source[0] == '\0') {
        return ZR_FALSE;
    }
    if (source[0] == '/' || source[0] == '\\' || strncmp(source, "file:", 5u) == 0) {
        return ZR_FALSE;
    }
    return !(((source[0] >= 'a' && source[0] <= 'z') || (source[0] >= 'A' && source[0] <= 'Z')) &&
             source[1] == ':');
}

static TZrBool library_project_manifest_v2_text_equals_ignore_case(const TZrChar *text,
                                                                    TZrSize length,
                                                                    const TZrChar *literal) {
    TZrSize index;

    if (text == ZR_NULL || literal == ZR_NULL || strlen(literal) != length) {
        return ZR_FALSE;
    }
    for (index = 0u; index < length; index++) {
        TZrChar left = text[index];
        TZrChar right = literal[index];

        if (left >= 'A' && left <= 'Z') {
            left = (TZrChar)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (TZrChar)(right - 'A' + 'a');
        }
        if (left != right) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static int library_project_manifest_v2_hex_value(TZrChar character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static TZrBool library_project_manifest_v2_parse_ipv4_words(const TZrChar *text,
                                                             TZrSize length,
                                                             TZrUInt16 *outWords) {
    TZrUInt32 octets[4];
    TZrUInt32 value = 0u;
    TZrSize index;
    TZrSize octetIndex = 0u;
    TZrBool hasDigit = ZR_FALSE;

    if (text == ZR_NULL || outWords == ZR_NULL || length == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index <= length; index++) {
        TZrChar character = index == length ? '.' : text[index];

        if (character == '.') {
            if (!hasDigit || value > 255u || octetIndex >= ZR_ARRAY_COUNT(octets)) {
                return ZR_FALSE;
            }
            octets[octetIndex++] = value;
            value = 0u;
            hasDigit = ZR_FALSE;
        } else if (character >= '0' && character <= '9') {
            if (value > 25u || (value == 25u && character > '5')) {
                return ZR_FALSE;
            }
            value = value * 10u + (TZrUInt32)(character - '0');
            hasDigit = ZR_TRUE;
        } else {
            return ZR_FALSE;
        }
    }
    if (octetIndex != ZR_ARRAY_COUNT(octets)) {
        return ZR_FALSE;
    }
    outWords[0] = (TZrUInt16)((octets[0] << 8u) | octets[1]);
    outWords[1] = (TZrUInt16)((octets[2] << 8u) | octets[3]);
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_parse_ipv6_address(const TZrChar *host,
                                                               TZrSize hostLength,
                                                               TZrBool *outIsLoopback) {
    TZrUInt16 groups[8] = {0u};
    const TZrChar *cursor;
    const TZrChar *end;
    TZrSize groupCount = 0u;
    int compressionIndex = -1;
    TZrSize index;

    if (outIsLoopback == ZR_NULL) {
        return ZR_FALSE;
    }
    *outIsLoopback = ZR_FALSE;
    if (host == ZR_NULL || hostLength < 4u || host[0] != '[' || host[hostLength - 1u] != ']') {
        return ZR_FALSE;
    }
    cursor = host + 1;
    end = host + hostLength - 1u;
    while (cursor < end) {
        const TZrChar *tokenStart;
        TZrUInt32 value = 0u;
        TZrSize tokenLength;

        if (cursor[0] == ':') {
            if (cursor + 1 >= end || cursor[1] != ':' || compressionIndex >= 0) {
                return ZR_FALSE;
            }
            compressionIndex = (int)groupCount;
            cursor += 2;
            continue;
        }
        tokenStart = cursor;
        while (cursor < end && cursor[0] != ':') {
            cursor++;
        }
        tokenLength = (TZrSize)(cursor - tokenStart);
        if (memchr(tokenStart, '.', tokenLength) != ZR_NULL) {
            if (cursor != end || groupCount > 6u ||
                !library_project_manifest_v2_parse_ipv4_words(tokenStart, tokenLength, &groups[groupCount])) {
                return ZR_FALSE;
            }
            groupCount += 2u;
            break;
        }
        if (tokenLength == 0u || tokenLength > 4u || groupCount >= ZR_ARRAY_COUNT(groups)) {
            return ZR_FALSE;
        }
        for (index = 0u; index < tokenLength; index++) {
            int digit = library_project_manifest_v2_hex_value(tokenStart[index]);

            if (digit < 0) {
                return ZR_FALSE;
            }
            value = (value << 4u) | (TZrUInt32)digit;
        }
        groups[groupCount++] = (TZrUInt16)value;
        if (cursor == end) {
            break;
        }
        if (cursor + 1 < end && cursor[1] == ':') {
            if (compressionIndex >= 0) {
                return ZR_FALSE;
            }
            compressionIndex = (int)groupCount;
            cursor += 2;
        } else {
            cursor++;
            if (cursor == end) {
                return ZR_FALSE;
            }
        }
    }
    if (compressionIndex >= 0) {
        TZrSize zeroCount;

        if (groupCount >= ZR_ARRAY_COUNT(groups)) {
            return ZR_FALSE;
        }
        zeroCount = ZR_ARRAY_COUNT(groups) - groupCount;
        memmove(&groups[(TZrSize)compressionIndex + zeroCount],
                &groups[(TZrSize)compressionIndex],
                (groupCount - (TZrSize)compressionIndex) * sizeof(groups[0]));
        memset(&groups[compressionIndex], 0, zeroCount * sizeof(groups[0]));
        groupCount = ZR_ARRAY_COUNT(groups);
    }
    if (groupCount != ZR_ARRAY_COUNT(groups)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < 7u; index++) {
        if (groups[index] != 0u) {
            break;
        }
    }
    if (index == 7u && groups[7] == 1u) {
        *outIsLoopback = ZR_TRUE;
        return ZR_TRUE;
    }
    for (index = 0u; index < 6u; index++) {
        if (groups[index] != 0u) {
            break;
        }
    }
    if (index == 6u && (groups[6] >> 8u) == 127u) {
        *outIsLoopback = ZR_TRUE;
        return ZR_TRUE;
    }
    for (index = 0u; index < 5u; index++) {
        if (groups[index] != 0u) {
            break;
        }
    }
    *outIsLoopback = index == 5u && groups[5] == 0xffffu && (groups[6] >> 8u) == 127u;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_is_loopback_host(const TZrChar *host, TZrSize hostLength) {
    return library_project_manifest_v2_text_equals_ignore_case(host, hostLength, "localhost") ||
           library_project_manifest_v2_text_equals_ignore_case(host, hostLength, "localhost.") ||
           (hostLength >= 4u && host[0] == '1' && host[1] == '2' && host[2] == '7' && host[3] == '.');
}

static TZrBool library_project_manifest_v2_is_registry_package_id(const TZrChar *source) {
    TZrSize index;

    if (source == ZR_NULL || source[0] == '\0') {
        return ZR_FALSE;
    }
    for (index = 0u; source[index] != '\0'; index++) {
        TZrChar character = source[index];

        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '.' || character == '_' ||
              character == '-')) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_has_network_uri(const TZrChar *source,
                                                            const TZrChar *scheme) {
    const TZrChar *authority;
    const TZrChar *cursor;
    const TZrChar *host;
    const TZrChar *hostEnd;
    TZrBool isLoopback = ZR_FALSE;

    if (source == ZR_NULL || scheme == ZR_NULL || strncmp(source, scheme, strlen(scheme)) != 0) {
        return ZR_FALSE;
    }
    authority = source + strlen(scheme);
    if (authority[0] == '\0' || authority[0] == '/' || authority[0] == '\\' ||
        authority[0] == '?' || authority[0] == '#') {
        return ZR_FALSE;
    }
    if (((authority[0] >= 'a' && authority[0] <= 'z') ||
         (authority[0] >= 'A' && authority[0] <= 'Z')) &&
        authority[1] == ':' && (authority[2] == '\0' || authority[2] == '/' || authority[2] == '\\')) {
        return ZR_FALSE;
    }
    for (cursor = authority; cursor[0] != '\0' && cursor[0] != '/' && cursor[0] != '?' && cursor[0] != '#'; cursor++) {
        if (cursor[0] == '\\' || cursor[0] == '%' || cursor[0] <= ' ') {
            return ZR_FALSE;
        }
    }
    host = authority;
    for (hostEnd = authority; hostEnd < cursor; hostEnd++) {
        if (hostEnd[0] == '@') {
            host = hostEnd + 1;
        }
    }
    if (host == cursor) {
        return ZR_FALSE;
    }
    if (host[0] == '[') {
        for (hostEnd = host + 1; hostEnd < cursor && hostEnd[0] != ']'; hostEnd++) {
        }
        if (hostEnd == cursor) {
            return ZR_FALSE;
        }
        hostEnd++;
        if (hostEnd < cursor && hostEnd[0] != ':') {
            return ZR_FALSE;
        }
        if (!library_project_manifest_v2_parse_ipv6_address(host,
                                                            (TZrSize)(hostEnd - host),
                                                            &isLoopback) ||
            isLoopback) {
            return ZR_FALSE;
        }
    } else {
        for (hostEnd = host; hostEnd < cursor && hostEnd[0] != ':'; hostEnd++) {
        }
    }
    if (hostEnd == host || (host[0] != '[' &&
                            library_project_manifest_v2_is_loopback_host(host, (TZrSize)(hostEnd - host)))) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_has_network_scheme(const TZrChar *source,
                                                               TZrBool allowGitScheme) {
    if (!library_project_manifest_v2_path_source_is_portable(source)) {
        return ZR_FALSE;
    }
    return library_project_manifest_v2_has_network_uri(source, "https://") ||
           library_project_manifest_v2_has_network_uri(source, "http://") ||
           (allowGitScheme && (library_project_manifest_v2_has_network_uri(source, "ssh://") ||
                               library_project_manifest_v2_has_network_uri(source, "git://")));
}

static TZrBool library_project_manifest_v2_dependency_source_is_publishable(
        EZrLibrary_ProjectManifestDependencySourceKind sourceKind,
        const TZrChar *source) {
    switch (sourceKind) {
    case ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH:
        return library_project_manifest_v2_path_source_is_portable(source);
    case ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY:
        return library_project_manifest_v2_has_network_scheme(source, ZR_FALSE) ||
               library_project_manifest_v2_is_registry_package_id(source);
    case ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT:
        return library_project_manifest_v2_has_network_scheme(source, ZR_TRUE);
    default:
        return ZR_FALSE;
    }
}

static TZrBool library_project_manifest_v2_copy_display_segments(const TZrChar *segments,
                                                                  TZrChar *outLiteral,
                                                                  TZrSize outLiteralSize) {
    TZrSize index;
    TZrSize length;

    if (segments == ZR_NULL || outLiteral == ZR_NULL || outLiteralSize == 0u || segments[0] == '\0') {
        return ZR_FALSE;
    }
    length = strlen(segments);
    if (length + 1u > outLiteralSize) {
        return ZR_FALSE;
    }
    for (index = 0u; index < length; index++) {
        outLiteral[index] = segments[index] == '.' ? '/' : segments[index];
    }
    outLiteral[length] = '\0';
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_specifier_to_literal(const SZrLibrary_ModuleSpecifier *specifier,
                                                                 TZrChar *outLiteral,
                                                                 TZrSize outLiteralSize) {
    TZrChar segments[ZR_LIBRARY_MAX_PATH_LENGTH];
    int written;

    if (specifier == ZR_NULL || outLiteral == ZR_NULL || outLiteralSize == 0u) {
        return ZR_FALSE;
    }
    outLiteral[0] = '\0';
    switch (specifier->kind) {
    case ZR_LIBRARY_MODULE_SPECIFIER_KIND_OFFICIAL_NATIVE:
    case ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE:
        return library_project_manifest_v2_copy_display_segments(specifier->identity.segments,
                                                                 outLiteral,
                                                                 outLiteralSize);
    case ZR_LIBRARY_MODULE_SPECIFIER_KIND_REGISTERED_NATIVE:
        if (!library_project_manifest_v2_copy_display_segments(specifier->identity.segments,
                                                               segments,
                                                               sizeof(segments))) {
            return ZR_FALSE;
        }
        written = snprintf(outLiteral, outLiteralSize, "native:%s", segments);
        return written >= 0 && (TZrSize)written < outLiteralSize;
    case ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE:
        if (specifier->identity.packageName[0] == '\0') {
            return ZR_FALSE;
        }
        if (specifier->identity.segments[0] == '\0') {
            written = snprintf(outLiteral, outLiteralSize, "@%s", specifier->identity.packageName);
            return written >= 0 && (TZrSize)written < outLiteralSize;
        }
        if (!library_project_manifest_v2_copy_display_segments(specifier->identity.segments,
                                                               segments,
                                                               sizeof(segments))) {
            return ZR_FALSE;
        }
        written = snprintf(outLiteral, outLiteralSize, "@%s/%s", specifier->identity.packageName, segments);
        return written >= 0 && (TZrSize)written < outLiteralSize;
    case ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE:
        if (specifier->locator[0] == '\0' || strlen(specifier->locator) + 1u > outLiteralSize) {
            return ZR_FALSE;
        }
        strcpy(outLiteral, specifier->locator);
        return ZR_TRUE;
    default:
        return ZR_FALSE;
    }
}

static TZrBool library_project_manifest_v2_package_identity_to_literal(
        const SZrLibrary_ModuleIdentity *identity,
        TZrChar *outLiteral,
        TZrSize outLiteralSize) {
    int written;

    if (identity == ZR_NULL || outLiteral == ZR_NULL || outLiteralSize == 0u ||
        identity->domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE || identity->packageName[0] == '\0' ||
        identity->segments[0] != '\0') {
        return ZR_FALSE;
    }
    written = snprintf(outLiteral, outLiteralSize, "@%s", identity->packageName);
    return written >= 0 && (TZrSize)written < outLiteralSize;
}

static TZrBool library_project_manifest_v2_alias_index_at_ordinal(const SZrLibrary_Project *project,
                                                                   TZrSize ordinal,
                                                                   TZrSize *outIndex) {
    TZrSize index;

    if (project == ZR_NULL || outIndex == ZR_NULL || ordinal >= project->manifestAliasCount) {
        return ZR_FALSE;
    }
    for (index = 0u; index < project->manifestAliasCount; index++) {
        const TZrChar *key = library_project_manifest_v2_string_text(project->manifestAliases[index].root);
        TZrSize otherIndex;
        TZrSize rank = 0u;

        if (key == ZR_NULL || !library_project_manifest_v2_parse_alias_root(key)) {
            return ZR_FALSE;
        }
        for (otherIndex = 0u; otherIndex < project->manifestAliasCount; otherIndex++) {
            const TZrChar *otherKey = library_project_manifest_v2_string_text(project->manifestAliases[otherIndex].root);
            int comparison;

            if (otherKey == ZR_NULL || !library_project_manifest_v2_parse_alias_root(otherKey)) {
                return ZR_FALSE;
            }
            comparison = strcmp(otherKey, key);
            if ((otherIndex != index && comparison == 0) || comparison < 0) {
                rank++;
            }
        }
        if (rank == ordinal) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool library_project_manifest_v2_export_index_at_ordinal(const SZrLibrary_Project *project,
                                                                    TZrSize ordinal,
                                                                    TZrSize *outIndex) {
    TZrSize index;

    if (project == ZR_NULL || outIndex == ZR_NULL || ordinal >= project->packageExportCount) {
        return ZR_FALSE;
    }
    for (index = 0u; index < project->packageExportCount; index++) {
        const TZrChar *key = library_project_manifest_v2_string_text(project->packageExports[index].key);
        TZrChar canonicalKey[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize otherIndex;
        TZrSize rank = 0u;

        if (key == ZR_NULL || !library_project_manifest_v2_export_key_to_canonical(key,
                                                                                    canonicalKey,
                                                                                    sizeof(canonicalKey)) ||
            strcmp(key, canonicalKey) != 0) {
            return ZR_FALSE;
        }
        for (otherIndex = 0u; otherIndex < project->packageExportCount; otherIndex++) {
            const TZrChar *otherKey = library_project_manifest_v2_string_text(project->packageExports[otherIndex].key);
            int comparison;

            if (otherKey == ZR_NULL) {
                return ZR_FALSE;
            }
            comparison = strcmp(otherKey, key);
            if ((otherIndex != index && comparison == 0) || comparison < 0) {
                rank++;
            }
        }
        if (rank == ordinal) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool library_project_manifest_v2_dependency_index_at_ordinal(const SZrLibrary_Project *project,
                                                                        TZrSize ordinal,
                                                                        TZrSize *outIndex) {
    TZrSize index;

    if (project == ZR_NULL || outIndex == ZR_NULL || ordinal >= project->manifestDependencyCount) {
        return ZR_FALSE;
    }
    for (index = 0u; index < project->manifestDependencyCount; index++) {
        const TZrChar *packageName = project->manifestDependencies[index].packageIdentity.packageName;
        TZrSize otherIndex;
        TZrSize rank = 0u;

        if (project->manifestDependencies[index].packageIdentity.domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE ||
            packageName[0] == '\0' || project->manifestDependencies[index].packageIdentity.segments[0] != '\0') {
            return ZR_FALSE;
        }
        for (otherIndex = 0u; otherIndex < project->manifestDependencyCount; otherIndex++) {
            const SZrLibrary_ModuleIdentity *otherIdentity =
                    &project->manifestDependencies[otherIndex].packageIdentity;
            int comparison;

            if (otherIdentity->domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE || otherIdentity->packageName[0] == '\0' ||
                otherIdentity->segments[0] != '\0') {
                return ZR_FALSE;
            }
            comparison = strcmp(otherIdentity->packageName, packageName);
            if ((otherIndex != index && comparison == 0) || comparison < 0) {
                rank++;
            }
        }
        if (rank == ordinal) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool library_project_manifest_v2_dependency_source_kind_is_valid(
        EZrLibrary_ProjectManifestDependencySourceKind sourceKind) {
    return sourceKind == ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH ||
           sourceKind == ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY ||
           sourceKind == ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT;
}

TZrBool library_project_manifest_v2_validate_writer_input(const SZrLibrary_Project *project) {
    TZrSize index;

    if (project == ZR_NULL || project->manifestVersion != 2u ||
        !library_project_manifest_v2_has_nonempty_string(project->name) ||
        !library_project_manifest_v2_has_nonempty_string(project->version) ||
        !library_project_manifest_v2_has_nonempty_string(project->assemblyKind) ||
        !library_project_manifest_v2_has_nonempty_string(project->source) ||
        !library_project_manifest_v2_has_nonempty_string(project->binary) ||
        !library_project_manifest_v2_has_nonempty_string(project->entry) ||
        (project->manifestAliasCount > 0u && project->manifestAliases == ZR_NULL) ||
        (project->manifestDependencyCount > 0u && project->manifestDependencies == ZR_NULL) ||
        (project->packageExportCount > 0u && project->packageExports == ZR_NULL)) {
        return ZR_FALSE;
    }
    if (project->packageIdentity.domain == ZR_LIBRARY_MODULE_DOMAIN_INVALID) {
        if (project->packageExportCount != 0u) {
            return ZR_FALSE;
        }
    } else if (project->packageIdentity.domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE ||
               project->packageIdentity.packageName[0] == '\0' || project->packageIdentity.segments[0] != '\0' ||
               project->packageExportCount == 0u) {
        return ZR_FALSE;
    }
    if (!library_project_manifest_v2_validate_alias_package_targets(project)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < project->manifestDependencyCount; index++) {
        const SZrLibrary_ProjectManifestDependency *dependency = &project->manifestDependencies[index];

        if (!library_project_manifest_v2_has_nonempty_string(dependency->versionRequirement) ||
            !library_project_manifest_v2_has_nonempty_string(dependency->source) ||
            !library_project_manifest_v2_dependency_source_kind_is_valid(dependency->sourceKind) ||
            !library_project_manifest_v2_dependency_source_is_publishable(
                    dependency->sourceKind,
                    library_project_manifest_v2_string_text(dependency->source))) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static cJSON *library_project_manifest_v2_build_aliases(const SZrLibrary_Project *project) {
    cJSON *aliasesJson;
    TZrSize ordinal;

    aliasesJson = cJSON_CreateObject();
    if (aliasesJson == ZR_NULL) {
        return ZR_NULL;
    }
    for (ordinal = 0u; ordinal < project->manifestAliasCount; ordinal++) {
        TZrSize index;
        const SZrLibrary_ProjectManifestAlias *alias;
        const TZrChar *root;
        TZrChar targetLiteral[ZR_LIBRARY_MAX_PATH_LENGTH];

        if (!library_project_manifest_v2_alias_index_at_ordinal(project, ordinal, &index)) {
            cJSON_Delete(aliasesJson);
            return ZR_NULL;
        }
        alias = &project->manifestAliases[index];
        root = library_project_manifest_v2_string_text(alias->root);
        if (!library_project_manifest_v2_alias_target_is_supported(&alias->target) ||
            !library_project_manifest_v2_specifier_to_literal(&alias->target, targetLiteral, sizeof(targetLiteral)) ||
            cJSON_AddStringToObject(aliasesJson, root, targetLiteral) == ZR_NULL) {
            cJSON_Delete(aliasesJson);
            return ZR_NULL;
        }
    }
    return aliasesJson;
}

static cJSON *library_project_manifest_v2_build_package(const SZrLibrary_Project *project) {
    cJSON *packageJson;
    cJSON *exportsJson;
    TZrChar packageName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize ordinal;

    packageJson = cJSON_CreateObject();
    exportsJson = cJSON_CreateObject();
    if (packageJson == ZR_NULL || exportsJson == ZR_NULL ||
        !library_project_manifest_v2_package_identity_to_literal(&project->packageIdentity,
                                                                 packageName,
                                                                 sizeof(packageName)) ||
        cJSON_AddStringToObject(packageJson, "name", packageName) == ZR_NULL) {
        cJSON_Delete(packageJson);
        cJSON_Delete(exportsJson);
        return ZR_NULL;
    }
    for (ordinal = 0u; ordinal < project->packageExportCount; ordinal++) {
        TZrSize index;
        const SZrLibrary_ProjectPackageExport *projectExport;
        const TZrChar *key;
        TZrChar targetLiteral[ZR_LIBRARY_MAX_PATH_LENGTH];

        if (!library_project_manifest_v2_export_index_at_ordinal(project, ordinal, &index)) {
            cJSON_Delete(packageJson);
            cJSON_Delete(exportsJson);
            return ZR_NULL;
        }
        projectExport = &project->packageExports[index];
        key = library_project_manifest_v2_string_text(projectExport->key);
        if (projectExport->target.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE ||
            !library_project_manifest_v2_specifier_to_literal(&projectExport->target,
                                                              targetLiteral,
                                                              sizeof(targetLiteral)) ||
            cJSON_AddStringToObject(exportsJson, key, targetLiteral) == ZR_NULL) {
            cJSON_Delete(packageJson);
            cJSON_Delete(exportsJson);
            return ZR_NULL;
        }
    }
    if (!cJSON_AddItemToObject(packageJson, "exports", exportsJson)) {
        cJSON_Delete(packageJson);
        cJSON_Delete(exportsJson);
        return ZR_NULL;
    }
    return packageJson;
}

static const TZrChar *library_project_manifest_v2_dependency_source_field(
        EZrLibrary_ProjectManifestDependencySourceKind sourceKind) {
    switch (sourceKind) {
    case ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH:
        return "path";
    case ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY:
        return "registry";
    case ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT:
        return "git";
    default:
        return ZR_NULL;
    }
}

static cJSON *library_project_manifest_v2_build_dependencies(const SZrLibrary_Project *project) {
    cJSON *dependenciesJson;
    TZrSize ordinal;

    dependenciesJson = cJSON_CreateObject();
    if (dependenciesJson == ZR_NULL) {
        return ZR_NULL;
    }
    for (ordinal = 0u; ordinal < project->manifestDependencyCount; ordinal++) {
        TZrSize index;
        const SZrLibrary_ProjectManifestDependency *dependency;
        const TZrChar *sourceField;
        TZrChar packageLiteral[ZR_LIBRARY_MAX_PATH_LENGTH];
        cJSON *dependencyJson;

        if (!library_project_manifest_v2_dependency_index_at_ordinal(project, ordinal, &index)) {
            cJSON_Delete(dependenciesJson);
            return ZR_NULL;
        }
        dependency = &project->manifestDependencies[index];
        sourceField = library_project_manifest_v2_dependency_source_field(dependency->sourceKind);
        dependencyJson = cJSON_CreateObject();
        if (sourceField == ZR_NULL || dependencyJson == ZR_NULL ||
            !library_project_manifest_v2_package_identity_to_literal(&dependency->packageIdentity,
                                                                     packageLiteral,
                                                                     sizeof(packageLiteral)) ||
            cJSON_AddStringToObject(dependencyJson,
                                    "version",
                                    library_project_manifest_v2_string_text(dependency->versionRequirement)) == ZR_NULL ||
            cJSON_AddStringToObject(dependencyJson,
                                    sourceField,
                                    library_project_manifest_v2_string_text(dependency->source)) == ZR_NULL ||
            !cJSON_AddItemToObject(dependenciesJson, packageLiteral, dependencyJson)) {
            cJSON_Delete(dependencyJson);
            cJSON_Delete(dependenciesJson);
            return ZR_NULL;
        }
    }
    return dependenciesJson;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_Write(const SZrLibrary_Project *project,
                                                          TZrChar *outManifest,
                                                          TZrSize outManifestSize) {
    cJSON *manifestJson = ZR_NULL;
    cJSON *aliasesJson = ZR_NULL;
    cJSON *packageJson = ZR_NULL;
    cJSON *dependenciesJson = ZR_NULL;
    TZrBool ok = ZR_FALSE;

    if (outManifest != ZR_NULL && outManifestSize > 0u) {
        outManifest[0] = '\0';
    }
    if (outManifest == ZR_NULL || outManifestSize == 0u || outManifestSize > (TZrSize)INT_MAX ||
        !library_project_manifest_v2_validate_writer_input(project)) {
        return ZR_FALSE;
    }
    manifestJson = cJSON_CreateObject();
    if (manifestJson == ZR_NULL ||
        cJSON_AddNumberToObject(manifestJson, "manifestVersion", 2) == ZR_NULL ||
        cJSON_AddStringToObject(manifestJson, "name", library_project_manifest_v2_string_text(project->name)) == ZR_NULL ||
        cJSON_AddStringToObject(manifestJson, "version", library_project_manifest_v2_string_text(project->version)) == ZR_NULL ||
        cJSON_AddStringToObject(manifestJson, "kind", library_project_manifest_v2_string_text(project->assemblyKind)) == ZR_NULL ||
        cJSON_AddStringToObject(manifestJson, "source", library_project_manifest_v2_string_text(project->source)) == ZR_NULL ||
        cJSON_AddStringToObject(manifestJson, "binary", library_project_manifest_v2_string_text(project->binary)) == ZR_NULL ||
        cJSON_AddStringToObject(manifestJson, "entry", library_project_manifest_v2_string_text(project->entry)) == ZR_NULL) {
        goto cleanup;
    }
    if (project->manifestAliasCount > 0u) {
        aliasesJson = library_project_manifest_v2_build_aliases(project);
        if (aliasesJson == ZR_NULL || !cJSON_AddItemToObject(manifestJson, "aliases", aliasesJson)) {
            goto cleanup;
        }
        aliasesJson = ZR_NULL;
    }
    if (project->packageIdentity.domain == ZR_LIBRARY_MODULE_DOMAIN_PACKAGE) {
        packageJson = library_project_manifest_v2_build_package(project);
        if (packageJson == ZR_NULL || !cJSON_AddItemToObject(manifestJson, "package", packageJson)) {
            goto cleanup;
        }
        packageJson = ZR_NULL;
    }
    if (project->manifestDependencyCount > 0u) {
        dependenciesJson = library_project_manifest_v2_build_dependencies(project);
        if (dependenciesJson == ZR_NULL || !cJSON_AddItemToObject(manifestJson, "dependencies", dependenciesJson)) {
            goto cleanup;
        }
        dependenciesJson = ZR_NULL;
    }
    ok = cJSON_PrintPreallocated(manifestJson, outManifest, (int)outManifestSize, 0) ? ZR_TRUE : ZR_FALSE;

cleanup:
    if (!ok) {
        outManifest[0] = '\0';
    }
    cJSON_Delete(aliasesJson);
    cJSON_Delete(packageJson);
    cJSON_Delete(dependenciesJson);
    cJSON_Delete(manifestJson);
    return ok;
}

static TZrBool library_project_manifest_v2_lock_entry_index_at_ordinal(
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        TZrSize ordinal,
        TZrSize *outIndex) {
    TZrSize index;

    if (entries == ZR_NULL || outIndex == ZR_NULL || ordinal >= entryCount) {
        return ZR_FALSE;
    }
    for (index = 0u; index < entryCount; index++) {
        const SZrLibrary_ModuleIdentity *identity = &entries[index].packageIdentity;
        TZrSize otherIndex;
        TZrSize rank = 0u;

        if (identity->domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE || identity->packageName[0] == '\0' ||
            identity->segments[0] != '\0' || entries[index].resolvedVersion == ZR_NULL ||
            entries[index].resolvedVersion[0] == '\0' || entries[index].contentHash == ZR_NULL ||
            entries[index].contentHash[0] == '\0' || entries[index].transitiveIdentity == ZR_NULL ||
            entries[index].transitiveIdentity[0] == '\0' ||
            !library_project_manifest_v2_dependency_source_kind_is_valid(entries[index].providerSourceKind)) {
            return ZR_FALSE;
        }
        for (otherIndex = 0u; otherIndex < entryCount; otherIndex++) {
            const SZrLibrary_ModuleIdentity *otherIdentity = &entries[otherIndex].packageIdentity;
            int comparison;

            if (otherIdentity->domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE || otherIdentity->packageName[0] == '\0' ||
                otherIdentity->segments[0] != '\0') {
                return ZR_FALSE;
            }
            comparison = strcmp(otherIdentity->packageName, identity->packageName);
            if ((otherIndex != index && comparison == 0) || comparison < 0) {
                rank++;
            }
        }
        if (rank == ordinal) {
            *outIndex = index;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const SZrLibrary_ProjectManifestDependency *library_project_manifest_v2_find_dependency(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleIdentity *identity) {
    TZrSize index;

    for (index = 0u; index < project->manifestDependencyCount; index++) {
        if (ZrLibrary_ModuleIdentity_Equals(&project->manifestDependencies[index].packageIdentity, identity)) {
            return &project->manifestDependencies[index];
        }
    }
    return ZR_NULL;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_WriteDependencyLock(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        TZrChar *outLock,
        TZrSize outLockSize) {
    cJSON *lockJson = ZR_NULL;
    cJSON *dependenciesJson = ZR_NULL;
    TZrSize ordinal;
    TZrBool ok = ZR_FALSE;

    if (outLock != ZR_NULL && outLockSize > 0u) {
        outLock[0] = '\0';
    }
    if (outLock == ZR_NULL || outLockSize == 0u || outLockSize > (TZrSize)INT_MAX ||
        !library_project_manifest_v2_validate_writer_input(project) ||
        entryCount != project->manifestDependencyCount || (entryCount > 0u && entries == ZR_NULL)) {
        return ZR_FALSE;
    }
    lockJson = cJSON_CreateObject();
    dependenciesJson = cJSON_CreateObject();
    if (lockJson == ZR_NULL || dependenciesJson == ZR_NULL ||
        cJSON_AddNumberToObject(lockJson, "lockVersion", 1) == ZR_NULL) {
        goto cleanup;
    }
    for (ordinal = 0u; ordinal < entryCount; ordinal++) {
        TZrSize index;
        const SZrLibrary_ProjectManifestDependencyLockEntry *entry;
        const SZrLibrary_ProjectManifestDependency *dependency;
        const TZrChar *provider;
        TZrChar packageLiteral[ZR_LIBRARY_MAX_PATH_LENGTH];
        cJSON *entryJson;

        if (!library_project_manifest_v2_lock_entry_index_at_ordinal(entries, entryCount, ordinal, &index)) {
            goto cleanup;
        }
        entry = &entries[index];
        dependency = library_project_manifest_v2_find_dependency(project, &entry->packageIdentity);
        provider = library_project_manifest_v2_dependency_source_field(entry->providerSourceKind);
        entryJson = cJSON_CreateObject();
        if (dependency == ZR_NULL || dependency->sourceKind != entry->providerSourceKind || provider == ZR_NULL ||
            entryJson == ZR_NULL ||
            !library_project_manifest_v2_package_identity_to_literal(&entry->packageIdentity,
                                                                     packageLiteral,
                                                                     sizeof(packageLiteral)) ||
            cJSON_AddStringToObject(entryJson, "version", entry->resolvedVersion) == ZR_NULL ||
            cJSON_AddStringToObject(entryJson, "contentHash", entry->contentHash) == ZR_NULL ||
            cJSON_AddStringToObject(entryJson, "transitiveIdentity", entry->transitiveIdentity) == ZR_NULL ||
            cJSON_AddStringToObject(entryJson, "provider", provider) == ZR_NULL ||
            !cJSON_AddItemToObject(dependenciesJson, packageLiteral, entryJson)) {
            cJSON_Delete(entryJson);
            goto cleanup;
        }
    }
    if (!cJSON_AddItemToObject(lockJson, "dependencies", dependenciesJson)) {
        goto cleanup;
    }
    dependenciesJson = ZR_NULL;
    ok = cJSON_PrintPreallocated(lockJson, outLock, (int)outLockSize, 0) ? ZR_TRUE : ZR_FALSE;

cleanup:
    if (!ok) {
        outLock[0] = '\0';
    }
    cJSON_Delete(dependenciesJson);
    cJSON_Delete(lockJson);
    return ok;
}
