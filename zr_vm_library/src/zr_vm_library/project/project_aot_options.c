#include "project/project_aot_options.h"

#include <string.h>

TZrBool library_project_parse_aot_options(SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *aotModeJson;

    if (project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    project->aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
    aotModeJson = cJSON_GetObjectItemCaseSensitive(projectJson, "aotMode");
    if (aotModeJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsString(aotModeJson) || aotModeJson->valuestring == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(aotModeJson->valuestring, "hybrid") == 0) {
        project->aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID;
        return ZR_TRUE;
    }
    if (strcmp(aotModeJson->valuestring, "full-aot") == 0) {
        project->aotMode = ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}
