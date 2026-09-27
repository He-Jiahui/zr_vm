#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_U64_THREE_ARG_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_U64_THREE_ARG_THUNKS_H

#include <stdio.h>

#include "backend_aot_function_table.h"

/** @brief 识别三参数 u64 运算可否使用原生标量 thunk。 */
TZrBool backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function);
/** @brief 判断该三参数 thunk 是否无需 state。 */
TZrBool backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(const SZrFunction *function);
/** @brief 声明需要 state 的三参数 u64 thunk。 */
void backend_aot_c_write_u64_three_arg_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex);
/** @brief 声明不需要 state 的三参数 u64 thunk。 */
void backend_aot_c_write_u64_three_arg_state_free_thunk_forward_decl(FILE *file, TZrUInt32 flatIndex);
/** @brief 写出已识别形状的定义并报告是否命中。 */
TZrBool backend_aot_c_try_write_u64_three_arg_thunk_definition(FILE *file, const SZrAotFunctionEntry *entry);

#endif
