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
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_library/zrm.h"

#include "project/project_aot_options.h"
#include "project/project_features.h"
#include "project/project_preserve.h"

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

typedef struct SZrLibrary_ProjectMemoryReader {
    TZrByte *bytes;
    TZrSize byteCount;
    TZrSize offset;
} SZrLibrary_ProjectMemoryReader;

static void library_project_free_path_aliases(SZrGlobalState *global, SZrLibrary_Project *project);
static void library_project_free_dependencies(SZrGlobalState *global, SZrLibrary_Project *project);
static void library_project_free_resources(SZrGlobalState *global, SZrLibrary_Project *project);
static TZrBool library_project_validate_alias_key(const TZrChar *aliasKey);
static TZrBool library_project_validate_alias_module_prefix(const TZrChar *modulePrefix);
static TZrBool library_project_parse_path_aliases(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
static TZrBool library_project_parse_resources(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
static TZrBool library_project_parse_dependencies(SZrState *state,
                                                  SZrLibrary_Project *project,
                                                  cJSON *projectJson,
                                                  const TZrChar *ownerDirectory,
                                                  TZrSize ownerPackageIndex,
                                                  TZrBool ownerIsPackage);
static TZrBool library_project_parse_references(SZrState *state,
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

static TZrBool library_project_optional_string_matches(SZrString *value, const TZrChar *text) {
    const TZrChar *valueText = library_project_string_text(value);

    if (valueText == ZR_NULL) {
        return text == ZR_NULL;
    }

    return text != ZR_NULL && strcmp(valueText, text) == 0;
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

static void library_project_free_resources(SZrGlobalState *global, SZrLibrary_Project *project) {
    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    if (project->resources != ZR_NULL && project->resourceCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->resources,
                                      sizeof(*project->resources) * project->resourceCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->resources = ZR_NULL;
    project->resourceCount = 0;
    project->resourceCapacity = 0;
}

static void library_project_free_dependencies(SZrGlobalState *global, SZrLibrary_Project *project) {
    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < project->dependencyPackageCount; index++) {
        SZrLibrary_ProjectDependencyPackage *package = &project->dependencyPackages[index];
        if (package->zrmArchiveOpen) {
            ZrLibrary_Zrm_Close(&package->zrmArchive);
            package->zrmArchiveOpen = ZR_FALSE;
        }
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

static TZrBool library_project_has_suffix(const TZrChar *text, const TZrChar *suffix) {
    TZrSize textLength;
    TZrSize suffixLength;

    if (text == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }
    textLength = strlen(text);
    suffixLength = strlen(suffix);
    return textLength >= suffixLength &&
           memcmp(text + textLength - suffixLength, suffix, suffixLength) == 0;
}

static TZrBool library_project_parse_resource_entry_value(cJSON *resourceEntry,
                                                          const TZrChar **outSourcePath,
                                                          TZrBool *outCompress) {
    cJSON *pathJson;
    cJSON *compressJson;

    if (outSourcePath != ZR_NULL) {
        *outSourcePath = ZR_NULL;
    }
    if (outCompress != ZR_NULL) {
        *outCompress = ZR_TRUE;
    }
    if (resourceEntry == ZR_NULL || outSourcePath == ZR_NULL || outCompress == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cJSON_IsString(resourceEntry) && resourceEntry->valuestring != ZR_NULL &&
        resourceEntry->valuestring[0] != '\0') {
        *outSourcePath = resourceEntry->valuestring;
        *outCompress = ZR_TRUE;
        return ZR_TRUE;
    }

    if (!cJSON_IsObject(resourceEntry)) {
        return ZR_FALSE;
    }

    pathJson = cJSON_GetObjectItemCaseSensitive(resourceEntry, "path");
    compressJson = cJSON_GetObjectItemCaseSensitive(resourceEntry, "compress");
    if (!cJSON_IsString(pathJson) || pathJson->valuestring == ZR_NULL || pathJson->valuestring[0] == '\0') {
        return ZR_FALSE;
    }
    if (compressJson != ZR_NULL && !cJSON_IsBool(compressJson)) {
        return ZR_FALSE;
    }

    *outSourcePath = pathJson->valuestring;
    *outCompress = compressJson == ZR_NULL || cJSON_IsTrue(compressJson) ? ZR_TRUE : ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool library_project_parse_resources(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *resourcesJson;
    cJSON *resourceEntry;
    TZrSize resourceCount = 0;
    TZrSize resourceIndex = 0;
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    resourcesJson = cJSON_GetObjectItemCaseSensitive(projectJson, "resources");
    if (resourcesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(resourcesJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(resourceEntry, resourcesJson) {
        resourceCount++;
    }
    if (resourceCount == 0) {
        return ZR_TRUE;
    }

    global = state->global;
    project->resources = (SZrLibrary_ProjectResource *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(*project->resources) * resourceCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->resources == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->resources, 0, sizeof(*project->resources) * resourceCount);
    project->resourceCapacity = resourceCount;

    cJSON_ArrayForEach(resourceEntry, resourcesJson) {
        const TZrChar *sourcePath;
        TZrBool compress;

        if (resourceEntry->string == ZR_NULL ||
            !ZrLibrary_Zrm_ValidateLogicalName(resourceEntry->string) ||
            !library_project_parse_resource_entry_value(resourceEntry, &sourcePath, &compress)) {
            return ZR_FALSE;
        }

        for (TZrSize previousIndex = 0; previousIndex < resourceIndex; previousIndex++) {
            const TZrChar *existing = library_project_string_text(project->resources[previousIndex].logicalName);
            if (existing != ZR_NULL && strcmp(existing, resourceEntry->string) == 0) {
                return ZR_FALSE;
            }
        }

        project->resources[resourceIndex].logicalName =
                ZrCore_String_CreateTryHitCache(state, resourceEntry->string);
        project->resources[resourceIndex].sourcePath =
                ZrCore_String_CreateTryHitCache(state, (TZrNativeString)sourcePath);
        project->resources[resourceIndex].compress = compress;
        if (project->resources[resourceIndex].logicalName == ZR_NULL ||
            project->resources[resourceIndex].sourcePath == ZR_NULL) {
            return ZR_FALSE;
        }
        resourceIndex++;
    }

    project->resourceCount = resourceIndex;
    return ZR_TRUE;
}

static TZrBool library_project_get_manifest_assembly_output(cJSON *manifestJson, const TZrChar **outOutput) {
    cJSON *assemblyJson;
    cJSON *outputJson;

    if (outOutput != ZR_NULL) {
        *outOutput = ZR_NULL;
    }
    if (manifestJson == ZR_NULL || outOutput == ZR_NULL) {
        return ZR_FALSE;
    }

    assemblyJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "assembly");
    if (assemblyJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(assemblyJson)) {
        return ZR_FALSE;
    }

    outputJson = cJSON_GetObjectItemCaseSensitive(assemblyJson, "output");
    if (outputJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsString(outputJson) || outputJson->valuestring == ZR_NULL ||
        outputJson->valuestring[0] == '\0' ||
        !library_project_has_suffix(outputJson->valuestring, ZR_LIBRARY_ZRM_FILE_EXTENSION)) {
        return ZR_FALSE;
    }

    *outOutput = outputJson->valuestring;
    return ZR_TRUE;
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

static TZrBool library_project_validate_assembly_name(const TZrChar *assemblyName) {
    TZrSize index;
    TZrBool previousWasDot = ZR_TRUE;

    if (assemblyName == ZR_NULL || assemblyName[0] == '\0') {
        return ZR_FALSE;
    }

    for (index = 0; assemblyName[index] != '\0'; index++) {
        TZrChar current = assemblyName[index];
        if (current == '/' || current == '\\' || current == ' ' || current == '\t' ||
            current == '\r' || current == '\n' || current == '@' || current == '$') {
            return ZR_FALSE;
        }
        if (current == '.') {
            if (previousWasDot) {
                return ZR_FALSE;
            }
            previousWasDot = ZR_TRUE;
            continue;
        }
        previousWasDot = ZR_FALSE;
    }

    return !previousWasDot;
}

static TZrBool library_project_validate_manifest_version(cJSON *manifestJson) {
    cJSON *manifestVersionJson;

    if (manifestJson == ZR_NULL) {
        return ZR_FALSE;
    }

    manifestVersionJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "manifestVersion");
    if (manifestVersionJson == ZR_NULL) {
        return ZR_TRUE;
    }

    return cJSON_IsNumber(manifestVersionJson) &&
           manifestVersionJson->valueint == 1 &&
           manifestVersionJson->valuedouble == 1.0;
}

static TZrBool library_project_normalize_public_key_token(TZrChar *publicKeyToken) {
    TZrSize index;

    if (publicKeyToken == ZR_NULL || publicKeyToken[0] == '\0') {
        return ZR_FALSE;
    }

    for (index = 0; publicKeyToken[index] != '\0'; index++) {
        TZrChar current = publicKeyToken[index];
        if (current >= '0' && current <= '9') {
            continue;
        }
        if (current >= 'a' && current <= 'f') {
            continue;
        }
        if (current >= 'A' && current <= 'F') {
            publicKeyToken[index] = (TZrChar)(current - 'A' + 'a');
            continue;
        }
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool library_project_get_manifest_assembly_identity(cJSON *manifestJson,
                                                              const TZrChar **outName,
                                                              const TZrChar **outVersion,
                                                              const TZrChar **outCulture,
                                                              const TZrChar **outPublicKeyToken,
                                                              const TZrChar **outKind) {
    cJSON *assemblyJson;
    cJSON *assemblyNameJson;
    cJSON *assemblyVersionJson;
    cJSON *cultureJson;
    cJSON *publicKeyTokenJson;
    cJSON *kindJson;
    cJSON *legacyNameJson;
    cJSON *legacyVersionJson;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (outVersion != ZR_NULL) {
        *outVersion = ZR_NULL;
    }
    if (outCulture != ZR_NULL) {
        *outCulture = "neutral";
    }
    if (outPublicKeyToken != ZR_NULL) {
        *outPublicKeyToken = ZR_NULL;
    }
    if (outKind != ZR_NULL) {
        *outKind = "library";
    }
    if (manifestJson == ZR_NULL || outName == ZR_NULL || outVersion == ZR_NULL ||
        outCulture == ZR_NULL || outPublicKeyToken == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    assemblyJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "assembly");
    if (assemblyJson != ZR_NULL) {
        if (!cJSON_IsObject(assemblyJson)) {
            return ZR_FALSE;
        }
        assemblyNameJson = cJSON_GetObjectItemCaseSensitive(assemblyJson, "name");
        assemblyVersionJson = cJSON_GetObjectItemCaseSensitive(assemblyJson, "version");
        cultureJson = cJSON_GetObjectItemCaseSensitive(assemblyJson, "culture");
        publicKeyTokenJson = cJSON_GetObjectItemCaseSensitive(assemblyJson, "publicKeyToken");
        kindJson = cJSON_GetObjectItemCaseSensitive(assemblyJson, "kind");

        if (!cJSON_IsString(assemblyNameJson) || assemblyNameJson->valuestring == ZR_NULL ||
            !library_project_validate_assembly_name(assemblyNameJson->valuestring)) {
            return ZR_FALSE;
        }
        *outName = assemblyNameJson->valuestring;
        if (assemblyVersionJson != ZR_NULL) {
            if (!cJSON_IsString(assemblyVersionJson) || assemblyVersionJson->valuestring == ZR_NULL ||
                !library_project_validate_dependency_version(assemblyVersionJson->valuestring)) {
                return ZR_FALSE;
            }
            *outVersion = assemblyVersionJson->valuestring;
        }
        if (cultureJson != ZR_NULL) {
            if (!cJSON_IsString(cultureJson) || cultureJson->valuestring == ZR_NULL ||
                cultureJson->valuestring[0] == '\0') {
                return ZR_FALSE;
            }
            *outCulture = cultureJson->valuestring;
        }
        if (publicKeyTokenJson != ZR_NULL && !cJSON_IsNull(publicKeyTokenJson)) {
            if (!cJSON_IsString(publicKeyTokenJson) || publicKeyTokenJson->valuestring == ZR_NULL ||
                !library_project_normalize_public_key_token(publicKeyTokenJson->valuestring)) {
                return ZR_FALSE;
            }
            *outPublicKeyToken = publicKeyTokenJson->valuestring;
        }
        if (kindJson != ZR_NULL) {
            if (!cJSON_IsString(kindJson) || kindJson->valuestring == ZR_NULL ||
                (strcmp(kindJson->valuestring, "library") != 0 &&
                 strcmp(kindJson->valuestring, "application") != 0)) {
                return ZR_FALSE;
            }
            *outKind = kindJson->valuestring;
        }
    }

    legacyNameJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "name");
    legacyVersionJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "version");
    if (*outName == ZR_NULL && legacyNameJson != ZR_NULL) {
        if (!cJSON_IsString(legacyNameJson) || legacyNameJson->valuestring == ZR_NULL ||
            !library_project_validate_assembly_name(legacyNameJson->valuestring)) {
            return ZR_FALSE;
        }
        *outName = legacyNameJson->valuestring;
    }
    if (*outVersion == ZR_NULL && legacyVersionJson != ZR_NULL) {
        if (!cJSON_IsString(legacyVersionJson) || legacyVersionJson->valuestring == ZR_NULL) {
            return ZR_FALSE;
        }
        if (!library_project_validate_dependency_version(legacyVersionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outVersion = legacyVersionJson->valuestring;
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
                                                          const TZrChar **outAssemblyName,
                                                          const TZrChar **outPath,
                                                          const TZrChar **outVersion,
                                                          const TZrChar **outMinVersionInclusive,
                                                          const TZrChar **outMaxVersionExclusive) {
    cJSON *assemblyJson;
    cJSON *nameJson;
    cJSON *pathJson;
    cJSON *versionJson;
    cJSON *minVersionJson;
    cJSON *maxVersionJson;

    if (outAssemblyName != ZR_NULL) {
        *outAssemblyName = ZR_NULL;
    }
    if (outPath != ZR_NULL) {
        *outPath = ZR_NULL;
    }
    if (outVersion != ZR_NULL) {
        *outVersion = ZR_NULL;
    }
    if (outMinVersionInclusive != ZR_NULL) {
        *outMinVersionInclusive = ZR_NULL;
    }
    if (outMaxVersionExclusive != ZR_NULL) {
        *outMaxVersionExclusive = ZR_NULL;
    }
    if (dependencyEntry == ZR_NULL || outAssemblyName == ZR_NULL || outPath == ZR_NULL ||
        outVersion == ZR_NULL || outMinVersionInclusive == ZR_NULL || outMaxVersionExclusive == ZR_NULL) {
        return ZR_FALSE;
    }

    if (cJSON_IsString(dependencyEntry) && dependencyEntry->valuestring != ZR_NULL) {
        *outPath = dependencyEntry->valuestring;
        return ZR_TRUE;
    }

    if (!cJSON_IsObject(dependencyEntry)) {
        return ZR_FALSE;
    }

    assemblyJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "assembly");
    nameJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "name");
    pathJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "path");
    versionJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "version");
    minVersionJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "minVersionInclusive");
    maxVersionJson = cJSON_GetObjectItemCaseSensitive(dependencyEntry, "maxVersionExclusive");
    if (!cJSON_IsString(pathJson) || pathJson->valuestring == ZR_NULL) {
        return ZR_FALSE;
    }

    if (assemblyJson != ZR_NULL) {
        if (!cJSON_IsString(assemblyJson) || assemblyJson->valuestring == ZR_NULL ||
            !library_project_validate_assembly_name(assemblyJson->valuestring)) {
            return ZR_FALSE;
        }
        *outAssemblyName = assemblyJson->valuestring;
    }
    if (nameJson != ZR_NULL) {
        if (!cJSON_IsString(nameJson) || nameJson->valuestring == ZR_NULL ||
            !library_project_validate_assembly_name(nameJson->valuestring)) {
            return ZR_FALSE;
        }
        if (*outAssemblyName != ZR_NULL && strcmp(*outAssemblyName, nameJson->valuestring) != 0) {
            return ZR_FALSE;
        }
        *outAssemblyName = nameJson->valuestring;
    }
    *outPath = pathJson->valuestring;
    if (versionJson != ZR_NULL) {
        if (!cJSON_IsString(versionJson) || versionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(versionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outVersion = versionJson->valuestring;
    }
    if (minVersionJson != ZR_NULL) {
        if (!cJSON_IsString(minVersionJson) || minVersionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(minVersionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outMinVersionInclusive = minVersionJson->valuestring;
    }
    if (maxVersionJson != ZR_NULL) {
        if (!cJSON_IsString(maxVersionJson) || maxVersionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(maxVersionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outMaxVersionExclusive = maxVersionJson->valuestring;
    }
    return ZR_TRUE;
}

static TZrBool library_project_append_dependency_ref(SZrState *state,
                                                     SZrLibrary_Project *project,
                                                     TZrSize ownerPackageIndex,
                                                     TZrBool ownerIsPackage,
                                                     const TZrChar *name,
                                                     const TZrChar *assemblyName,
                                                     TZrSize packageIndex,
                                                     const TZrChar *minVersionInclusive,
                                                     const TZrChar *maxVersionExclusive,
                                                     TZrBool useAliasForModuleKey) {
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
            if ((*refs)[index].packageIndex == packageIndex &&
                library_project_optional_string_matches((*refs)[index].assemblyName, assemblyName) &&
                library_project_optional_string_matches((*refs)[index].minVersionInclusive, minVersionInclusive) &&
                library_project_optional_string_matches((*refs)[index].maxVersionExclusive, maxVersionExclusive)) {
                if (useAliasForModuleKey) {
                    (*refs)[index].useAliasForModuleKey = ZR_TRUE;
                }
                return ZR_TRUE;
            }
            return ZR_FALSE;
        }
        if ((*refs)[index].packageIndex == packageIndex &&
            (!library_project_optional_string_matches((*refs)[index].minVersionInclusive, minVersionInclusive) ||
             !library_project_optional_string_matches((*refs)[index].maxVersionExclusive, maxVersionExclusive))) {
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
    if (assemblyName != ZR_NULL) {
        slot->assemblyName = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)assemblyName);
    }
    slot->packageIndex = packageIndex;
    if (minVersionInclusive != ZR_NULL) {
        slot->minVersionInclusive = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)minVersionInclusive);
    }
    if (maxVersionExclusive != ZR_NULL) {
        slot->maxVersionExclusive = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)maxVersionExclusive);
    }
    slot->useAliasForModuleKey = useAliasForModuleKey;
    if (slot->name == ZR_NULL ||
        (assemblyName != ZR_NULL && slot->assemblyName == ZR_NULL) ||
        (minVersionInclusive != ZR_NULL && slot->minVersionInclusive == ZR_NULL) ||
        (maxVersionExclusive != ZR_NULL && slot->maxVersionExclusive == ZR_NULL)) {
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
                                                               const TZrChar *assemblyName,
                                                               const TZrChar *version,
                                                               const TZrChar *culture,
                                                               const TZrChar *publicKeyToken,
                                                               const TZrChar *kind,
                                                               const TZrChar *manifestPath,
                                                               const TZrChar *directory) {
    cJSON *sourceJson;
    cJSON *binaryJson;
    cJSON *entryJson;
    cJSON *pathAliasesJson;

    if (state == ZR_NULL || package == ZR_NULL || manifestJson == ZR_NULL || name == ZR_NULL ||
        assemblyName == ZR_NULL || version == ZR_NULL || culture == ZR_NULL || kind == ZR_NULL ||
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

    package->artifactKind = ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_PROJECT;
    package->name = ZrCore_String_CreateTryHitCache(state, name);
    package->assemblyName = ZrCore_String_CreateTryHitCache(state, assemblyName);
    package->version = ZrCore_String_CreateTryHitCache(state, version);
    package->file = ZrCore_String_CreateTryHitCache(state, manifestPath);
    package->directory = ZrCore_String_CreateTryHitCache(state, directory);
    package->culture = ZrCore_String_CreateTryHitCache(state, culture);
    if (publicKeyToken != ZR_NULL) {
        package->publicKeyToken = ZrCore_String_CreateTryHitCache(state, publicKeyToken);
    }
    package->kind = ZrCore_String_CreateTryHitCache(state, kind);
    package->source = ZrCore_String_CreateTryHitCache(state, sourceJson->valuestring);
    package->binary = ZrCore_String_CreateTryHitCache(state, binaryJson->valuestring);
    package->entry = ZrCore_String_CreateTryHitCache(state, entryJson->valuestring);
    if (package->name == ZR_NULL || package->assemblyName == ZR_NULL || package->version == ZR_NULL ||
        package->file == ZR_NULL || package->directory == ZR_NULL || package->culture == ZR_NULL ||
        package->kind == ZR_NULL || (publicKeyToken != ZR_NULL && package->publicKeyToken == ZR_NULL) ||
        package->source == ZR_NULL || package->binary == ZR_NULL || package->entry == ZR_NULL) {
        return ZR_FALSE;
    }

    pathAliasesJson = cJSON_GetObjectItemCaseSensitive(manifestJson, "pathAliases");
    return library_project_parse_path_alias_map(state,
                                                pathAliasesJson,
                                                &package->pathAliases,
                                                &package->pathAliasCount);
}

static TZrBool library_project_parse_zrm_package_fields(SZrState *state,
                                                        SZrLibrary_ProjectDependencyPackage *package,
                                                        SZrLibrary_ZrmArchive *archive,
                                                        const TZrChar *referenceName,
                                                        const TZrChar *archivePath,
                                                        const TZrChar *directory) {
    const TZrChar *culture;
    const TZrChar *publicKeyToken;
    const TZrChar *kind;
    TZrChar normalizedEntry[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (state == ZR_NULL || package == ZR_NULL || archive == ZR_NULL ||
        referenceName == ZR_NULL || archivePath == ZR_NULL || directory == ZR_NULL ||
        !library_project_validate_dependency_name(referenceName) ||
        !library_project_validate_assembly_name(archive->assemblyName) ||
        !library_project_validate_dependency_version(archive->assemblyVersion) ||
        !ZrLibrary_Project_NormalizeModuleKey(archive->entryModule, normalizedEntry, sizeof(normalizedEntry))) {
        return ZR_FALSE;
    }

    culture = archive->assemblyCulture[0] != '\0' ? archive->assemblyCulture : "neutral";
    publicKeyToken = archive->assemblyPublicKeyToken[0] != '\0' ? archive->assemblyPublicKeyToken : ZR_NULL;
    kind = archive->assemblyKind[0] != '\0' ? archive->assemblyKind : "library";

    package->artifactKind = ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM;
    package->name = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)referenceName);
    package->assemblyName = ZrCore_String_CreateTryHitCache(state, archive->assemblyName);
    package->version = ZrCore_String_CreateTryHitCache(state, archive->assemblyVersion);
    package->file = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)archivePath);
    package->directory = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)directory);
    package->culture = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)culture);
    if (publicKeyToken != ZR_NULL) {
        package->publicKeyToken = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)publicKeyToken);
    }
    package->kind = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)kind);
    package->entry = ZrCore_String_CreateTryHitCache(state, normalizedEntry);
    if (package->name == ZR_NULL || package->assemblyName == ZR_NULL || package->version == ZR_NULL ||
        package->file == ZR_NULL || package->directory == ZR_NULL || package->culture == ZR_NULL ||
        (publicKeyToken != ZR_NULL && package->publicKeyToken == ZR_NULL) ||
        package->kind == ZR_NULL || package->entry == ZR_NULL) {
        return ZR_FALSE;
    }

    package->zrmArchive = *archive;
    package->zrmArchiveOpen = ZR_TRUE;
    memset(archive, 0, sizeof(*archive));
    return ZR_TRUE;
}

