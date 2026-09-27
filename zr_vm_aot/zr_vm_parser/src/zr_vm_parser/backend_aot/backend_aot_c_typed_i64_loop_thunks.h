#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_I64_LOOP_THUNKS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_I64_LOOP_THUNKS_H

#include "backend_aot_internal.h"

/** @brief 严格匹配计数求和循环的参数、常量与控制流形状。 */
TZrBool backend_aot_c_can_emit_typed_i64_counting_sum_loop_thunk(const SZrFunction *function);
/** @brief 为上述形状生成独立的原生 i64 循环 thunk。 */
void backend_aot_c_write_typed_i64_counting_sum_loop_thunk(FILE *file, TZrUInt32 flatIndex);

#endif
