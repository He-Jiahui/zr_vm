#include "project/project_preserve.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

static TZrBool library_project_preserve_validate_target(const TZrChar *target) {
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

static TZrBool library_project_preserve_parse_kind(const TZrChar *text,
                                                   EZrLibrary_ProjectPreserveRuleKind *outKind) {
    if (outKind != ZR_NULL) {
        *outKind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE;
    }
    if (text == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(text, "type") == 0) {
        *outKind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE;
        return ZR_TRUE;
    }
    if (strcmp(text, "method") == 0) {
        *outKind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD;
        return ZR_TRUE;
    }
    if (strcmp(text, "generic") == 0) {
        *outKind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool library_project_preserve_parse_members(const TZrChar *text,
                                                      EZrLibrary_ProjectPreserveMembers *outMembers) {
    if (outMembers != ZR_NULL) {
        *outMembers = ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_DEFAULT;
    }
    if (outMembers == ZR_NULL) {
        return ZR_FALSE;
    }
    if (text == ZR_NULL) {
        return ZR_TRUE;
    }
    if (strcmp(text, "all") == 0) {
        *outMembers = ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_ALL;
        return ZR_TRUE;
    }
    if (strcmp(text, "methods") == 0) {
        *outMembers = ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_METHODS;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool library_project_preserve_parse_generic_arguments(SZrState *state,
                                                                SZrLibrary_ProjectPreserveRule *rule,
                                                                cJSON *argumentsJson) {
    cJSON *argumentJson;
    TZrSize argumentCount = 0;
    TZrSize argumentIndex = 0;

    if (state == ZR_NULL || rule == ZR_NULL) {
        return ZR_FALSE;
    }
    if (argumentsJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsArray(argumentsJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(argumentJson, argumentsJson) {
        argumentCount++;
    }
    if (argumentCount == 0) {
        return ZR_FALSE;
    }

    rule->genericArguments = (SZrString **)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*rule->genericArguments) * argumentCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (rule->genericArguments == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(rule->genericArguments, 0, sizeof(*rule->genericArguments) * argumentCount);
    rule->genericArgumentCapacity = argumentCount;

    cJSON_ArrayForEach(argumentJson, argumentsJson) {
        if (!cJSON_IsString(argumentJson) || argumentJson->valuestring == ZR_NULL ||
            !library_project_preserve_validate_target(argumentJson->valuestring)) {
            return ZR_FALSE;
        }
        rule->genericArguments[argumentIndex] =
                ZrCore_String_CreateTryHitCache(state, argumentJson->valuestring);
        if (rule->genericArguments[argumentIndex] == ZR_NULL) {
            return ZR_FALSE;
        }
        argumentIndex++;
    }

    rule->genericArgumentCount = argumentIndex;
    return ZR_TRUE;
}

void library_project_free_preserve_rules(SZrGlobalState *global, SZrLibrary_Project *project) {
    TZrSize ruleIndex;

    if (global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    if (project->preserveRules != ZR_NULL) {
        for (ruleIndex = 0; ruleIndex < project->preserveRuleCapacity; ruleIndex++) {
            SZrLibrary_ProjectPreserveRule *rule = &project->preserveRules[ruleIndex];
            if (rule->genericArguments != ZR_NULL && rule->genericArgumentCapacity > 0) {
                ZrCore_Memory_RawFreeWithType(global,
                                              rule->genericArguments,
                                              sizeof(*rule->genericArguments) * rule->genericArgumentCapacity,
                                              ZR_MEMORY_NATIVE_TYPE_PROJECT);
            }
            rule->genericArguments = ZR_NULL;
            rule->genericArgumentCount = 0;
            rule->genericArgumentCapacity = 0;
        }
    }

    if (project->preserveRules != ZR_NULL && project->preserveRuleCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      project->preserveRules,
                                      sizeof(*project->preserveRules) * project->preserveRuleCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_PROJECT);
    }
    project->preserveRules = ZR_NULL;
    project->preserveRuleCount = 0;
    project->preserveRuleCapacity = 0;
}

TZrBool library_project_parse_preserve_rules(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson) {
    cJSON *preserveJson;
    cJSON *ruleJson;
    TZrSize ruleCount = 0;
    TZrSize ruleIndex = 0;

    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL || projectJson == ZR_NULL) {
        return ZR_FALSE;
    }

    preserveJson = cJSON_GetObjectItemCaseSensitive(projectJson, "preserve");
    if (preserveJson == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsArray(preserveJson)) {
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(ruleJson, preserveJson) {
        ruleCount++;
    }
    if (ruleCount == 0) {
        return ZR_TRUE;
    }

    project->preserveRules = (SZrLibrary_ProjectPreserveRule *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*project->preserveRules) * ruleCount,
            ZR_MEMORY_NATIVE_TYPE_PROJECT);
    if (project->preserveRules == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(project->preserveRules, 0, sizeof(*project->preserveRules) * ruleCount);
    project->preserveRuleCapacity = ruleCount;

    cJSON_ArrayForEach(ruleJson, preserveJson) {
        cJSON *kindJson;
        cJSON *targetJson;
        cJSON *membersJson;
        cJSON *argumentsJson;
        cJSON *featureJson;
        cJSON *featureValueJson;
        EZrLibrary_ProjectPreserveRuleKind kind = ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE;
        EZrLibrary_ProjectPreserveMembers members = ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_DEFAULT;
        TZrBool hasFeatureValue = ZR_FALSE;
        TZrBool featureValue = ZR_FALSE;

        if (!cJSON_IsObject(ruleJson)) {
            return ZR_FALSE;
        }

        kindJson = cJSON_GetObjectItemCaseSensitive(ruleJson, "kind");
        targetJson = cJSON_GetObjectItemCaseSensitive(ruleJson, "target");
        membersJson = cJSON_GetObjectItemCaseSensitive(ruleJson, "members");
        argumentsJson = cJSON_GetObjectItemCaseSensitive(ruleJson, "arguments");
        featureJson = cJSON_GetObjectItemCaseSensitive(ruleJson, "feature");
        featureValueJson = cJSON_GetObjectItemCaseSensitive(ruleJson, "featureValue");
        if (!cJSON_IsString(kindJson) || kindJson->valuestring == ZR_NULL ||
            !cJSON_IsString(targetJson) || targetJson->valuestring == ZR_NULL ||
            !library_project_preserve_parse_kind(kindJson->valuestring, &kind) ||
            !library_project_preserve_validate_target(targetJson->valuestring)) {
            return ZR_FALSE;
        }
        if (membersJson != ZR_NULL &&
            (!cJSON_IsString(membersJson) || membersJson->valuestring == ZR_NULL ||
             !library_project_preserve_parse_members(membersJson->valuestring, &members))) {
            return ZR_FALSE;
        }
        if (membersJson == ZR_NULL &&
            !library_project_preserve_parse_members(ZR_NULL, &members)) {
            return ZR_FALSE;
        }
        if ((kind == ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC && argumentsJson == ZR_NULL) ||
            (kind != ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC && argumentsJson != ZR_NULL)) {
            return ZR_FALSE;
        }
        if (featureJson != ZR_NULL &&
            (!cJSON_IsString(featureJson) || featureJson->valuestring == ZR_NULL ||
             !library_project_preserve_validate_target(featureJson->valuestring))) {
            return ZR_FALSE;
        }
        if (featureValueJson != ZR_NULL) {
            if (featureJson == ZR_NULL || !cJSON_IsBool(featureValueJson)) {
                return ZR_FALSE;
            }
            hasFeatureValue = ZR_TRUE;
            featureValue = cJSON_IsTrue(featureValueJson) ? ZR_TRUE : ZR_FALSE;
        } else if (featureJson != ZR_NULL) {
            return ZR_FALSE;
        }

        project->preserveRules[ruleIndex].kind = kind;
        project->preserveRules[ruleIndex].members = members;
        project->preserveRules[ruleIndex].target =
                ZrCore_String_CreateTryHitCache(state, targetJson->valuestring);
        if (project->preserveRules[ruleIndex].target == ZR_NULL) {
            return ZR_FALSE;
        }
        if (!library_project_preserve_parse_generic_arguments(state,
                                                              &project->preserveRules[ruleIndex],
                                                              argumentsJson)) {
            return ZR_FALSE;
        }
        if (featureJson != ZR_NULL) {
            project->preserveRules[ruleIndex].feature =
                    ZrCore_String_CreateTryHitCache(state, featureJson->valuestring);
            if (project->preserveRules[ruleIndex].feature == ZR_NULL) {
                return ZR_FALSE;
            }
        }
        project->preserveRules[ruleIndex].hasFeatureValue = hasFeatureValue;
        project->preserveRules[ruleIndex].featureValue = featureValue;
        ruleIndex++;
    }

    project->preserveRuleCount = ruleIndex;
    return ZR_TRUE;
}
