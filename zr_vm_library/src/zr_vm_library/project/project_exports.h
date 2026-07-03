#ifndef ZR_VM_LIBRARY_PROJECT_EXPORTS_H
#define ZR_VM_LIBRARY_PROJECT_EXPORTS_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

TZrBool library_project_parse_export_declarations(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
void library_project_free_export_declarations(SZrGlobalState *global, SZrLibrary_Project *project);

#endif
