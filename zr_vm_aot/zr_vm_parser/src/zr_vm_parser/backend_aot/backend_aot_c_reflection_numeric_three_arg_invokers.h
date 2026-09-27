#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_NUMERIC_THREE_ARG_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_NUMERIC_THREE_ARG_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 生成三个 i64 参数的反射快速调用入口，按 thunk ABI 决定是否传入 state。 */
void backend_aot_write_c_reflection_i64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
/** @brief 生成三个 u64 参数的反射快速调用入口，按 thunk ABI 决定是否传入 state。 */
void backend_aot_write_c_reflection_u64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);
/** @brief 生成三个 f64 参数的反射快速调用入口，按 thunk ABI 决定是否传入 state。 */
void backend_aot_write_c_reflection_f64_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);

#endif