static TZrBool library_project_parse_dependency_entry(SZrState *state,
                                                      SZrLibrary_Project *project,
                                                      cJSON *dependencyEntry,
                                                      const TZrChar *ownerDirectory,
                                                      TZrSize ownerPackageIndex,
                                                      TZrBool ownerIsPackage) {
    const TZrChar *declaredAssemblyName;
    const TZrChar *declaredPath;
    const TZrChar *declaredVersion;
    const TZrChar *declaredMinVersionInclusive;
    const TZrChar *declaredMaxVersionExclusive;
    const TZrChar *dependencyName;
    TZrChar manifestPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar manifestDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString manifestText;
    TZrSize manifestLength;
    cJSON *manifestJson;
    const TZrChar *manifestName;
    const TZrChar *manifestVersion;
    const TZrChar *manifestCulture;
    const TZrChar *manifestPublicKeyToken;
    const TZrChar *manifestKind;
    const TZrChar *packageName;
    const TZrChar *referenceAssemblyName;
    const TZrChar *effectiveVersion;
    TZrSize packageIndex;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || project == ZR_NULL || dependencyEntry == ZR_NULL || ownerDirectory == ZR_NULL ||
        dependencyEntry->string == ZR_NULL || !library_project_validate_dependency_key(dependencyEntry->string) ||
        !library_project_get_dependency_declaration(dependencyEntry,
                                                    &declaredAssemblyName,
                                                    &declaredPath,
                                                    &declaredVersion,
                                                    &declaredMinVersionInclusive,
                                                    &declaredMaxVersionExclusive) ||
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

    if (!library_project_get_manifest_assembly_identity(manifestJson,
                                                        &manifestName,
                                                        &manifestVersion,
                                                        &manifestCulture,
                                                        &manifestPublicKeyToken,
                                                        &manifestKind)) {
        goto cleanup_json;
    }
    if (declaredAssemblyName != ZR_NULL &&
        (manifestName == ZR_NULL || strcmp(manifestName, declaredAssemblyName) != 0)) {
        goto cleanup_json;
    }
    if (manifestName != ZR_NULL && declaredAssemblyName == ZR_NULL &&
        cJSON_GetObjectItemCaseSensitive(manifestJson, "assembly") == ZR_NULL &&
        !library_project_validate_dependency_name(manifestName)) {
        goto cleanup_json;
    }
    if (manifestVersion != ZR_NULL && declaredVersion != ZR_NULL && strcmp(manifestVersion, declaredVersion) != 0) {
        goto cleanup_json;
    }
    packageName = declaredAssemblyName != ZR_NULL ? dependencyName : (manifestName != ZR_NULL ? manifestName : dependencyName);
    referenceAssemblyName = manifestName != ZR_NULL ? manifestName :
                            (declaredAssemblyName != ZR_NULL ? declaredAssemblyName : packageName);
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
                                                             referenceAssemblyName,
                                                             effectiveVersion,
                                                             manifestCulture,
                                                             manifestPublicKeyToken,
                                                             manifestKind,
                                                             manifestPath,
                                                             manifestDirectory) ||
            !library_project_parse_dependencies(state,
                                                project,
                                                manifestJson,
                                                manifestDirectory,
                                                packageIndex,
                                                ZR_TRUE) ||
            !library_project_parse_references(state,
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
                                                    referenceAssemblyName,
                                                    packageIndex,
                                                    declaredMinVersionInclusive,
                                                    declaredMaxVersionExclusive,
                                                    ZR_FALSE);

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

