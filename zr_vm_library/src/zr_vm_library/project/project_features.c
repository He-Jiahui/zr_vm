#include "project/project_features.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool library_project_feature_validate_name(const TZrChar *name) {
    TZrSize index;
    TZrBool previousWasDot = ZR_TRUE;

    if (name == ZR_NULL || name[0] == '\0') {
        return ZR_FALSE;
    }

    for (index = 0; name[index] != '\0'; index++) {
        TZrChar current = name[index];
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

void library_project_free_feature_switches(SZrGlobalState *global, SZrLibrary_Project *project) {
    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    if (project->featureSwitches != ZR_NULL && project->featureSwitchCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->featureSwitches,
                                      sizeof(*project->featureSwitches) * project->featureSwitchCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->featureSwitches = ZR_NULL;
    project->featureSwitchCount = 0;
    project->featureSwitchCapacity = 0;
}

TZrBool library_project_parse_feature_switches(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *featuresJson;
    cJSON *featureJson;
    TZrSize featureCount = 0;
    TZrSize featureIndex = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    featuresJson = cJSON_GetObjectItemCaseSensitive(projectJson, "features");
    if (featuresJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsObject(featuresJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(featureJson, featuresJson) {
        featureCount++;
    }
    if (featureCount == 0) {
        return ZR_TRUE;
    }

    project->featureSwitches = (SZrLibrary_ProjectFeatureSwitch *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*project->featureSwitches) * featureCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->featureSwitches == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->featureSwitches, 0, sizeof(*project->featureSwitches) * featureCount);
    project->featureSwitchCapacity = featureCount;

    cJSON_ArrayForEach(featureJson, featuresJson) {
        if (featureJson->string == ZR_NULL ||
            !library_project_feature_validate_name(featureJson->string) ||
            !cJSON_IsBool(featureJson)) {
            return ZR_FALSE;
        }

        for (TZrSize previousIndex = 0; previousIndex < featureIndex; previousIndex++) {
            const SZrLibrary_ProjectFeatureSwitch *existing = &project->featureSwitches[previousIndex];
            const TZrChar *existingName = existing->name != ZR_NULL
                                                  ? ZrCore_String_GetNativeString(existing->name)
                                                  : ZR_NULL;
            if (existingName != ZR_NULL && strcmp(existingName, featureJson->string) == 0) {
                return ZR_FALSE;
            }
        }

        project->featureSwitches[featureIndex].name =
                ZrCore_String_CreateTryHitCache(state, featureJson->string);
        if (project->featureSwitches[featureIndex].name == ZR_NULL) {
            return ZR_FALSE;
        }
        project->featureSwitches[featureIndex].value =
                cJSON_IsTrue(featureJson) ? ZR_TRUE : ZR_FALSE;
        featureIndex++;
    }

    project->featureSwitchCount = featureIndex;
    return ZR_TRUE;
}
