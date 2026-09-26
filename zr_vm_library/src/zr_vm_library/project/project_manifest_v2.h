#ifndef ZR_VM_LIBRARY_PROJECT_MANIFEST_V2_H
#define ZR_VM_LIBRARY_PROJECT_MANIFEST_V2_H

#include "cJSON/cJSON.h"

#include "zr_vm_library/project.h"

/** @brief 构造项目时选择 v1/v2 清单语义；无显式版本按 v1 兼容。 */
TZrBool library_project_manifest_validate_version(cJSON *manifestJson, TZrUInt32 *outManifestVersion);

/** @brief v2 声明解析前确认项目最小可装载字段齐备。 */
TZrBool library_project_manifest_v2_validate_base(cJSON *manifestJson);

/** @brief 装入 v2 别名、包导出和两类依赖；失败时回滚原生声明数组。 */
TZrBool library_project_manifest_v2_parse_declarations(SZrState *state,
                                                        SZrLibrary_Project *project,
                                                        cJSON *manifestJson);

/** @brief 写出规范发布清单之前执行比本地加载更严格的来源和身份检查。 */
TZrBool library_project_manifest_v2_validate_writer_input(const SZrLibrary_Project *project);

/** @brief 将规范包身份转为公开 @ 根字面量，供 manifest 与锁写出共用。 */
TZrBool library_project_manifest_v2_package_identity_to_literal(
        const SZrLibrary_ModuleIdentity *identity,
        TZrChar *outLiteral,
        TZrSize outLiteralSize);

/** @brief 按规范包身份选择指定序位，写出器借此获得与输入数组顺序无关的结果。 */
TZrBool library_project_manifest_v2_dependency_index_at_ordinal(
        const SZrLibrary_ProjectManifestDependency *dependencies,
        TZrSize dependencyCount,
        TZrSize ordinal,
        TZrSize *outIndex);

/** @brief 依赖来源种类对应唯一 JSON 字段名，供清单与锁协议保持一致。 */
const TZrChar *library_project_manifest_v2_dependency_source_field(
        EZrLibrary_ProjectManifestDependencySourceKind sourceKind);

/** @brief 构造失败和项目析构共用清理，包括锁读入后由项目持有的字符串块。 */
void library_project_manifest_v2_free_declarations(SZrGlobalState *global,
                                                    SZrLibrary_Project *project);

#endif
