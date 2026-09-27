#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_F64_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_F64_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 识别可直接返回 f64 常量的零参数函数。 */
TZrBool backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function);
/** @brief 识别一参数 f64 恒等或单目运算函数。 */
TZrBool backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function);
/** @brief 识别二参数 f64 算术函数。 */
TZrBool backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function);
/** @brief 判断二参数 f64 thunk 是否采用无 state 的原生 ABI。 */
TZrBool backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(const SZrFunction *function);
/** @brief 识别三参数 f64 算术函数。 */
TZrBool backend_aot_c_can_emit_typed_f64_three_arg_thunk(const SZrFunction *function);
/** @brief 判断三参数 f64 thunk 是否采用无 state 的原生 ABI。 */
TZrBool backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(const SZrFunction *function);
/** @brief 在调用方使用前声明所有可生成的 f64 thunk。 */
void backend_aot_write_c_typed_f64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table);
/** @brief 写出与形状识别和 ABI 判定一致的 f64 thunk 定义。 */
void backend_aot_write_c_typed_f64_thunks(FILE *file, const SZrAotFunctionTable *table);

#endif
