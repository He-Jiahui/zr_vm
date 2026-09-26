#ifndef ZR_VM_LIBRARY_PROJECT_FEATURES_H
#define ZR_VM_LIBRARY_PROJECT_FEATURES_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

/** @brief 解析 manifest features，供编译期条件和 AOT 保留规则共用。 */
TZrBool library_project_parse_feature_switches(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
/** @brief 释放项目持有的原生 feature 数组，包括解析中途的分配。 */
void library_project_free_feature_switches(SZrGlobalState *global, SZrLibrary_Project *project);

#endif
