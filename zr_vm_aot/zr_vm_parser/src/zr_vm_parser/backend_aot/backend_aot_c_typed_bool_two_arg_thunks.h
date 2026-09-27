#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_TWO_ARG_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_TWO_ARG_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 判断双布尔参数比较或逻辑函数能否采用原生标量 ABI。 */
TZrBool backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function);
/** @brief 声明以扁平函数索引命名的双布尔参数 thunk。 */
void backend_aot_c_write_bool_two_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex);
/** @brief 仅为已识别形状写出双布尔参数定义，并报告是否命中。 */
TZrBool backend_aot_c_try_write_bool_two_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry);

#endif
