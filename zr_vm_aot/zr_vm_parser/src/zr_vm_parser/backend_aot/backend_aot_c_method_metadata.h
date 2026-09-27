#ifndef ZR_VM_PARSER_BACKEND_AOT_C_METHOD_METADATA_H
#define ZR_VM_PARSER_BACKEND_AOT_C_METHOD_METADATA_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"
#include "backend_aot_c_zrp_metadata_prune.h"

/** @brief 为保留函数写出 MethodInfo、签名和 GC 根表，并返回实际写出字节数。 */
unsigned long long backend_aot_write_c_method_infos(FILE *file,
                                                    SZrState *state,
                                                    const SZrAotFunctionTable *table,
                                                    const SZrAotExecIrModule *module,
                                                    TZrUInt8 reflectionMetadataLevel);
/** @brief 用临时文件估算同一 MethodInfo 发射路径的字节数，供裁剪统计。 */
unsigned long long backend_aot_c_method_metadata_generated_bytes_referenced(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const SZrAotExecIrModule *module,
        TZrUInt8 reflectionMetadataLevel);
/** @brief 统计可解析 inline 布局中的有效 GC 字段根；用于决定帧根注册。 */
TZrUInt32 backend_aot_c_method_metadata_count_gc_roots(SZrState *state,
                                                       const SZrAotExecIrFunction *functionIr);
/** @brief 按函数索引空间发射稀疏 MethodInfo 指针表，裁剪空洞为 null。 */
void backend_aot_write_c_method_info_table(FILE *file,
                                           const SZrAotFunctionTable *table,
                                           TZrUInt32 functionIndexSpace);
/** @brief 发射重排后的方法 token 表；无导出映射的位置写零。 */
void backend_aot_write_c_method_token_table(FILE *file,
                                            const SZrAotFunctionTable *table,
                                            const SZrAotCEmbeddedZrpMetadata *embeddedZrpMetadata,
                                            TZrUInt32 functionIndexSpace);

#endif
