#ifndef ZR_VM_PARSER_BACKEND_AOT_C_CALL_BINDINGS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_CALL_BINDINGS_H

#include "backend_aot_function_table.h"
#include "zr_vm_core/artifact_schema.h"

/** @brief 将调用点缓存投影为可序列化行，并解析静态目标 flat index。
 * @return 失败时输出行清零、目标置为无效索引；diagnostic 可选。
 */
ZR_PARSER_API TZrBool backend_aot_c_project_call_binding(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const SZrAotFunctionEntry *entry,
        TZrUInt32 cacheIndex,
        SZrArtifactCallBindingRow *outRow,
        TZrUInt32 *outTargetFunctionIndex,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 在发射前校验保留函数的绑定行顺序、目标与签名，并统计行数。 */
TZrBool backend_aot_c_validate_call_bindings(
        SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 *outCount);

/** @brief 写出绑定行字节和对应目标索引；count 必须来自同一函数表的校验。 */
TZrBool backend_aot_c_write_call_bindings(
        FILE *file, SZrState *state, const SZrAotFunctionTable *table, TZrUInt32 count);

/** @brief 在生成模块描述符中引用上一步发射的绑定表。 */
void backend_aot_c_write_call_binding_registration(FILE *file, TZrUInt32 count);

#endif
