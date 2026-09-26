#ifndef ZR_VM_LIBRARY_PROJECT_AOT_OPTIONS_H
#define ZR_VM_LIBRARY_PROJECT_AOT_OPTIONS_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

/** @brief 解析项目 AOT 模式，缺省使用 hybrid，供 CLI 编译选项消费。 */
TZrBool library_project_parse_aot_options(SZrLibrary_Project *project, cJSON *projectJson);

#endif
