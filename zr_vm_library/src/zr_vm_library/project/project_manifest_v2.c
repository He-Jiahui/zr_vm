#include "project/project_manifest_v2.h"

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
