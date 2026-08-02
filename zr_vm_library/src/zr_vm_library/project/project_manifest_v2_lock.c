#include "project/project_manifest_v2.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/memory.h"

static void library_project_manifest_v2_lock_set_error(
        TZrChar *errorBuffer,
        TZrSize errorBufferSize,
        const TZrChar *format,
        ...) {
    va_list arguments;

    if (errorBuffer == ZR_NULL || errorBufferSize == 0u) {
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

static TZrSize library_project_manifest_v2_lock_field_count(
        const cJSON *object,
        const TZrChar *fieldName) {
    const cJSON *field;
    TZrSize count = 0u;

    if (!cJSON_IsObject(object) || fieldName == ZR_NULL) {
        return 0u;
    }
    cJSON_ArrayForEach(field, object) {
        if (field->string != ZR_NULL && strcmp(field->string, fieldName) == 0) {
            count++;
        }
    }
    return count;
}

static const SZrLibrary_ProjectManifestDependency *
library_project_manifest_v2_lock_find_dependency(
        const SZrLibrary_ProjectManifestDependency *dependencies,
        TZrSize dependencyCount,
        const SZrLibrary_ModuleIdentity *identity) {
    const SZrLibrary_ProjectManifestDependency *match = ZR_NULL;

    for (TZrSize index = 0u; index < dependencyCount; index++) {
        if (!ZrLibrary_ModuleIdentity_Equals(
                    &dependencies[index].packageIdentity, identity)) {
            continue;
        }
        if (match != ZR_NULL) {
            return ZR_NULL;
        }
        match = &dependencies[index];
    }
    return match;
}

static TZrBool library_project_manifest_v2_lock_parse_provider(
        const TZrChar *text,
        EZrLibrary_ProjectManifestDependencySourceKind *outProvider) {
    if (text == ZR_NULL || outProvider == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(text, "path") == 0) {
        *outProvider = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH;
        return ZR_TRUE;
    }
    if (strcmp(text, "registry") == 0) {
        *outProvider = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY;
        return ZR_TRUE;
    }
    if (strcmp(text, "git") == 0) {
        *outProvider = ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool library_project_manifest_v2_lock_parse_entry(
        const cJSON *entryJson,
        const TZrChar **outVersion,
        const TZrChar **outContentHash,
        const TZrChar **outTransitiveIdentity,
        EZrLibrary_ProjectManifestDependencySourceKind *outProvider) {
    const cJSON *field;
    const cJSON *versionJson;
    const cJSON *contentHashJson;
    const cJSON *transitiveIdentityJson;
    const cJSON *providerJson;
    TZrSize fieldCount = 0u;

    if (!cJSON_IsObject(entryJson) || outVersion == ZR_NULL ||
        outContentHash == ZR_NULL || outTransitiveIdentity == ZR_NULL ||
        outProvider == ZR_NULL) {
        return ZR_FALSE;
    }
    cJSON_ArrayForEach(field, entryJson) {
        fieldCount++;
    }
    if (fieldCount != 4u ||
        library_project_manifest_v2_lock_field_count(entryJson, "version") != 1u ||
        library_project_manifest_v2_lock_field_count(entryJson, "contentHash") != 1u ||
        library_project_manifest_v2_lock_field_count(entryJson, "transitiveIdentity") != 1u ||
        library_project_manifest_v2_lock_field_count(entryJson, "provider") != 1u) {
        return ZR_FALSE;
    }
    versionJson = cJSON_GetObjectItemCaseSensitive(entryJson, "version");
    contentHashJson = cJSON_GetObjectItemCaseSensitive(entryJson, "contentHash");
    transitiveIdentityJson = cJSON_GetObjectItemCaseSensitive(
            entryJson, "transitiveIdentity");
    providerJson = cJSON_GetObjectItemCaseSensitive(entryJson, "provider");
    if (!cJSON_IsString(versionJson) || versionJson->valuestring == ZR_NULL ||
        versionJson->valuestring[0] == '\0' ||
        !cJSON_IsString(contentHashJson) ||
        contentHashJson->valuestring == ZR_NULL ||
        contentHashJson->valuestring[0] == '\0' ||
        !cJSON_IsString(transitiveIdentityJson) ||
        transitiveIdentityJson->valuestring == ZR_NULL ||
        transitiveIdentityJson->valuestring[0] == '\0' ||
        !cJSON_IsString(providerJson) || providerJson->valuestring == ZR_NULL ||
        !library_project_manifest_v2_lock_parse_provider(
                providerJson->valuestring, outProvider)) {
        return ZR_FALSE;
    }
    *outVersion = versionJson->valuestring;
    *outContentHash = contentHashJson->valuestring;
    *outTransitiveIdentity = transitiveIdentityJson->valuestring;
    return ZR_TRUE;
}

static TZrBool library_project_manifest_v2_lock_validate_collection(
        const cJSON *collectionJson,
        const SZrLibrary_ProjectManifestDependency *dependencies,
        TZrSize dependencyCount,
        TZrSize *inOutTextByteCount) {
    const cJSON *entryJson;
    TZrSize entryCount = 0u;

    if (!cJSON_IsObject(collectionJson) || inOutTextByteCount == ZR_NULL) {
        return ZR_FALSE;
    }
    cJSON_ArrayForEach(entryJson, collectionJson) {
        SZrLibrary_ModuleSpecifier specifier;
        const SZrLibrary_ProjectManifestDependency *dependency;
        const TZrChar *version;
        const TZrChar *contentHash;
        const TZrChar *transitiveIdentity;
        EZrLibrary_ProjectManifestDependencySourceKind provider;
        const cJSON *priorEntry;
        TZrChar specifierError[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize textByteCount;

        entryCount++;
        if (entryJson->string == ZR_NULL ||
            !ZrLibrary_ModuleSpecifier_Parse(
                    entryJson->string,
                    &specifier,
                    specifierError,
                    sizeof(specifierError)) ||
            specifier.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE ||
            specifier.identity.segments[0] != '\0' ||
            !library_project_manifest_v2_lock_parse_entry(
                    entryJson,
                    &version,
                    &contentHash,
                    &transitiveIdentity,
                    &provider)) {
            return ZR_FALSE;
        }
        dependency = library_project_manifest_v2_lock_find_dependency(
                dependencies, dependencyCount, &specifier.identity);
        if (dependency == ZR_NULL || dependency->sourceKind != provider) {
            return ZR_FALSE;
        }
        for (priorEntry = collectionJson->child;
             priorEntry != ZR_NULL && priorEntry != entryJson;
             priorEntry = priorEntry->next) {
            if (priorEntry->string != ZR_NULL &&
                strcmp(priorEntry->string, entryJson->string) == 0) {
                return ZR_FALSE;
            }
        }
        textByteCount = strlen(version) + 1u;
        if (textByteCount > SIZE_MAX - (strlen(contentHash) + 1u) ||
            textByteCount + strlen(contentHash) + 1u >
                    SIZE_MAX - (strlen(transitiveIdentity) + 1u)) {
            return ZR_FALSE;
        }
        textByteCount += strlen(contentHash) + 1u;
        textByteCount += strlen(transitiveIdentity) + 1u;
        if (*inOutTextByteCount > SIZE_MAX - textByteCount) {
            return ZR_FALSE;
        }
        *inOutTextByteCount += textByteCount;
    }
    return (TZrBool)(entryCount == dependencyCount);
}

static TZrBool library_project_manifest_v2_lock_copy_collection(
        const cJSON *collectionJson,
        EZrLibrary_ProviderPhase providerPhase,
        SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize *inOutEntryIndex,
        TZrChar **inOutTextCursor) {
    const cJSON *entryJson;

    cJSON_ArrayForEach(entryJson, collectionJson) {
        SZrLibrary_ProjectManifestDependencyLockEntry *entry =
                &entries[*inOutEntryIndex];
        SZrLibrary_ModuleSpecifier specifier;
        const TZrChar *version;
        const TZrChar *contentHash;
        const TZrChar *transitiveIdentity;
        TZrChar specifierError[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize textLength;

        if (!ZrLibrary_ModuleSpecifier_Parse(
                    entryJson->string,
                    &specifier,
                    specifierError,
                    sizeof(specifierError)) ||
            !library_project_manifest_v2_lock_parse_entry(
                    entryJson,
                    &version,
                    &contentHash,
                    &transitiveIdentity,
                    &entry->providerSourceKind)) {
            return ZR_FALSE;
        }
        entry->packageIdentity = specifier.identity;
        entry->providerPhase = providerPhase;

        textLength = strlen(version) + 1u;
        memcpy(*inOutTextCursor, version, textLength);
        entry->resolvedVersion = *inOutTextCursor;
        *inOutTextCursor += textLength;
        textLength = strlen(contentHash) + 1u;
        memcpy(*inOutTextCursor, contentHash, textLength);
        entry->contentHash = *inOutTextCursor;
        *inOutTextCursor += textLength;
        textLength = strlen(transitiveIdentity) + 1u;
        memcpy(*inOutTextCursor, transitiveIdentity, textLength);
        entry->transitiveIdentity = *inOutTextCursor;
        *inOutTextCursor += textLength;
        (*inOutEntryIndex)++;
    }
    return ZR_TRUE;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_ReadDependencyLock(
        SZrState *state,
        SZrLibrary_Project *project,
        const TZrChar *rawLock,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    cJSON *lockJson = ZR_NULL;
    cJSON *dependenciesJson;
    cJSON *buildDependenciesJson;
    cJSON *lockVersionJson;
    cJSON *field;
    SZrLibrary_ProjectManifestDependencyLockEntry *entries = ZR_NULL;
    TZrSize entryCount;
    TZrSize entryBytes;
    TZrSize textBytes = 0u;
    TZrSize storageSize;
    TZrSize entryIndex = 0u;
    TZrSize fieldCount = 0u;
    TZrChar *textCursor;
    TZrBool ok = ZR_FALSE;

    library_project_manifest_v2_lock_set_error(
            errorBuffer, errorBufferSize, ZR_NULL);
    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL ||
        project->manifestVersion != 2u || rawLock == ZR_NULL) {
        library_project_manifest_v2_lock_set_error(
                errorBuffer,
                errorBufferSize,
                "project.lock.invalid_argument: v2 project and lock text are required");
        return ZR_FALSE;
    }
    lockJson = cJSON_ParseWithOpts(rawLock, ZR_NULL, 1);
    if (!cJSON_IsObject(lockJson)) {
        library_project_manifest_v2_lock_set_error(
                errorBuffer, errorBufferSize, "project.lock.invalid_json");
        goto cleanup;
    }
    cJSON_ArrayForEach(field, lockJson) {
        fieldCount++;
    }
    if (fieldCount != (project->manifestBuildDependencyCount > 0u ? 3u : 2u) ||
        library_project_manifest_v2_lock_field_count(lockJson, "lockVersion") != 1u ||
        library_project_manifest_v2_lock_field_count(lockJson, "dependencies") != 1u ||
        library_project_manifest_v2_lock_field_count(lockJson, "buildDependencies") !=
                (project->manifestBuildDependencyCount > 0u ? 1u : 0u)) {
        library_project_manifest_v2_lock_set_error(
                errorBuffer,
                errorBufferSize,
                "project.lock.shape: canonical lock sections are required exactly once");
        goto cleanup;
    }
    lockVersionJson = cJSON_GetObjectItemCaseSensitive(lockJson, "lockVersion");
    dependenciesJson = cJSON_GetObjectItemCaseSensitive(lockJson, "dependencies");
    buildDependenciesJson = cJSON_GetObjectItemCaseSensitive(
            lockJson, "buildDependencies");
    if (!cJSON_IsNumber(lockVersionJson) || lockVersionJson->valueint != 1 ||
        lockVersionJson->valuedouble != 1.0 ||
        !library_project_manifest_v2_lock_validate_collection(
                dependenciesJson,
                project->manifestDependencies,
                project->manifestDependencyCount,
                &textBytes) ||
        (project->manifestBuildDependencyCount > 0u &&
         !library_project_manifest_v2_lock_validate_collection(
                 buildDependenciesJson,
                 project->manifestBuildDependencies,
                 project->manifestBuildDependencyCount,
                 &textBytes))) {
        library_project_manifest_v2_lock_set_error(
                errorBuffer,
                errorBufferSize,
                "project.lock.contract: dependency identity, phase, source, or entry is invalid");
        goto cleanup;
    }
    entryCount = project->manifestDependencyCount +
                 project->manifestBuildDependencyCount;
    if (entryCount > SIZE_MAX / sizeof(*entries)) {
        library_project_manifest_v2_lock_set_error(
                errorBuffer, errorBufferSize, "project.lock.size_overflow");
        goto cleanup;
    }
    entryBytes = entryCount * sizeof(*entries);
    if (entryBytes > SIZE_MAX - textBytes) {
        library_project_manifest_v2_lock_set_error(
                errorBuffer, errorBufferSize, "project.lock.size_overflow");
        goto cleanup;
    }
    storageSize = entryBytes + textBytes;
    if (storageSize > 0u) {
        entries = (SZrLibrary_ProjectManifestDependencyLockEntry *)
                ZrCore_Memory_RawMallocWithType(
                        state->global,
                        storageSize,
                        ZR_MEMORY_NATIVE_TYPE_PROJECT);
        if (entries == ZR_NULL) {
            library_project_manifest_v2_lock_set_error(
                    errorBuffer, errorBufferSize, "project.lock.allocation_failed");
            goto cleanup;
        }
        memset(entries, 0, storageSize);
        textCursor = (TZrChar *)entries + entryBytes;
        if (!library_project_manifest_v2_lock_copy_collection(
                    dependenciesJson,
                    ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
                    entries,
                    &entryIndex,
                    &textCursor) ||
            (project->manifestBuildDependencyCount > 0u &&
             !library_project_manifest_v2_lock_copy_collection(
                     buildDependenciesJson,
                     ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
                     entries,
                     &entryIndex,
                     &textCursor)) ||
            entryIndex != entryCount) {
            library_project_manifest_v2_lock_set_error(
                    errorBuffer, errorBufferSize, "project.lock.copy_failed");
            goto cleanup;
        }
    }

    if (project->manifestDependencyLockEntries != ZR_NULL &&
        project->manifestDependencyLockStorageSize > 0u) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                project->manifestDependencyLockEntries,
                project->manifestDependencyLockStorageSize,
                ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->manifestDependencyLockEntries = entries;
    project->manifestDependencyLockEntryCount = entryCount;
    project->manifestDependencyLockStorageSize = storageSize;
    entries = ZR_NULL;
    ok = ZR_TRUE;

cleanup:
    if (entries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                entries,
                storageSize,
                ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    cJSON_Delete(lockJson);
    return ok;
}

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
