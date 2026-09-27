#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_BOOL_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 识别返回布尔常量的零参数函数。 */
TZrBool backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function);
/** @brief 识别布尔恒等或逻辑取反的一参数函数。 */
TZrBool backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function);
/** @brief 识别双布尔参数比较或逻辑函数。 */
TZrBool backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function);
/** @brief 识别两个 i64 参数比较并返回 bool 的函数。 */
TZrBool backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(const SZrFunction *function);
/** @brief 识别两个 u64 参数比较并返回 bool 的函数。 */
TZrBool backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function);
/** @brief 识别两个 f64 参数比较并返回 bool 的函数。 */
TZrBool backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(const SZrFunction *function);
/** @brief 在反射分派器引用 thunk 前声明所有可生成的布尔入口。 */
void backend_aot_write_c_typed_bool_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table);
/** @brief 为已识别的布尔函数写出标量 thunk，其他函数交给一般函数体生成器。 */
void backend_aot_write_c_typed_bool_thunks(FILE *file, const SZrAotFunctionTable *table);

#endif
