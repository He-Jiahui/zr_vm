#ifndef ZR_VM_LIBRARY_PROJECT_PRESERVE_H
#define ZR_VM_LIBRARY_PROJECT_PRESERVE_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

TZrBool library_project_parse_preserve_rules(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
void library_project_free_preserve_rules(SZrGlobalState *global, SZrLibrary_Project *project);

#endif