static TZrBool library_project_get_reference_declaration(cJSON *referenceEntry,
                                                         const TZrChar **outAssemblyName,
                                                         const TZrChar **outPath,
                                                         const TZrChar **outVersion,
                                                         const TZrChar **outMinVersionInclusive,
                                                         const TZrChar **outMaxVersionExclusive) {
    cJSON *assemblyJson;
    cJSON *pathJson;
    cJSON *versionJson;
    cJSON *minVersionJson;
    cJSON *maxVersionJson;

    if (outAssemblyName != ZR_NULL) {
        *outAssemblyName = ZR_NULL;
    }
    if (outPath != ZR_NULL) {
        *outPath = ZR_NULL;
    }
    if (outVersion != ZR_NULL) {
        *outVersion = ZR_NULL;
    }
    if (outMinVersionInclusive != ZR_NULL) {
        *outMinVersionInclusive = ZR_NULL;
    }
    if (outMaxVersionExclusive != ZR_NULL) {
        *outMaxVersionExclusive = ZR_NULL;
    }
    if (referenceEntry == ZR_NULL || outAssemblyName == ZR_NULL || outPath == ZR_NULL ||
        outVersion == ZR_NULL || outMinVersionInclusive == ZR_NULL || outMaxVersionExclusive == ZR_NULL ||
        !cJSON_IsObject(referenceEntry)) {
        return ZR_FALSE;
    }

    assemblyJson = cJSON_GetObjectItemCaseSensitive(referenceEntry, "assembly");
    pathJson = cJSON_GetObjectItemCaseSensitive(referenceEntry, "path");
    versionJson = cJSON_GetObjectItemCaseSensitive(referenceEntry, "version");
    minVersionJson = cJSON_GetObjectItemCaseSensitive(referenceEntry, "minVersionInclusive");
    maxVersionJson = cJSON_GetObjectItemCaseSensitive(referenceEntry, "maxVersionExclusive");
    if (!cJSON_IsString(assemblyJson) || assemblyJson->valuestring == ZR_NULL ||
        !library_project_validate_assembly_name(assemblyJson->valuestring) ||
        !cJSON_IsString(pathJson) || pathJson->valuestring == ZR_NULL || pathJson->valuestring[0] == '\0') {
        return ZR_FALSE;
    }

    *outAssemblyName = assemblyJson->valuestring;
    *outPath = pathJson->valuestring;
    if (versionJson != ZR_NULL) {
        if (!cJSON_IsString(versionJson) || versionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(versionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outVersion = versionJson->valuestring;
    }
    if (minVersionJson != ZR_NULL) {
        if (!cJSON_IsString(minVersionJson) || minVersionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(minVersionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outMinVersionInclusive = minVersionJson->valuestring;
    }
    if (maxVersionJson != ZR_NULL) {
        if (!cJSON_IsString(maxVersionJson) || maxVersionJson->valuestring == ZR_NULL ||
            !library_project_validate_dependency_version(maxVersionJson->valuestring)) {
            return ZR_FALSE;
        }
        *outMaxVersionExclusive = maxVersionJson->valuestring;
    }
    return ZR_TRUE;
}

static TZrBool library_project_parse_reference_entry(SZrState *state,
                                                     SZrLibrary_Project *project,
                                                     cJSON *referenceEntry,
                                                     const TZrChar *ownerDirectory,
                                                     TZrSize ownerPackageIndex,
                                                     TZrBool ownerIsPackage) {
    const TZrChar *referenceName;
    const TZrChar *declaredAssemblyName;
    const TZrChar *declaredPath;
    const TZrChar *declaredVersion;
    const TZrChar *declaredMinVersionInclusive;
    const TZrChar *declaredMaxVersionExclusive;
    TZrChar manifestPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar manifestDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString manifestText;
    TZrSize manifestLength;
    cJSON *manifestJson;
    const TZrChar *manifestAssemblyName;
    const TZrChar *manifestVersion;
    const TZrChar *manifestCulture;
    const TZrChar *manifestPublicKeyToken;
    const TZrChar *manifestKind;
    const TZrChar *effectiveVersion;
    TZrSize packageIndex;
    SZrLibrary_ZrmArchive zrmArchive;
    TZrBool zrmArchiveOpen = ZR_FALSE;
    TZrChar zrmError[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || project == ZR_NULL || referenceEntry == ZR_NULL || ownerDirectory == ZR_NULL ||
        referenceEntry->string == ZR_NULL || !library_project_validate_dependency_name(referenceEntry->string) ||
        !library_project_get_reference_declaration(referenceEntry,
                                                   &declaredAssemblyName,
                                                   &declaredPath,
                                                   &declaredVersion,
                                                   &declaredMinVersionInclusive,
                                                   &declaredMaxVersionExclusive) ||
        !library_project_resolve_manifest_path(ownerDirectory, declaredPath, manifestPath, sizeof(manifestPath)) ||
        ZrLibrary_File_Exist(manifestPath) != ZR_LIBRARY_FILE_IS_FILE ||
        !ZrLibrary_File_GetDirectory(manifestPath, manifestDirectory)) {
        return ZR_FALSE;
    }

    referenceName = referenceEntry->string;
    if (library_project_has_suffix(manifestPath, ZR_LIBRARY_ZRM_FILE_EXTENSION)) {
        memset(&zrmArchive, 0, sizeof(zrmArchive));
        memset(zrmError, 0, sizeof(zrmError));
        if (!ZrLibrary_Zrm_Open(manifestPath, &zrmArchive, zrmError, sizeof(zrmError))) {
            return ZR_FALSE;
        }
        zrmArchiveOpen = ZR_TRUE;

        if (strcmp(zrmArchive.assemblyName, declaredAssemblyName) != 0 ||
            (declaredVersion != ZR_NULL && strcmp(zrmArchive.assemblyVersion, declaredVersion) != 0)) {
            goto cleanup_zrm_archive;
        }
        effectiveVersion = zrmArchive.assemblyVersion[0] != '\0'
                         ? zrmArchive.assemblyVersion
                         : (declaredVersion != ZR_NULL ? declaredVersion : "0.0.0");

        if (!library_project_find_dependency_package(project, referenceName, effectiveVersion, &packageIndex)) {
            SZrLibrary_ProjectDependencyPackage *package;

            if (!library_project_append_dependency_package(state, project, &packageIndex)) {
                goto cleanup_zrm_archive;
            }
            package = &project->dependencyPackages[packageIndex];
            if (!library_project_parse_zrm_package_fields(state,
                                                          package,
                                                          &zrmArchive,
                                                          referenceName,
                                                          manifestPath,
                                                          manifestDirectory)) {
                goto cleanup_zrm_archive;
            }
            zrmArchiveOpen = ZR_FALSE;
        }

        success = library_project_append_dependency_ref(state,
                                                        project,
                                                        ownerPackageIndex,
                                                        ownerIsPackage,
                                                        referenceName,
                                                        declaredAssemblyName,
                                                        packageIndex,
                                                        declaredMinVersionInclusive,
                                                        declaredMaxVersionExclusive,
                                                        ZR_TRUE);

cleanup_zrm_archive:
        if (zrmArchiveOpen) {
            ZrLibrary_Zrm_Close(&zrmArchive);
        }
        return success;
    }

    manifestText = ZrLibrary_File_ReadAll(state->global, manifestPath);
    if (manifestText == ZR_NULL) {
        return ZR_FALSE;
    }

    manifestLength = strlen(manifestText);
    manifestJson = cJSON_Parse(manifestText);
    if (manifestJson == ZR_NULL) {
        goto cleanup_text;
    }

    if (!library_project_get_manifest_assembly_identity(manifestJson,
                                                        &manifestAssemblyName,
                                                        &manifestVersion,
                                                        &manifestCulture,
                                                        &manifestPublicKeyToken,
                                                        &manifestKind) ||
        manifestAssemblyName == ZR_NULL ||
        strcmp(manifestAssemblyName, declaredAssemblyName) != 0 ||
        (manifestVersion != ZR_NULL && declaredVersion != ZR_NULL && strcmp(manifestVersion, declaredVersion) != 0)) {
        goto cleanup_json;
    }
    effectiveVersion = manifestVersion != ZR_NULL ? manifestVersion : (declaredVersion != ZR_NULL ? declaredVersion : "0.0.0");

    if (!library_project_find_dependency_package(project, referenceName, effectiveVersion, &packageIndex)) {
        SZrLibrary_ProjectDependencyPackage *package;

        if (!library_project_append_dependency_package(state, project, &packageIndex)) {
            goto cleanup_json;
        }
        package = &project->dependencyPackages[packageIndex];
        if (!library_project_parse_dependency_package_fields(state,
                                                             package,
                                                             manifestJson,
                                                             referenceName,
                                                             manifestAssemblyName,
                                                             effectiveVersion,
                                                             manifestCulture,
                                                             manifestPublicKeyToken,
                                                             manifestKind,
                                                             manifestPath,
                                                             manifestDirectory) ||
            !library_project_parse_dependencies(state,
                                                project,
                                                manifestJson,
                                                manifestDirectory,
                                                packageIndex,
                                                ZR_TRUE) ||
            !library_project_parse_references(state,
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
                                                    referenceName,
                                                    manifestAssemblyName,
                                                    packageIndex,
                                                    declaredMinVersionInclusive,
                                                    declaredMaxVersionExclusive,
                                                    ZR_TRUE);

cleanup_json:
    cJSON_Delete(manifestJson);
cleanup_text:
    ZrCore_Memory_RawFreeWithType(state->global,
                                  manifestText,
                                  manifestLength + 1,
                                  ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return success;
}

static TZrBool library_project_parse_references(SZrState *state,
                                                SZrLibrary_Project *project,
                                                cJSON *projectJson,
                                                const TZrChar *ownerDirectory,
                                                TZrSize ownerPackageIndex,
                                                TZrBool ownerIsPackage) {
    cJSON *referencesJson;
    cJSON *referenceEntry;

    if (state == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL || ownerDirectory == ZR_NULL) {
        return ZR_FALSE;
    }

    referencesJson = cJSON_GetObjectItemCaseSensitive(projectJson, "references");
    if (referencesJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(referencesJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(referenceEntry, referencesJson) {
        if (!library_project_parse_reference_entry(state,
                                                   project,
                                                   referenceEntry,
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
    if (!library_project_validate_manifest_version(projectJson)) {
        cJSON_Delete(projectJson);
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
    {
        const TZrChar *assemblyNameText;
        const TZrChar *assemblyVersionText;
        const TZrChar *assemblyCultureText;
        const TZrChar *assemblyPublicKeyTokenText;
        const TZrChar *assemblyKindText;
        const TZrChar *assemblyOutputText;

        if (!library_project_get_manifest_assembly_identity(projectJson,
                                                            &assemblyNameText,
                                                            &assemblyVersionText,
                                                            &assemblyCultureText,
                                                            &assemblyPublicKeyTokenText,
                                                            &assemblyKindText)) {
            cJSON_Delete(projectJson);
            library_project_free_dependencies(global, project);
            library_project_free_path_aliases(global, project);
            ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
            return ZR_NULL;
        }
        if (!library_project_get_manifest_assembly_output(projectJson, &assemblyOutputText)) {
            cJSON_Delete(projectJson);
            library_project_free_resources(global, project);
            library_project_free_dependencies(global, project);
            library_project_free_path_aliases(global, project);
            ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
            return ZR_NULL;
        }
        if (assemblyNameText != ZR_NULL) {
            project->assemblyName = ZrCore_String_CreateTryHitCache(state, assemblyNameText);
            project->name = project->assemblyName;
        }
        if (assemblyVersionText == ZR_NULL) {
            assemblyVersionText = "0.0.0";
        }
        project->version = ZrCore_String_CreateTryHitCache(state, assemblyVersionText);
        project->assemblyCulture = ZrCore_String_CreateTryHitCache(state, assemblyCultureText);
        if (assemblyPublicKeyTokenText != ZR_NULL) {
            project->assemblyPublicKeyToken = ZrCore_String_CreateTryHitCache(state, assemblyPublicKeyTokenText);
        }
        project->assemblyKind = ZrCore_String_CreateTryHitCache(state, assemblyKindText);
        if (assemblyOutputText != ZR_NULL) {
            project->assemblyOutput = ZrCore_String_CreateTryHitCache(state, (TZrNativeString)assemblyOutputText);
        }
        if ((assemblyNameText != ZR_NULL && project->assemblyName == ZR_NULL) ||
            project->version == ZR_NULL ||
            project->assemblyCulture == ZR_NULL ||
            (assemblyPublicKeyTokenText != ZR_NULL && project->assemblyPublicKeyToken == ZR_NULL) ||
            project->assemblyKind == ZR_NULL ||
            (assemblyOutputText != ZR_NULL && project->assemblyOutput == ZR_NULL)) {
            cJSON_Delete(projectJson);
            library_project_free_resources(global, project);
            library_project_free_dependencies(global, project);
            library_project_free_path_aliases(global, project);
            ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
            return ZR_NULL;
        }
    }

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
    if (!library_project_parse_aot_options(project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_resources(global, project);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_feature_switches(state, project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_resources(state, project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_path_aliases(state, project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_dependencies(state, project, projectJson, path, 0, ZR_FALSE)) {
        cJSON_Delete(projectJson);
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_references(state, project, projectJson, path, 0, ZR_FALSE)) {
        cJSON_Delete(projectJson);
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_preserve_rules(global, project);
        library_project_free_dependencies(global, project);
        library_project_free_path_aliases(global, project);
        ZrCore_Memory_RawFreeWithType(global, project, sizeof(SZrLibrary_Project), ZR_MEMORY_NATIVE_TYPE_PROJECT);
        return ZR_NULL;
    }
    if (!library_project_parse_preserve_rules(state, project, projectJson)) {
        cJSON_Delete(projectJson);
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_preserve_rules(global, project);
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
        library_project_free_feature_switches(global, project);
        library_project_free_resources(global, project);
        library_project_free_preserve_rules(global, project);
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
    ZrLibrary_AotRuntime_FreeProjectState(state, project);
    library_project_free_feature_switches(global, project);
    library_project_free_resources(global, project);
    library_project_free_preserve_rules(global, project);
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

static TZrBytePtr library_project_memory_read_implementation(SZrState *state,
                                                             TZrPtr reader,
                                                             ZR_OUT TZrSize *size) {
    SZrLibrary_ProjectMemoryReader *memoryReader = (SZrLibrary_ProjectMemoryReader *)reader;
    TZrSize remaining;

    ZR_UNUSED_PARAMETER(state);
    if (memoryReader == ZR_NULL || size == ZR_NULL || memoryReader->offset >= memoryReader->byteCount) {
        return ZR_NULL;
    }

    remaining = memoryReader->byteCount - memoryReader->offset;
    *size = remaining;
    memoryReader->offset = memoryReader->byteCount;
    return memoryReader->bytes + (memoryReader->byteCount - remaining);
}

static void library_project_memory_close_implementation(SZrState *state, TZrPtr reader) {
    SZrLibrary_ProjectMemoryReader *memoryReader = (SZrLibrary_ProjectMemoryReader *)reader;

    if (state == ZR_NULL || state->global == ZR_NULL || memoryReader == ZR_NULL) {
        return;
    }

    if (memoryReader->bytes != ZR_NULL) {
        ZrLibrary_Zrm_FreeBytes(memoryReader->bytes);
    }
    ZrCore_Memory_RawFreeWithType(state->global,
                                  memoryReader,
                                  sizeof(*memoryReader),
                                  ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
}

static TZrBool library_project_load_zrm_module_entry(SZrState *state,
                                                     const SZrLibrary_Project *project,
                                                     const TZrChar *moduleName,
                                                     SZrIo *io) {
    const SZrLibrary_ZrmArchive *archive;
    const SZrLibrary_ZrmEntryInfo *entry;
    SZrLibrary_ProjectMemoryReader *reader;
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0;
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || moduleName == ZR_NULL || io == ZR_NULL ||
        !ZrLibrary_Project_ResolveZrmModuleEntry(project, moduleName, &archive, &entry)) {
        return ZR_FALSE;
    }

    memset(error, 0, sizeof(error));
    if (!ZrLibrary_Zrm_ReadEntry(archive,
                                 entry->entryName,
                                 &bytes,
                                 &byteCount,
                                 error,
                                 sizeof(error)) ||
        bytes == ZR_NULL) {
        return ZR_FALSE;
    }

    reader = (SZrLibrary_ProjectMemoryReader *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*reader),
            ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
    if (reader == ZR_NULL) {
        ZrLibrary_Zrm_FreeBytes(bytes);
        return ZR_FALSE;
    }

    reader->bytes = bytes;
    reader->byteCount = byteCount;
    reader->offset = 0;
    ZrCore_Io_Init(state,
                   io,
                   library_project_memory_read_implementation,
                   library_project_memory_close_implementation,
                   reader);
    io->isBinary = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool library_project_copy_diagnostic_path(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize index;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    for (index = 0; path[index] != '\0'; index++) {
        if (index + 1 >= bufferSize) {
            buffer[0] = '\0';
            return ZR_FALSE;
        }
        buffer[index] = path[index] == '\\' ? '/' : path[index];
    }
    buffer[index] = '\0';
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
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar diagnosticSourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar diagnosticBinaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrBool hasSourcePath;
    TZrBool hasBinaryPath;

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || state->global == ZR_NULL || io == ZR_NULL) {
        return ZR_FALSE;
    }

    const SZrLibrary_Project *project = ZrLibrary_Project_GetFromGlobal(state->global);
    if (project == ZR_NULL || path == ZR_NULL) {
        return ZR_FALSE;
    }

    hasSourcePath = library_project_resolve_source_path(project, path, sourcePath);
    if (hasSourcePath && ZrLibrary_File_Exist(sourcePath) == ZR_LIBRARY_FILE_IS_FILE) {
        return library_project_load_resolved_file(state, sourcePath, ZR_FALSE, io);
    }

    hasBinaryPath = library_project_resolve_binary_path(project, path, binaryPath);
    if (hasBinaryPath && ZrLibrary_File_Exist(binaryPath) == ZR_LIBRARY_FILE_IS_FILE) {
        return library_project_load_resolved_file(state, binaryPath, ZR_TRUE, io);
    }

    if (library_project_load_zrm_module_entry(state, project, path, io)) {
        return ZR_TRUE;
    }

    if (ZrCore_GlobalState_GetModuleLoadDiagnostic(state->global) == ZR_NULL) {
        const TZrChar *sourceDiagnostic = "<unresolved>";
        const TZrChar *binaryDiagnostic = "<unresolved>";

        if (hasSourcePath &&
            library_project_copy_diagnostic_path(sourcePath, diagnosticSourcePath, sizeof(diagnosticSourcePath))) {
            sourceDiagnostic = diagnosticSourcePath;
        }
        if (hasBinaryPath &&
            library_project_copy_diagnostic_path(binaryPath, diagnosticBinaryPath, sizeof(diagnosticBinaryPath))) {
            binaryDiagnostic = diagnosticBinaryPath;
        }
        ZrCore_GlobalState_SetModuleLoadDiagnostic(
                state->global,
                "loader=project-source result=not-found module='%s' source='%s' binary='%s'",
                path,
                sourceDiagnostic,
                binaryDiagnostic);
    }
    return ZR_FALSE;
}
