#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_I64_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_I64_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 识别可直接返回 i64 常量的零参数函数。 */
TZrBool backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function);
/** @brief 识别一参数 i64 恒等或单目运算函数。 */
TZrBool backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function);
/** @brief 识别二参数 i64 标量运算及特定循环函数。 */
TZrBool backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function);
/** @brief 判断二参数 thunk 是否可省略 state 参数；调用者须据此选 ABI。 */
TZrBool backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(const SZrFunction *function);
/** @brief 识别三参数 i64 标量运算函数。 */
TZrBool backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function);
/** @brief 判断三参数 thunk 是否可省略 state 参数；调用者须据此选 ABI。 */
TZrBool backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(const SZrFunction *function);
/** @brief 预先声明可由反射入口和直接调用引用的 i64 thunk。 */
void backend_aot_write_c_typed_i64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table);
/** @brief 为已识别的 i64 函数写出与声明一致的 thunk 定义。 */
void backend_aot_write_c_typed_i64_thunks(FILE *file, const SZrAotFunctionTable *table);

#endif
