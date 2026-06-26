#ifndef ZR_VM_LIBRARY_PROJECT_FEATURES_H
#define ZR_VM_LIBRARY_PROJECT_FEATURES_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

TZrBool library_project_parse_feature_switches(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
void library_project_free_feature_switches(SZrGlobalState *global, SZrLibrary_Project *project);

#endif
