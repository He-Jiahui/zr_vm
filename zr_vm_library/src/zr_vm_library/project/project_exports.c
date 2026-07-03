#include "project/project_exports.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool library_project_export_validate_target(const TZrChar *target) {
    TZrSize index;
    TZrBool previousWasDot = ZR_TRUE;

    if (target == ZR_NULL || target[0] == '\0') {
        return ZR_FALSE;
    }

    for (index = 0; target[index] != '\0'; index++) {
        TZrChar current = target[index];
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

static TZrBool library_project_export_parse_kind(const TZrChar *text,
                                                 EZrLibrary_ProjectExportDeclarationKind *outKind) {
    if (outKind != ZR_NULL) {
        *outKind = ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_TYPE;
    }
    if (text == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(text, "type") == 0) {
        *outKind = ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_TYPE;
        return ZR_TRUE;
    }
    if (strcmp(text, "method") == 0) {
        *outKind = ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_METHOD;
        return ZR_TRUE;
    }
    if (strcmp(text, "field") == 0) {
        *outKind = ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_FIELD;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool library_project_export_declaration_exists(const SZrLibrary_Project *project,
                                                         TZrSize declarationCount,
                                                         EZrLibrary_ProjectExportDeclarationKind kind,
                                                         const TZrChar *target) {
    if (project == ZR_NULL || project->exportDeclarations == ZR_NULL || target == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < declarationCount; index++) {
        const SZrLibrary_ProjectExportDeclaration *declaration = &project->exportDeclarations[index];
        const TZrChar *existingTarget = declaration->target != ZR_NULL
                                                ? ZrCore_String_GetNativeString(declaration->target)
                                                : ZR_NULL;
        if (declaration->kind == kind &&
            existingTarget != ZR_NULL &&
            strcmp(existingTarget, target) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void library_project_free_export_declarations(SZrGlobalState *global, SZrLibrary_Project *project) {
    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    if (project->exportDeclarations != ZR_NULL && project->exportDeclarationCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->exportDeclarations,
                                      sizeof(*project->exportDeclarations) * project->exportDeclarationCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->exportDeclarations = ZR_NULL;
    project->exportDeclarationCount = 0;
    project->exportDeclarationCapacity = 0;
}

TZrBool library_project_parse_export_declarations(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *exportsJson;
    cJSON *exportJson;
    TZrSize exportCount = 0;
    TZrSize exportIndex = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    exportsJson = cJSON_GetObjectItemCaseSensitive(projectJson, "exports");
    if (exportsJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsArray(exportsJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(exportJson, exportsJson) {
        exportCount++;
    }
    if (exportCount == 0) {
        return ZR_TRUE;
    }

    project->exportDeclarations = (SZrLibrary_ProjectExportDeclaration *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*project->exportDeclarations) * exportCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->exportDeclarations == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->exportDeclarations, 0, sizeof(*project->exportDeclarations) * exportCount);
    project->exportDeclarationCapacity = exportCount;

    cJSON_ArrayForEach(exportJson, exportsJson) {
        cJSON *kindJson;
        cJSON *targetJson;
        EZrLibrary_ProjectExportDeclarationKind kind = ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_TYPE;

        if (!cJSON_IsObject(exportJson)) {
            return ZR_FALSE;
        }

        kindJson = cJSON_GetObjectItemCaseSensitive(exportJson, "kind");
        targetJson = cJSON_GetObjectItemCaseSensitive(exportJson, "target");
        if (!cJSON_IsString(kindJson) || kindJson->valuestring == ZR_NULL ||
            !cJSON_IsString(targetJson) || targetJson->valuestring == ZR_NULL ||
            !library_project_export_parse_kind(kindJson->valuestring, &kind) ||
            !library_project_export_validate_target(targetJson->valuestring) ||
            library_project_export_declaration_exists(project,
                                                      exportIndex,
                                                      kind,
                                                      targetJson->valuestring)) {
            return ZR_FALSE;
        }

        project->exportDeclarations[exportIndex].kind = kind;
        project->exportDeclarations[exportIndex].target =
                ZrCore_String_CreateTryHitCache(state, targetJson->valuestring);
        if (project->exportDeclarations[exportIndex].target == ZR_NULL) {
            return ZR_FALSE;
        }
        exportIndex++;
    }

    project->exportDeclarationCount = exportIndex;
    return ZR_TRUE;
}
