#ifndef ZR_VM_PARSER_BACKEND_AOT_C_NATIVE_IMPORTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_NATIVE_IMPORTS_H

#include "backend_aot_function_table.h"

/** @brief 递归检查整个函数树的原生导入契约，写出前拒绝损坏的表。 */
TZrBool backend_aot_c_native_import_validate_function_tree(
        const SZrFunction *function);

/** @brief 校验保留函数表并统计导入契约；溢出时失败且 outCount 归零。 */
TZrBool backend_aot_c_native_import_count(
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 *outCount);

/** @brief 写出裁剪后导入契约的函数归属与保留原因。 */
TZrBool backend_aot_c_native_import_write_reachability_manifest(
        FILE *file,
        const SZrAotFunctionTable *functionTable);

/** @brief 将已校验的原生 ABI/FFI 契约写入生成 C 的静态表。 */
void backend_aot_c_write_native_import_table(
        FILE *file,
        const SZrAotFunctionTable *functionTable);

/** @brief 为稀疏函数索引空间生成每个函数在连续导入表中的范围。 */
void backend_aot_c_write_native_import_range_table(
        FILE *file,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 functionIndexSpace);

#endif
