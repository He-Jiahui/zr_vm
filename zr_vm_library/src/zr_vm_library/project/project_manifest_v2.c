#include "project/project_manifest_v2.h"

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
