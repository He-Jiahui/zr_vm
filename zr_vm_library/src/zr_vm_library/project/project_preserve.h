#ifndef ZR_VM_LIBRARY_PROJECT_PRESERVE_H
#define ZR_VM_LIBRARY_PROJECT_PRESERVE_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

/** @brief 解析 AOT preserve 规则及 feature 条件；成功后数组归 project 所有。 */
TZrBool library_project_parse_preserve_rules(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
/** @brief 清理规则及嵌套 genericArguments 原生数组，允许解析中途调用。 */
void library_project_free_preserve_rules(SZrGlobalState *global, SZrLibrary_Project *project);

#endif
