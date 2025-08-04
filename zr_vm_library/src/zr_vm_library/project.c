//
// Created by HeJiahui on 2025/7/27.
//
#include "cJSON/cJSON.h"

#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"

#define ZR_JSON_READ_STRING(STATE, OBJECT, NAME)                                                                       \
    TZrString *NAME = ZR_NULL;                                                                                         \
    {                                                                                                                  \
        cJSON *JSON_##NAME = cJSON_GetObjectItemCaseSensitive(OBJECT, #NAME);                                          \
        if (cJSON_IsString(JSON_##NAME) && JSON_##NAME->valuestring != ZR_NULL) {                                      \
            NAME = ZrStringCreateTryHitCache(STATE, JSON_##NAME->valuestring);                                         \
        } else {                                                                                                       \
            NAME = ZR_NULL;                                                                                            \
        }                                                                                                              \
    }

SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TNativeString raw, TNativeString file) {
    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project =
            ZrMemoryRawMallocWithType(global, sizeof(SZrLibrary_Project), ZR_VALUE_TYPE_NATIVE_DATA);

    cJSON *projectJson = cJSON_Parse(raw);
    if (projectJson == ZR_NULL) {
        return ZR_NULL;
    }

    project->file = ZrStringCreateTryHitCache(state, file);

    TChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    TBool success = ZrLibrary_File_GetDirectory(file, path);
    if (!success) {
        return ZR_NULL;
    }
    project->directory = ZrStringCreateTryHitCache(state, path);
    // cJSON *name = cJSON_GetObjectItemCaseSensitive(projectJson, "name");
    // if (cJSON_IsString(name) && name->valuestring != ZR_NULL) {
    //     project->name = ZrStringCreateTryHitCache(state, name->valuestring);
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

    // todo:
    return project;
}


void ZrLibrary_Project_Do(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project = global->userData;
    TNativeString entry = ZrStringGetNativeString(project->entry);
    SZrIoSource *source = ZrIoLoadSource(state, entry, 0);
}

TBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TNativeString path, TNativeString md5, SZrIo *io) {
    SZrGlobalState *global = state->global;
    SZrLibrary_Project *project = global->userData;
    TChar fullDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];
    TChar fullPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TNativeString directory = ZrStringGetNativeString(project->directory);
    TNativeString binary = ZrStringGetNativeString(project->binary);
    // todo: load module
    ZrLibrary_File_PathJoin(directory, binary, fullDirectory);
    ZrLibrary_File_PathJoin(fullDirectory, path, fullPath);
    ZrNativeStringConcat(fullPath, ZR_LIBRARY_BINARY_FILE_EXT, fullPath);
    return ZrLibrary_File_SourceLoadImplementation(state, fullPath, md5, io);
}
