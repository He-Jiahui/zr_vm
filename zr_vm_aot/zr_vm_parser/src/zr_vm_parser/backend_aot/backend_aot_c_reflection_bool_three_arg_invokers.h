#ifndef ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_BOOL_THREE_ARG_INVOKERS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_REFLECTION_BOOL_THREE_ARG_INVOKERS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 为可识别的三布尔参数 thunk 生成反射分派；未命中时保留通用入口回退。 */
void backend_aot_write_c_reflection_bool_three_arg_invoker(FILE *file, const SZrAotFunctionTable *table);

#endif
