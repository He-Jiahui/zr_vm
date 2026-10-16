#ifndef ZR_VM_PARSER_BACKEND_AOT_FUNCTION_TABLE_H
#define ZR_VM_PARSER_BACKEND_AOT_FUNCTION_TABLE_H

#include "zr_vm_parser/writer.h"

struct SZrAotReachabilityMark;

/** @brief 原始函数与稳定扁平索引的对应项；过滤后不重新编号。 */
typedef struct SZrAotFunctionEntry {
    const SZrFunction *function;
    TZrUInt32 flatIndex;
} SZrAotFunctionEntry;

/** @brief AOT 函数图的拥有者；count 可被可达性过滤缩小，indexSpace 保留原编号域。 */
typedef struct SZrAotFunctionTable {
    SZrAotFunctionEntry *entries;
    TZrUInt32 count;
    TZrUInt32 capacity;
    TZrUInt32 indexSpace;
} SZrAotFunctionTable;

/** @brief 展平根函数、常量中的函数与子函数，按定义身份去重。
 *  @note 成功后由 release_function_table 释放 entries。 */
TZrBool backend_aot_build_function_table(SZrState *state,
                                         const SZrFunction *function,
                                         SZrAotFunctionTable *outTable);
/** @brief 释放函数表内存并重置计数。 */
void backend_aot_release_function_table(SZrState *state, SZrAotFunctionTable *table);
/** @brief 原地压缩已标记项，保留原 flatIndex 供根与边引用。 */
TZrBool backend_aot_filter_function_table_by_reachability(SZrAotFunctionTable *table,
                                                           const struct SZrAotReachabilityMark *marks,
                                                           TZrUInt32 markCount);
/** @brief 返回过滤前的编号空间；表不一致时返回零。 */
TZrUInt32 backend_aot_function_table_index_space(const SZrAotFunctionTable *table);
/** @brief 将常量中的函数值解析为扁平函数索引。
 *  @note outFunctionIndex 非空时先置为无效索引。 */
TZrBool backend_aot_resolve_callable_constant_function_index(const SZrAotFunctionTable *table,
                                                             SZrState *state,
                                                             const SZrFunction *function,
                                                             TZrInt32 constantIndex,
                                                             TZrUInt32 *outFunctionIndex);
/** @brief 根据函数定义身份查询保留的扁平索引；未找到返回无效索引。 */
TZrUInt32 backend_aot_find_function_table_index(const SZrAotFunctionTable *table, const SZrFunction *function);

#endif
