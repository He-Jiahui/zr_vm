#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_U64_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_U64_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 识别可直接返回 u64 常量的零参数函数。 */
TZrBool backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function);
/** @brief 识别一参数 u64 恒等或常量运算函数。 */
TZrBool backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function);
/** @brief 识别二参数 u64 标量运算函数。 */
TZrBool backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function);
/** @brief 判断二参数 u64 thunk 是否可省略 state 参数。 */
TZrBool backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(const SZrFunction *function);
/** @brief 在反射入口及直接调用使用前声明所有可生成的 u64 thunk。 */
void backend_aot_write_c_typed_u64_thunk_forward_decls(FILE *file, const SZrAotFunctionTable *table);
/** @brief 为已识别的 u64 函数写出与声明匹配的 thunk 定义。 */
void backend_aot_write_c_typed_u64_thunks(FILE *file, const SZrAotFunctionTable *table);

#endif
