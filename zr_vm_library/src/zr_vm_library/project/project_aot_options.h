#ifndef ZR_VM_LIBRARY_PROJECT_AOT_OPTIONS_H
#define ZR_VM_LIBRARY_PROJECT_AOT_OPTIONS_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

TZrBool library_project_parse_aot_options(SZrLibrary_Project *project, cJSON *projectJson);

#endif
