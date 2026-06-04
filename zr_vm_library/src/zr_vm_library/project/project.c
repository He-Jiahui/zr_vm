//
// Created by HeJiahui on 2025/7/27.
//
#include "cJSON/cJSON.h"

#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define ZR_JSON_READ_STRING(STATE, OBJECT, NAME)                                                                       \
    SZrString *NAME = ZR_NULL;                                                                                         \
    {                                                                                                                  \
        cJSON *JSON_##NAME = cJSON_GetObjectItemCaseSensitive(OBJECT, #NAME);                                          \
        if (cJSON_IsString(JSON_##NAME) && JSON_##NAME->valuestring != ZR_NULL) {                                      \
            NAME = ZrCore_String_CreateTryHitCache(STATE, JSON_##NAME->valuestring);                                         \
        } else {                                                                                                       \
            NAME = ZR_NULL;                                                                                            \
        }                                                                                                              \
    }

typedef struct ZrLibraryProjectExecuteRequest {
    SZrFunction *function;
    TZrStackValuePointer resultBase;
    TZrBool callCompleted;
} ZrLibraryProjectExecuteRequest;

static void library_project_free_path_aliases(SZrGlobalState *global, SZrLibrary_Project *project);
static void library_project_free_dependencies(SZrGlobalState *global, SZrLibrary_Project *project);
static TZrBool library_project_validate_alias_key(const TZrChar *aliasKey);
static TZrBool library_project_validate_alias_module_prefix(const TZrChar *modulePrefix);
static TZrBool library_project_parse_path_aliases(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
static TZrBool library_project_parse_dependencies(SZrState *state,
                                                  SZrLibrary_Project *project,
                                                  cJSON *projectJson,
                                                  const TZrChar *ownerDirectory,
                                                  TZrSize ownerPackageIndex,
                                                  TZrBool ownerIsPackage);

static const TZrChar *library_project_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool zr_library_project_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void zr_library_project_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!zr_library_project_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-library-project] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}

static void library_project_free_path_alias_array(SZrGlobalState *global,
                                                  SZrLibrary_ProjectPathAlias *aliases,
                                                  TZrSize aliasCount) {
    TZrSize aliasBytes;

    if (global == ZR_NULL || aliases == ZR_NULL || aliasCount == 0) {
        return;
    }

    aliasBytes = sizeof(*aliases) * aliasCount;
    ZrCore_Memory_RawFreeWithType(global, aliases, aliasBytes, ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static void library_project_free_dependency_ref_array(SZrGlobalState *global,
                                                      SZrLibrary_ProjectDependencyReference *refs,
                                                      TZrSize capacity) {
    if (global == ZR_NULL || refs == ZR_NULL || capacity == 0) {
        return;
    }

    ZrCore_Memory_RawFreeWithType(global,
                                  refs,
                                  sizeof(*refs) * capacity,
                                  ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static void library_project_free_path_aliases(SZrGlobalState *global, SZrLibrary_Project *project) {
    if (project == ZR_NULL) {
        return;
    }

    library_project_free_path_alias_array(global, project->pathAliases, project->pathAliasCount);
    project->pathAliases = ZR_NULL;
    project->pathAliasCount = 0;
}

static void library_project_free_dependencies(SZrGlobalState *global, SZrLibrary_Project *project) {
    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < project->dependencyPackageCount; index++) {
        SZrLibrary_ProjectDependencyPackage *package = &project->dependencyPackages[index];
        library_project_free_path_alias_array(global, package->pathAliases, package->pathAliasCount);
        library_project_free_dependency_ref_array(global,
                                                  package->dependencyRefs,
                                                  package->dependencyRefCapacity);
        package->pathAliases = ZR_NULL;
        package->pathAliasCount = 0;
        package->dependencyRefs = ZR_NULL;
        package->dependencyRefCount = 0;
        package->dependencyRefCapacity = 0;
    }

    if (project->dependencyPackages != ZR_NULL && project->dependencyPackageCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->dependencyPackages,
                                      sizeof(*project->dependencyPackages) * project->dependencyPackageCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->dependencyPackages = ZR_NULL;
    project->dependencyPackageCount = 0;
    project->dependencyPackageCapacity = 0;

    library_project_free_dependency_ref_array(global,
                                              project->dependencyRefs,
                                              project->dependencyRefCapacity);
    project->dependencyRefs = ZR_NULL;
    project->dependencyRefCount = 0;
    project->dependencyRefCapacity = 0;
}

static TZrBool library_project_validate_alias_key(const TZrChar *aliasKey) {
    TZrSize index;

    if (aliasKey == ZR_NULL || aliasKey[0] != '@' || aliasKey[1] == '\0') {
        return ZR_FALSE;
    }

    for (index = 1; aliasKey[index] != '\0'; index++) {
        TZrChar current = aliasKey[index];
        if (current == '/' || current == '\\' || current == '.' || current == ' ' || current == '\t' ||
            current == '\r' || current == '\n') {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool library_project_validate_alias_module_prefix(const TZrChar *modulePrefix) {
    TZrSize segmentLength = 0;
    TZrSize index = 0;

    if (modulePrefix == ZR_NULL || modulePrefix[0] == '\0' || modulePrefix[0] == '@' || modulePrefix[0] == '.' ||
        modulePrefix[0] == '$' || modulePrefix[0] == '&') {
        return ZR_FALSE;
    }

    for (;;) {
        TZrChar current = modulePrefix[index++];
        if (current == '\0' || current == '/') {
            if (segmentLength == 0) {
                return ZR_FALSE;
            }
            if (segmentLength == 1 && modulePrefix[index - 2] == '.') {
                return ZR_FALSE;
            }
            if (segmentLength == 2 && modulePrefix[index - 3] == '.' && modulePrefix[index - 2] == '.') {
                return ZR_FALSE;
            }
            if (current == '\0') {
                return ZR_TRUE;
            }
            segmentLength = 0;
            continue;
        }
        segmentLength++;
    }
}

static TZrBool library_project_parse_path_alias_map(SZrState *state,
                                                    cJSON *pathAliasesJson,
                                                    SZrLibrary_ProjectPathAlias **outAliases,
                                                    TZrSize *outAliasCount) {
    cJSON *aliasEntry;
    TZrSize aliasCount = 0;
    TZrSize aliasIndex = 0;
    SZrGlobalState *global;

    if (outAliases != ZR_NULL) {
        *outAliases = ZR_NULL;
    }
    if (outAliasCount != ZR_NULL) {
        *outAliasCount = 0;
    }

    if (state == ZR_NULL || state->global == ZR_NULL || outAliases == ZR_NULL || outAliasCount == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    if (pathAliasesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(pathAliasesJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(aliasEntry, pathAliasesJson) {
        aliasCount++;
    }
    if (aliasCount == 0) {
        return ZR_TRUE;
    }

    *outAliases = (SZrLibrary_ProjectPathAlias *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(**outAliases) * aliasCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (*outAliases == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(*outAliases, 0, sizeof(**outAliases) * aliasCount);
    *outAliasCount = aliasCount;

    cJSON_ArrayForEach(aliasEntry, pathAliasesJson) {
        TZrChar normalizedPrefix[ZR_LIBRARY_MAX_PATH_LENGTH];
        TZrSize previousIndex;

        if (aliasEntry->string == ZR_NULL ||
            !cJSON_IsString(aliasEntry) ||
            aliasEntry->valuestring == ZR_NULL ||
            !library_project_validate_alias_key(aliasEntry->string) ||
            !ZrLibrary_Project_NormalizeModuleKey(aliasEntry->valuestring,
                                                  normalizedPrefix,
                                                  sizeof(normalizedPrefix)) ||
            !library_project_validate_alias_module_prefix(normalizedPrefix)) {
            return ZR_FALSE;
        }

        for (previousIndex = 0; previousIndex < aliasIndex; previousIndex++) {
            const TZrChar *existingAlias = library_project_string_text((*outAliases)[previousIndex].alias);
            if (existingAlias != ZR_NULL && strcmp(existingAlias, aliasEntry->string) == 0) {
                return ZR_FALSE;
            }
        }

        (*outAliases)[aliasIndex].alias =
                ZrCore_String_CreateTryHitCache(state, aliasEntry->string);
        (*outAliases)[aliasIndex].modulePrefix =
                ZrCore_String_CreateTryHitCache(state, normalizedPrefix);
        if ((*outAliases)[aliasIndex].alias == ZR_NULL ||
            (*outAliases)[aliasIndex].modulePrefix == ZR_NULL) {
            return ZR_FALSE;
        }
        aliasIndex++;
    }

    *outAliasCount = aliasIndex;
    return ZR_TRUE;
}

static TZrBool library_project_parse_path_aliases(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *pathAliasesJson;

    if (state == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    pathAliasesJson = cJSON_GetObjectItemCaseSensitive(projectJson, "pathAliases");
    return library_project_parse_path_alias_map(state,
                                                pathAliasesJson,
                                                &project->pathAliases,
                                                &project->pathAliasCount);
}

static TZrBool library_project_validate_dependency_name(const TZrChar *dependencyName) {
    TZrSize index;

    if (dependencyName == ZR_NULL || dependencyName[0] == '\0') {
        return ZR_FALSE;
    }

    for (index = 0; dependencyName[index] != '\0'; index++) {
        TZrChar current = dependencyName[index];
        if (current == '/' || current == '\\' || current == '.' || current == ' ' || current == '\t' ||
            current == '\r' || current == '\n' || current == '@' || current == '$') {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool library_project_validate_dependency_key(const TZrChar *dependencyKey) {
    return dependencyKey != ZR_NULL && dependencyKey[0] == '$' &&
           library_project_validate_dependency_name(dependencyKey + 1);
}

static TZrBool library_project_validate_dependency_version(const TZrChar *version) {
    TZrSize index;

    if (version == ZR_NULL || version[0] == '\0') {
        return ZR_FALSE;
    }

    for (index = 0; version[index] != '\0'; index++) {
        TZrChar current = version[index];
        if (current == '/' || current == '\\' || current == ' ' || current == '\t' ||
            current == '\r' || current == '\n') {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool library_project_resolve_manifest_path(const TZrChar *ownerDirectory,
                                                     const TZrChar *declaredPath,
                                                     TZrChar *buffer,
                                                     TZrSize bufferSize) {
    TZrChar joinedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (ownerDirectory == ZR_NULL || declaredPath == ZR_NULL || declaredPath[0] == '\0' ||
        buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    if (ZrLibrary_File_IsAbsolutePath(declaredPath)) {
        return ZrLibrary_File_NormalizePath(declaredPath, buffer, bufferSize);
    }

    ZrLibrary_File_PathJoin(ownerDirectory, declaredPath, joinedPath);
    return joinedPath[0] != '\0' && ZrLibrary_File_NormalizePath(joinedPath, buffer, bufferSize);
}

static TZrBool library_project_get_dependency_declaration(cJSON *dependencyEntry,
                                                          const TZrChar **outPath,
                                                          const TZrChar **outVersion) {
    cJSON *pathJson;
    cJSON *versionJson;

    if (outPath != ZR_NULL) {
        *outPath = ZR_NULL;
    }
    if (outVersion != ZR_NULL) {
        *outVersion = ZR_NULL;
    }
    if (dependencyEntry == ZR_NULL || outPath == ZR_NULL || outVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cJSON_IsString(dependencyEntry) && dependencyEntry->valuestring != ZR_NULL) {
        *outPath = dependencyEntry->valuestring;
        return ZR_TRUE;
    }

    if (!cJSON_IsObject(dependencyEntry)) {
        return ZR_FALSE;
    }

    pathJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "path");
    versionJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "version");
    if (!cJSON_IsString(pathJson) || pathJson->valuestring == ZR_NULL) {
        return ZR_FALSE;
    }

    *outPath = pathJson->valuestring;
    if (versionJson != ZR_NULL) {
        if (!cJSON_IsString(versionJson) || versionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(versionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outVersion = versionJson->valuestring;
    }
    return ZR_TRUE;
}

static TZrBool library_project_append_dependency_ref(SZrState *state,
                                                     SZrLibrary_Project *project,
                                                     TZrSize ownerPackageIndex,
                                                     TZrBool ownerIsPackage,
                                                     const TZrChar *name,
                                                     TZrSize packageIndex) {
    SZrGlobalState *global;
    SZrLibrary_ProjectDependencyReference **refs;
    TZrSize *refCount;
    TZrSize *refCapacity;
    TZrSize newCapacity;
    SZrLibrary_ProjectDependencyReference *newRefs;
    SZrLibrary_ProjectDependencyReference *slot;

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    if (ownerIsPackage) {
        if (ownerPackageIndex >= project->dependencyPackageCount) {
            return ZR_FALSE;
        }
        refs = &project->dependencyPackages[ownerPackageIndex].dependencyRefs;
        refCount = &project->dependencyPackages[ownerPackageIndex].dependencyRefCount;
        refCapacity = &project->dependencyPackages[ownerPackageIndex].dependencyRefCapacity;
    } else {
        refs = &project->dependencyRefs;
        refCount = &project->dependencyRefCount;
        refCapacity = &project->dependencyRefCapacity;
    }

    for (TZrSize index = 0; index < *refCount; index++) {
        const TZrChar *existingName = library_project_string_text((*refs)[index].name);
        if (existingName != ZR_NULL && strcmp(existingName, name) == 0) {
            return ZR_FALSE;
        }
    }

    if (*refCount == *refCapacity) {
        newCapacity = *refCapacity == 0 ? 4U : *refCapacity * 2U;
        newRefs = (SZrLibrary_ProjectDependencyReference *)ZrCore_Memory_Allocate(
                global,
                *refs,
                sizeof(**refs) * *refCapacity,
                sizeof(**refs) * newCapacity,
                ZR_MEMORY_NATIVE_TYPE_PROJECT);
        if (newRefs == ZR_NULL) {
            return ZR_FALSE;
        }
        if (newCapacity > *refCapacity) {
            memset(newRefs + *refCapacity, 0, sizeof(*newRefs) * (newCapacity - *refCapacity));
        }
        *refs = newRefs;
        *refCapacity = newCapacity;
    }

    slot = &(*refs)[*refCount];
    memset(slot, 0, sizeof(*slot));
    slot->name = ZrCore_String_CreateTryHitCache(state, name);
    slot->packageIndex = packageIndex;
    if (slot->name == ZR_NULL) {
        return ZR_FALSE;
    }
    (*refCount)++;
    return ZR_TRUE;
}

static TZrBool library_project_find_dependency_package(const SZrLibrary_Project *project,
                                                       const TZrChar *name,
                                                       const TZrChar *version,
                                                       TZrSize *outIndex) {
    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (project == ZR_NULL || name == ZR_NULL || version == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < project->dependencyPackageCount; index++) {
        const TZrChar *existingName = library_project_string_text(project->dependencyPackages[index].name);
        const TZrChar *existingVersion = library_project_string_text(project->dependencyPackages[index].version);
        if (existingName != ZR_NULL && existingVersion != ZR_NULL &&
            strcmp(existingName, name) == 0 && strcmp(existingVersion, version) == 0) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool library_project_append_dependency_package(SZrState *state,
                                                         SZrLibrary_Project *project,
                                                         TZrSize *outIndex) {
    SZrGlobalState *global;
    TZrSize newCapacity;
    SZrLibrary_ProjectDependencyPackage *newPackages;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    if (project->dependencyPackageCount == project->dependencyPackageCapacity) {
        newCapacity = project->dependencyPackageCapacity == 0 ? 4U : project->dependencyPackageCapacity * 2U;
        newPackages = (SZrLibrary_ProjectDependencyPackage *)ZrCore_Memory_Allocate(
                global,
                project->dependencyPackages,
                sizeof(*project->dependencyPackages) * project->dependencyPackageCapacity,
                sizeof(*project->dependencyPackages) * newCapacity,
                ZR_MEMORY_NATIVE_TYPE_PROJECT);
        if (newPackages == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(newPackages + project->dependencyPackageCapacity,
               0,
               sizeof(*newPackages) * (newCapacity - project->dependencyPackageCapacity));
        project->dependencyPackages = newPackages;
        project->dependencyPackageCapacity = newCapacity;
    }

    *outIndex = project->dependencyPackageCount;
    memset(&project->dependencyPackages[*outIndex], 0, sizeof(project->dependencyPackages[*outIndex]));
    project->dependencyPackageCount++;
    return ZR_TRUE;
}

static TZrBool library_project_parse_dependency_package_fields(SZrState *state,
                                                               SZrLibrary_ProjectDependencyPackage *package,
                                                               cJSON *manifestJson,
                                                               const TZrChar *name,
                                                               const TZrChar *version,
                                                               const TZrChar *manifestPath,
                                                               const TZrChar *directory) {
    cJSON *sourceJson;
    cJSON *binaryJson;
    cJSON *entryJson;
    cJSON *pathAliasesJson;

    if (state == ZR_NULL || package == ZR_NULL || manifestJson == ZR_NULL || name == ZR_NULL || version == ZR_NULL ||
        manifestPath == ZR_NULL || directory == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "source");
    binaryJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "binary");
    entryJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "entry");
    if (!cJSON_IsString(sourceJson) || sourceJson->valuestring == ZR_NULL || sourceJson->valuestring[0] == '\0' ||
        !cJSON_IsString(binaryJson) || binaryJson->valuestring == ZR_NULL || binaryJson->valuestring[0] == '\0' ||
        !cJSON_IsString(entryJson) || entryJson->valuestring == ZR_NULL || entryJson->valuestring[0] == '\0') {
        return ZR_FALSE;
    }
    {
        TZrChar normalizedEntry[ZR_LIBRARY_MAX_PATH_LENGTH];
        if (!ZrLibrary_Project_NormalizeModuleKey(entryJson->valuestring,
                                                  normalizedEntry,
                                                  sizeof(normalizedEntry))) {
            return ZR_FALSE;
        }
    }

    if (!library_project_validate_dependency_name(name) ||
        !library_project_validate_dependency_version(version)) {
        return ZR_FALSE;
    }

    package->name = ZrCore_String_CreateTryHitCache(state, name);
    package->version = ZrCore_String_CreateTryHitCache(state, version);
    package->file = ZrCore_String_CreateTryHitCache(state, manifestPath);
    package->directory = ZrCore_String_CreateTryHitCache(state, directory);
    package->source = ZrCore_String_CreateTryHitCache(state, sourceJson->valuestring);
    package->binary = ZrCore_String_CreateTryHitCache(state, binaryJson->valuestring);
    package->entry = ZrCore_String_CreateTryHitCache(state, entryJson->valuestring);
    if (package->name == ZR_NULL || package->version == ZR_NULL || package->file == ZR_NULL ||
        package->directory == ZR_NULL || package->source == ZR_NULL || package->binary == ZR_NULL ||
        package->entry == ZR_NULL) {
        return ZR_FALSE;
    }

    pathAliasesJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "pathAliases");
    return library_project_parse_path_alias_map(state,
                                                pathAliasesJson,
                                                &package->pathAliases,
                                                &package->pathAliasCount);
}

static TZrBool library_project_parse_dependency_entry(SZrState *state,
                                                      SZrLibrary_Project *project,
                                                      cJSON *dependencyEntry,
                                                      const TZrChar *ownerDirectory,
                                                      TZrSize ownerPackageIndex,
                                                      TZrBool ownerIsPackage) {
    const TZrChar *declaredPath;
    const TZrChar *declaredVersion;
    const TZrChar *dependencyName;
    TZrChar manifestPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar manifestDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString manifestText;
    TZrSize manifestLength;
    cJSON *manifestJson;
    cJSON *manifestNameJson;
    cJSON *manifestVersionJson;
    const TZrChar *manifestName;
    const TZrChar *manifestVersion;
    const TZrChar *packageName;
    const TZrChar *effectiveVersion;
    TZrSize packageIndex;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || project == ZR_NULL || dependencyEntry == ZR_NULL || ownerDirectory == ZR_NULL ||
        dependencyEntry->string == ZR_NULL || !library_project_validate_dependency_key(dependencyEntry->string) ||
        !library_project_get_dependency_declaration(dependencyEntry, &declaredPath, &declaredVersion) ||
        !library_project_resolve_manifest_path(ownerDirectory, declaredPath, manifestPath, sizeof(manifestPath)) ||
        ZrLibrary_File_Exist(manifestPath) != ZR_LIBRARY_FILE_IS_FILE ||
        !ZrLibrary_File_GetDirectory(manifestPath, manifestDirectory)) {
        return ZR_FALSE;
    }

    dependencyName = dependencyEntry->string + 1;
    manifestText = ZrLibrary_File_ReadAll(state->global, manifestPath);
    if (manifestText == ZR_NULL) {
        return ZR_FALSE;
    }

    manifestLength = strlen(manifestText);
    manifestJson = cJSON_Parse(manifestText);
    if (manifestJson == ZR_NULL) {
        goto cleanup_text;
    }

    manifestNameJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "name");
    manifestVersionJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "version");
    if (manifestNameJson != ZR_NULL) {
        if (!cJSON_IsString(manifestNameJson) || manifestNameJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_name(manifestNameJson->valuestring)) {
            goto cleanup_json;
        }
        manifestName = manifestNameJson->valuestring;
    } else {
        manifestName = ZR_NULL;
    }
    manifestVersion = cJSON_IsString(manifestVersionJson) && manifestVersionJson->valuestring != ZR_NULL
                    ? manifestVersionJson->valuestring
                    : ZR_NULL;
    if (manifestVersionJson != ZR_NULL &&
        (manifestVersion == ZR_NULL || !library_project_validate_dependency_version(manifestVersion))) {
        goto cleanup_json;
    }
    if (manifestVersion != ZR_NULL && declaredVersion != ZR_NULL && strcmp(manifestVersion, declaredVersion) != 0) {
        goto cleanup_json;
    }
    packageName = manifestName != ZR_NULL ? manifestName : dependencyName;
    effectiveVersion = manifestVersion != ZR_NULL ? manifestVersion : (declaredVersion != ZR_NULL ? declaredVersion : "0.0.0");

    if (!library_project_find_dependency_package(project, packageName, effectiveVersion, &packageIndex)) {
        SZrLibrary_ProjectDependencyPackage *package;

        if (!library_project_append_dependency_package(state, project, &packageIndex)) {
            goto cleanup_json;
        }
        package = &project->dependencyPackages[packageIndex];
        if (!library_project_parse_dependency_package_fields(state,
                                                             package,
                                                             manifestJson,
                                                             packageName,
                                                             effectiveVersion,
                                                             manifestPath,
                                                             manifestDirectory) ||
            !library_project_parse_dependencies(state,
                                                project,
                                                manifestJson,
                                                manifestDirectory,
                                                packageIndex,
                                                ZR_TRUE)) {
            goto cleanup_json;
        }
    }

    success = library_project_append_dependency_ref(state,
                                                    project,
                                                    ownerPackageIndex,
                                                    ownerIsPackage,
                                                    dependencyName,
                                                    packageIndex);

cleanup_json:
    cJSON_Delete(manifestJson);
cleanup_text:
    ZrCore_Memory_RawFreeWithType(state->global,
                                  manifestText,
                                  manifestLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return success;
}

static TZrBool library_project_parse_dependencies(SZrState *state,
                                                  SZrLibrary_Project *project,
                                                  cJSON *projectJson,
                                                  const TZrChar *ownerDirectory,
                                                  TZrSize ownerPackageIndex,
                                                  TZrBool ownerIsPackage) {
    cJSON *dependenciesJson;
    cJSON *dependencyEntry;

    if (state == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL || ownerDirectory == ZR_NULL) {
        return ZR_FALSE;
    }

    dependenciesJson = cJSON_GetObjectItemCaseSensitive(projectJson, "dependencies");
    if (dependenciesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(dependenciesJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(dependencyEntry, dependenciesJson) {
        if (!library_project_parse_dependency_entry(state,
                                                    project,
                                                    dependencyEntry,
                                                    ownerDirectory,
                                                    ownerPackageIndex,
                                                    ownerIsPackage)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file) {
    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project == ZR_NULL) {
        return ZR_NULL;
    }
    memset(project, 0, sizeof(SZrLibrary_Project));
    project->signature = ZR_LIBRARY_PROJECT_SIGNATURE;

    cJSON *projectJson = cJSON_Parse(raw);
    if (projectJson == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }

    project->file = ZrCore_String_CreateTryHitCache(state, file);

    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool success = ZrLibrary_File_GetDirectory(file, path);
    if (!success) {
        cJSON_Delete(projectJson);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    project->directory = ZrCore_String_CreateTryHitCache(state, path);
    // cJSON *name = cJSON_GetObjectItemCaseSensitive(projectJson, "name");
    // if (cJSON_IsString(name) && name->valuestring != ZR_NULL) {
    //     project->name = ZrCore_String_CreateTryHitCache(state, name->valuestring);
    // } else {
    //     project->name = ZR_NULL;
    // }
    ZR_JSON_READ_STRING(state, projectJson, name);
    project->name = name;

    ZR_JSON_READ_STRING(state, projectJson, version);
    project->version = version;

    ZR_JSON_READ_STRING(state, projectJson, description);
    project->description = description;

    ZR_JSON_READ_STRING(state, projectJson, author);
    project->author = author;

    ZR_JSON_READ_STRING(state, projectJson, email);
    project->email = email;

    ZR_JSON_READ_STRING(state, projectJson, url);
    project->url = url;

    ZR_JSON_READ_STRING(state, projectJson, license);
    project->license = license;

    ZR_JSON_READ_STRING(state, projectJson, copyright);
    project->copyright = copyright;

    ZR_JSON_READ_STRING(state, projectJson, binary);
    project->binary = binary;

    ZR_JSON_READ_STRING(state, projectJson, source);
    project->source = source;

    ZR_JSON_READ_STRING(state, projectJson, entry);
    project->entry = entry;

    ZR_JSON_READ_STRING(state, projectJson, dependency);
    project->dependency = dependency;

    ZR_JSON_READ_STRING(state, projectJson, local);
    project->local = local;
    if (!library_project_parse_path_aliases(state, project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_dependencies(state, project, projectJson, path, 0, ZR_FALSE)) {
        cJSON_Delete(projectJson);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    {
        cJSON *supportMultithread = cJSON_GetObjectItemCaseSensitive(projectJson, "supportMultithread");
        cJSON *autoCoroutine = cJSON_GetObjectItemCaseSensitive(projectJson, "autoCoroutine");

        project->supportMultithread =
                cJSON_IsBool(supportMultithread) ? (cJSON_IsTrue(supportMultithread) ? ZR_TRUE : ZR_FALSE) : ZR_FALSE;
        project->autoCoroutine =
                cJSON_IsBool(autoCoroutine) ? (cJSON_IsTrue(autoCoroutine) ? ZR_TRUE : ZR_FALSE) : ZR_TRUE;
    }
    cJSON_Delete(projectJson);

    if (project->source == ZR_NULL || project->binary == ZR_NULL || project->entry == ZR_NULL) {
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }

    return project;
}

void ZrLibrary_Project_Free(SZrState *state, SZrLibrary_Project *project) {
    if (project == ZR_NULL) {
        return;
    }
    SZrGlobalState *global = state->global;
    library_project_free_dependencies(global, project);
    library_project_free_path_aliases(global, project);
    ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static TZrBool library_project_resolve_source_path(const SZrLibrary_Project *project, TZrNativeString modulePath,
                                                  TZrChar *resolvedPath) {
    return ZrLibrary_Project_ResolveSourcePath(project, modulePath, resolvedPath, ZR_LIBRARY_MAX_PATH_LENGTH);
}

static TZrBool library_project_resolve_binary_path(const SZrLibrary_Project *project, TZrNativeString modulePath,
                                                  TZrChar *resolvedPath) {
    return ZrLibrary_Project_ResolveBinaryPath(project, modulePath, resolvedPath, ZR_LIBRARY_MAX_PATH_LENGTH);
}

static TZrBool library_project_load_resolved_file(SZrState *state, TZrNativeString filePath, TZrBool isBinary, SZrIo *io) {
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(state->global, filePath, isBinary);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    io->isBinary = isBinary;
    return ZR_TRUE;
}

static EZrThreadStatus library_project_normalize_failure(SZrState *state, EZrThreadStatus status) {
    EZrThreadStatus effectiveStatus;

    if (state == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    effectiveStatus = status;
    if (effectiveStatus == ZR_THREAD_STATUS_FINE && state->threadStatus != ZR_THREAD_STATUS_FINE) {
        effectiveStatus = state->threadStatus;
    }
    if (effectiveStatus == ZR_THREAD_STATUS_FINE) {
        effectiveStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    if (!state->hasCurrentException) {
        (void)ZrCore_Exception_NormalizeStatus(state, effectiveStatus);
    }
    state->threadStatus = effectiveStatus;
    return effectiveStatus;
}

static void library_project_execute_body(SZrState *state, TZrPtr arguments) {
    ZrLibraryProjectExecuteRequest *request = (ZrLibraryProjectExecuteRequest *)arguments;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor anchor;
    SZrTypeValue *closureValue;

    if (state == ZR_NULL || request == ZR_NULL || request->function == ZR_NULL) {
        return;
    }

    zr_library_project_trace("execute body start function=%p stackTop=%p stackSize=%llu",
                             (void *)request->function,
                             (void *)state->stackTop.valuePointer,
                             (unsigned long long)request->function->stackSize);
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, request->function->stackSize + 1, base, base, &anchor);
    zr_library_project_trace("execute body anchored base=%p stackTop=%p",
                             (void *)base,
                             (void *)state->stackTop.valuePointer);

    ZrCore_Closure_PushToStack(state, request->function, ZR_NULL, base, state->stackTop.valuePointer);
    closureValue = ZrCore_Stack_GetValue(state->stackTop.valuePointer);
    if (closureValue == ZR_NULL || closureValue->value.object == ZR_NULL) {
        zr_library_project_trace("execute body closure push failed slot=%p",
                                 (void *)state->stackTop.valuePointer);
        return;
    }
    zr_library_project_trace("execute body closure slot=%p object=%p rawType=%d",
                             (void *)state->stackTop.valuePointer,
                             (void *)closureValue->value.object,
                             (int)closureValue->value.object->type);
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    zr_library_project_trace("execute body call start slot=%p stackTop=%p",
                             (void *)(state->stackTop.valuePointer - 1),
                             (void *)state->stackTop.valuePointer);
    request->resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &anchor, 1);
    zr_library_project_trace("execute body call done resultBase=%p threadStatus=%d",
                             (void *)request->resultBase,
                             (int)state->threadStatus);
    request->callCompleted = (TZrBool)(state->threadStatus == ZR_THREAD_STATUS_FINE);
}

static EZrThreadStatus library_project_execute_function(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    ZrLibraryProjectExecuteRequest request;
    EZrThreadStatus status;

    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    memset(&request, 0, sizeof(request));
    ZrCore_Value_ResetAsNull(result);
    request.function = function;
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    zr_library_project_trace("execute function enter function=%p", (void *)function);
    status = ZrCore_Exception_TryRun(state, library_project_execute_body, &request);
    zr_library_project_trace("execute function tryrun status=%d requestResult=%p callCompleted=%d threadStatus=%d",
                             (int)status,
                             (void *)request.resultBase,
                             (int)request.callCompleted,
                             (int)state->threadStatus);
    if (status != ZR_THREAD_STATUS_FINE) {
        return library_project_normalize_failure(state, status);
    }

    if (!request.callCompleted || request.resultBase == ZR_NULL) {
        return library_project_normalize_failure(state, state->threadStatus);
    }

    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(request.resultBase));
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    return ZR_THREAD_STATUS_FINE;
}

EZrThreadStatus ZrLibrary_Project_Run(SZrState *state, SZrTypeValue *result) {
    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return ZR_THREAD_STATUS_RUNTIME_ERROR;
    }

    SZrGlobalState *global = state->global;
    const SZrLibrary_Project *project = ZrLibrary_Project_GetFromGlobal(global);
    ZrCore_Value_ResetAsNull(result);

    if (project == ZR_NULL || project->entry == ZR_NULL || global->compileSource == ZR_NULL) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }
    zr_library_project_trace("run entry='%s'",
                             ZrCore_String_GetNativeString(project->entry));

    TZrChar entrySourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (!library_project_resolve_source_path(project, ZrCore_String_GetNativeString(project->entry), entrySourcePath)) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }
    zr_library_project_trace("resolved entry source='%s'", entrySourcePath);

    TZrNativeString sourceCode = ZrLibrary_File_ReadAll(global, entrySourcePath);
    zr_library_project_trace("read source=%p", (void *)sourceCode);
    if (sourceCode == ZR_NULL) {
        state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        return state->threadStatus;
    }

    TZrSize sourceLength = ZrCore_NativeString_Length(sourceCode);
    SZrString *sourceName = ZrCore_String_Create(state, entrySourcePath, ZrCore_NativeString_Length(entrySourcePath));
    zr_library_project_trace("compile source length=%llu sourceName=%p",
                             (unsigned long long)sourceLength,
                             (void *)sourceName);
    SZrFunction *function = global->compileSource(state, sourceCode, sourceLength, sourceName);
    TZrBool ignoredFunction = ZR_FALSE;
    EZrThreadStatus status;
    zr_library_project_trace("compiled function=%p threadStatus=%d", (void *)function, (int)state->threadStatus);
    ZrCore_Memory_RawFreeWithType(global, sourceCode, sourceLength + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);

    if (function == ZR_NULL) {
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            state->threadStatus = ZR_THREAD_STATUS_RUNTIME_ERROR;
        }
        return library_project_normalize_failure(state, state->threadStatus);
    }

    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(state->global,
                                                          state,
                                                          ZR_CAST_RAW_OBJECT_AS_SUPER(function),
                                                          &ignoredFunction)) {
        return library_project_normalize_failure(state, state->threadStatus);
    }

    SZrObjectModule *projectModule = ZrCore_Module_Create(state);
    zr_library_project_trace("project module=%p", (void *)projectModule);
    if (projectModule != ZR_NULL) {
        TZrUInt64 pathHash = ZrCore_Module_CalculatePathHash(state, sourceName);
        ZrCore_Module_SetInfo(state, projectModule, ZR_NULL, pathHash, sourceName);
        ZrCore_Reflection_AttachModuleRuntimeMetadata(state, projectModule, function);
        ZrCore_Module_CreatePrototypesFromConstants(state, projectModule, function);
        zr_library_project_trace("project module prepared pathHash=%llu", (unsigned long long)pathHash);
    }

    zr_library_project_trace("execute compiled function");
    status = library_project_execute_function(state, function, result);
    zr_library_project_trace("execute finished status=%d threadStatus=%d", (int)status, (int)state->threadStatus);
    if (ignoredFunction) {
        ZrCore_GarbageCollector_UnignoreObject(global, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    }
    return status;
}

void ZrLibrary_Project_Do(SZrState *state) {
    SZrTypeValue ignoredResult;
    ZrCore_Value_ResetAsNull(&ignoredResult);
    ZrLibrary_Project_Run(state, &ignoredResult);
}

TZrBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || state->global == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    const SZrLibrary_Project *project = ZrLibrary_Project_GetFromGlobal(state->global);
    if (project == ZR_NULL || path == ZR_NULL) {
        return ZR_FALSE;
    }

    TZrChar fullPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    if (library_project_resolve_source_path(project, path, fullPath) &&
        ZrLibrary_File_Exist(fullPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return library_project_load_resolved_file(state, fullPath, ZR_FALSE, io);
    }

    if (library_project_resolve_binary_path(project, path, fullPath) &&
        ZrLibrary_File_Exist(fullPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return library_project_load_resolved_file(state, fullPath, ZR_TRUE, io);
    }

    return ZR_FALSE;
}
