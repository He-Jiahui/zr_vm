#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPED_F64_THUNK_SHAPES_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPED_F64_THUNK_SHAPES_H

#include "backend_aot_internal.h"
#include "backend_aot_c_typed_f64_three_arg_shapes.h"

/** @brief 以下识别器先证实函数签名和指令形状；命中才允许在生成代码中跳过通用值路径。 */
TZrBool backend_aot_c_try_get_f64_constant_return(const SZrFunction *function, TZrFloat64 *outValue);
TZrBool backend_aot_c_try_get_f64_identity_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_f64_arg0_negate_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_f64_arg0_add_constant_return(const SZrFunction *function, TZrFloat64 *outValue);
TZrBool backend_aot_c_try_get_f64_arg0_subtract_constant_return(const SZrFunction *function, TZrFloat64 *outValue);
TZrBool backend_aot_c_try_get_f64_arg0_multiply_constant_return(const SZrFunction *function, TZrFloat64 *outValue);
TZrBool backend_aot_c_try_get_f64_arg0_divide_constant_return(const SZrFunction *function, TZrFloat64 *outValue);
TZrBool backend_aot_c_try_get_f64_arg0_modulo_constant_return(const SZrFunction *function, TZrFloat64 *outValue);
TZrBool backend_aot_c_try_get_f64_arg0_arg1_add_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_f64_arg0_arg1_subtract_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_f64_arg0_arg1_multiply_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_f64_arg0_arg1_divide_return(const SZrFunction *function);
TZrBool backend_aot_c_try_get_f64_arg0_arg1_modulo_return(const SZrFunction *function);

#endif
