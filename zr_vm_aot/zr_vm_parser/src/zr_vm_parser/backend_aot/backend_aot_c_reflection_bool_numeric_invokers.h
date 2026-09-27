#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_BOOL_NUMERIC_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_BOOL_NUMERIC_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 生成两个 i64 参数、bool 返回值的反射快速调用入口。 */
void backend_aot_write_c_reflection_bool_i64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
/** @brief 生成两个 u64 参数、bool 返回值的反射快速调用入口。 */
void backend_aot_write_c_reflection_bool_u64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
/** @brief 生成两个 f64 参数、bool 返回值的反射快速调用入口。 */
void backend_aot_write_c_reflection_bool_f64_two_arg_invoker(FILE *file, const SZrAotFunctionTable *table);

#endif
