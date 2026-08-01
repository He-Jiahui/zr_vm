#include "project/project_manifest_v2.h"

#include <limits.h>
#include <string.h>

static const SZrLibrary_ProjectManifestDependencyLockEntry *
library_project_manifest_v2_find_lock_entry(
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        const SZrLibrary_ModuleIdentity *identity,
        EZrLibrary_ProviderPhase providerPhase) {
    const SZrLibrary_ProjectManifestDependencyLockEntry *match = ZR_NULL;

    for (TZrSize index = 0u; index < entryCount; index++) {
        if (entries[index].providerPhase == providerPhase &&
            ZrLibrary_ModuleIdentity_Equals(&entries[index].packageIdentity, identity)) {
            if (match != ZR_NULL) {
                return ZR_NULL;
            }
            match = &entries[index];
        }
    }
    return match;
}

static cJSON *library_project_manifest_v2_build_lock_dependency_collection(
        const SZrLibrary_ProjectManifestDependency *dependencies,
        TZrSize dependencyCount,
        EZrLibrary_ProviderPhase providerPhase,
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount) {
    cJSON *dependenciesJson = cJSON_CreateObject();

    if (dependenciesJson == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize ordinal = 0u; ordinal < dependencyCount; ordinal++) {
        TZrSize dependencyIndex;
        const SZrLibrary_ProjectManifestDependency *dependency;
        const SZrLibrary_ProjectManifestDependencyLockEntry *entry;
        const TZrChar *provider;
        TZrChar packageLiteral[ZR_LIBRARY_MAX_PATH_LENGTH];
        cJSON *entryJson;

        if (!library_project_manifest_v2_dependency_index_at_ordinal(
                    dependencies, dependencyCount, ordinal, &dependencyIndex)) {
            cJSON_Delete(dependenciesJson);
            return ZR_NULL;
        }
        dependency = &dependencies[dependencyIndex];
        entry = library_project_manifest_v2_find_lock_entry(
                entries, entryCount, &dependency->packageIdentity, providerPhase);
        provider = entry == ZR_NULL
                           ? ZR_NULL
                           : library_project_manifest_v2_dependency_source_field(
                                     entry->providerSourceKind);
        entryJson = cJSON_CreateObject();
        if (entry == ZR_NULL || entry->resolvedVersion == ZR_NULL ||
            entry->resolvedVersion[0] == '\0' || entry->contentHash == ZR_NULL ||
            entry->contentHash[0] == '\0' || entry->transitiveIdentity == ZR_NULL ||
            entry->transitiveIdentity[0] == '\0' ||
            dependency->sourceKind != entry->providerSourceKind || provider == ZR_NULL ||
            entryJson == ZR_NULL ||
            !library_project_manifest_v2_package_identity_to_literal(
                    &entry->packageIdentity, packageLiteral, sizeof(packageLiteral)) ||
            cJSON_AddStringToObject(entryJson, "version", entry->resolvedVersion) == ZR_NULL ||
            cJSON_AddStringToObject(entryJson, "contentHash", entry->contentHash) == ZR_NULL ||
            cJSON_AddStringToObject(
                    entryJson, "transitiveIdentity", entry->transitiveIdentity) == ZR_NULL ||
            cJSON_AddStringToObject(entryJson, "provider", provider) == ZR_NULL ||
            !cJSON_AddItemToObject(dependenciesJson, packageLiteral, entryJson)) {
            cJSON_Delete(entryJson);
            cJSON_Delete(dependenciesJson);
            return ZR_NULL;
        }
    }
    return dependenciesJson;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_WriteDependencyLock(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        TZrChar *outLock,
        TZrSize outLockSize) {
    cJSON *lockJson = ZR_NULL;
    cJSON *dependenciesJson = ZR_NULL;
    cJSON *buildDependenciesJson = ZR_NULL;
    TZrBool ok = ZR_FALSE;

    if (outLock != ZR_NULL && outLockSize > 0u) {
        outLock[0] = '\0';
    }
    if (outLock == ZR_NULL || outLockSize == 0u || outLockSize > (TZrSize)INT_MAX ||
        !library_project_manifest_v2_validate_writer_input(project) ||
        entryCount != project->manifestDependencyCount + project->manifestBuildDependencyCount ||
        (entryCount > 0u && entries == ZR_NULL)) {
        return ZR_FALSE;
    }
    lockJson = cJSON_CreateObject();
    dependenciesJson = library_project_manifest_v2_build_lock_dependency_collection(
            project->manifestDependencies,
            project->manifestDependencyCount,
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
            entries,
            entryCount);
    if (lockJson == ZR_NULL || dependenciesJson == ZR_NULL ||
        cJSON_AddNumberToObject(lockJson, "lockVersion", 1) == ZR_NULL) {
        goto cleanup;
    }
    if (!cJSON_AddItemToObject(lockJson, "dependencies", dependenciesJson)) {
        goto cleanup;
    }
    dependenciesJson = ZR_NULL;
    if (project->manifestBuildDependencyCount > 0u) {
        buildDependenciesJson = library_project_manifest_v2_build_lock_dependency_collection(
                project->manifestBuildDependencies,
                project->manifestBuildDependencyCount,
                ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
                entries,
                entryCount);
        if (buildDependenciesJson == ZR_NULL ||
            !cJSON_AddItemToObject(
                    lockJson, "buildDependencies", buildDependenciesJson)) {
            goto cleanup;
        }
        buildDependenciesJson = ZR_NULL;
    }
    ok = cJSON_PrintPreallocated(lockJson, outLock, (int)outLockSize, 0) ? ZR_TRUE : ZR_FALSE;

cleanup:
    if (!ok) {
        outLock[0] = '\0';
    }
    cJSON_Delete(dependenciesJson);
    cJSON_Delete(buildDependenciesJson);
    cJSON_Delete(lockJson);
    return ok;
}
