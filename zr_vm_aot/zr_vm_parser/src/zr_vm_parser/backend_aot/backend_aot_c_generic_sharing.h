#ifndef ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_SHARING_H
#define ZR_VM_PARSER_BACKEND_AOT_C_GENERIC_SHARING_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 写入生成代码所用的泛型字典入口宏。 */
void backend_aot_write_c_generic_dictionary_macros(FILE *file);

/** @brief 校验函数及内嵌子函数的共享泛型类型绑定。 */
TZrBool backend_aot_c_generic_sharing_validate_function_tree(
        const SZrFunction *function);

/** @brief 统计可共享泛型引用实例；类型 ID 顺序与后续条目输出一致。 */
TZrBool backend_aot_c_generic_sharing_count_dictionaries(
        const SZrAotFunctionTable *table,
        TZrUInt32 *outCount);

/** @brief 将已保留的共享泛型实例写入可达性清单。 */
TZrBool backend_aot_c_generic_sharing_write_reachability_manifest(
        FILE *file,
        const SZrAotFunctionTable *table);

/** @brief 输出共享泛型字典与调用入口，stripGeneratedSymbols 控制对外符号展示。 */
void backend_aot_write_c_generic_sharing_entries(FILE *file,
                                                 const SZrAotFunctionTable *table,
                                                 TZrBool stripGeneratedSymbols);

/** @brief 查找函数所绑定的共享泛型字典 ID；无匹配返回 0。 */
TZrUInt32 backend_aot_c_generic_sharing_dictionary_id_for_function(const SZrAotFunctionTable *table,
                                                                   const SZrFunction *function);

#endif
