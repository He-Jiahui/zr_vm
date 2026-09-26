#ifndef ZR_VM_LIBRARY_PROJECT_EXPORTS_H
#define ZR_VM_LIBRARY_PROJECT_EXPORTS_H

#include "cJSON/cJSON.h"
#include "zr_vm_library/project.h"

/** @brief 将 manifest exports 投影为 CLI AOT 导出根；失败由项目构造器统一清理。 */
TZrBool library_project_parse_export_declarations(SZrState *state, SZrLibrary_Project *project, cJSON *projectJson);
/** @brief 项目构造失败或析构时释放原生导出数组；GC 字符串仍由 VM 持有。 */
void library_project_free_export_declarations(SZrGlobalState *global, SZrLibrary_Project *project);

#endif
