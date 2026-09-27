#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 生成按签名和函数索引选择 typed thunk 的反射入口及通用入口回退。 */
void backend_aot_write_c_reflection_invokers(FILE *file, const SZrAotFunctionTable *table);

#endif
