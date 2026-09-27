#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THREE_ARG_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THREE_ARG_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 仅在三参数布尔逻辑的指令形状完整匹配时允许生成标量 thunk。 */
TZrBool backend_aot_c_can_emit_typed_bool_three_arg_thunk(const SZrFunction *function);
/** @brief 按扁平函数索引声明三参数布尔 thunk，供前置反射入口引用。 */
void backend_aot_c_write_bool_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex);
/** @brief 写出已识别的逻辑运算 thunk；返回假表示应由一般函数体路径处理。 */
TZrBool backend_aot_c_try_write_bool_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry);

#endif
