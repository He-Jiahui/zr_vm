#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ANNOTATION_WARNINGS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ANNOTATION_WARNINGS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 统计保留函数之间调用 RequiresUnreferencedCode 目标产生的未抑制警告。 */
TZrUInt32 backend_aot_c_count_annotation_warnings(SZrState *state,
                                                  const SZrAotFunctionTable *functionTable);
/** @brief 统计调用方元数据标记抑制的同类警告，供裁剪摘要使用。 */
TZrUInt32 backend_aot_c_count_suppressed_annotation_warnings(SZrState *state,
                                                             const SZrAotFunctionTable *functionTable);
/** @brief 向生成 C 文件写出未抑制警告及源位置；调用方须提供可写文件。 */
void backend_aot_write_c_annotation_warnings(FILE *file,
                                             SZrState *state,
                                             const SZrAotFunctionTable *functionTable);

#endif
